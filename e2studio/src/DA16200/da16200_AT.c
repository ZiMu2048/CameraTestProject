/*
 * da16200_AT.c
 *
 *  Created on: 2026年7月24日
 *      Author: lingk
 */

#include "DA16200/da16200_AT.h"
#include <string.h>

static ring_buffer_t g_da16200_rx_ring;//UART RX 中断收到的每一个字节都进入这里
static volatile bool g_da16200_tx_done = false;//等到 UART_EVENT_TX_COMPLETE 才置位
static volatile bool g_da16200_rx_overflow = false;//环形缓冲满时记录错误
static volatile fsp_err_t g_da16200_uart_error = FSP_SUCCESS;//记录 UART 错误事件

static volatile uint32_t g_da16200_rx_char_count = 0U;
static volatile uint32_t g_da16200_rx_drop_count = 0U;


/*
 * 功能：初始化 SCI6 UART、DA16200 接收环形缓冲区和驱动状态变量。
 * 调用环境：系统初始化阶段调用一次，不可在中断中调用。
 * 返回值：FSP_SUCCESS 表示初始化成功，其他值表示 UART 打开失败。
 */
fsp_err_t DA16200_UartInit(void)
{
    fsp_err_t err;

    RingBuffer_Init(&g_da16200_rx_ring);
    g_da16200_tx_done = false;
    g_da16200_rx_overflow = false;
    g_da16200_uart_error = FSP_SUCCESS;
    g_da16200_rx_drop_count = 0U;

    err = g_uart6.p_api->open(g_uart6.p_ctrl, g_uart6.p_cfg);
    if (FSP_SUCCESS != err)
    {
        g_printf("DA16200: UART open failed: %d\r\n", err);
        return err;
    }

    g_printf("DA16200: SCI opened, 115200-8-N-1\r\n");
    return FSP_SUCCESS;
}

/*
 * 功能：通过 SCI6 启动一次原始字节序列发送。
 * 注意：本函数只启动发送，不等待全部字节真正发送完成。
 * 参数：p_data 为发送缓冲区地址，length 为发送字节数。
 * 返回值：FSP_SUCCESS 表示成功启动发送。
 */
static fsp_err_t da16200_send_raw(const uint8_t * p_data, uint16_t length)
{
    fsp_err_t err;

    if ((NULL == p_data) || (0U == length))
    {
        return FSP_ERR_ASSERTION;
    }

    g_da16200_tx_done = false;

    err = g_uart6.p_api->write(g_uart6.p_ctrl, p_data, length);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    return FSP_SUCCESS;
}

/*
 * 功能：在关闭 SCI6 接收中断的临界区内清空 DA16200 接收环形缓冲区。
 * 目的：防止清空过程中 UART 接收中断同时修改环形缓冲区读写索引。
 */
static void da16200_clear_rx_ring(void)
{
    R_BSP_IrqDisable(g_uart6_cfg.rxi_irq);
    RingBuffer_Clear(&g_da16200_rx_ring);
    R_BSP_IrqEnableNoClear(g_uart6_cfg.rxi_irq);
}

/*
 * 功能：从 DA16200 接收环形缓冲区中安全读取一个字节。
 * 参数：p_byte 指向接收结果存放位置。
 * 返回值：true 表示成功读取一个字节，false 表示当前没有数据。
 */
static bool da16200_read_rx_byte(uint8_t * p_byte)
{
    bool data_available;

    R_BSP_IrqDisable(g_uart6_cfg.rxi_irq);
    data_available = RingBuffer_Read(&g_da16200_rx_ring, p_byte);
    R_BSP_IrqEnableNoClear(g_uart6_cfg.rxi_irq);

    return data_available;
}

/*
 * 功能：检查响应字符串中是否存在独立且完整的终止行。
 * 参数：p_response 为完整响应字符串，p_terminal 为待查找的终止内容。
 * 返回值：true 表示找到独立终止行，否则返回 false。
 */
static bool da16200_response_has_terminal_line(const char * p_response,
                                               const char * p_terminal)
{
    const char * p_match = p_response;
    size_t terminal_length = strlen(p_terminal);

    while (NULL != (p_match = strstr(p_match, p_terminal)))
    {
        bool const line_start = (p_match == p_response) || ('\n' == p_match[-1]);
        char const following = p_match[terminal_length];
        bool const line_end = ('\0' == following) || ('\r' == following) || ('\n' == following);

        if (line_start && line_end)
        {
            return true;
        }

        p_match++;
    }

    return false;
}

/*
 * 功能：检查响应字符串中是否已经收到完整的 ERROR 行。
 * 返回值：true 表示收到完整错误响应，否则返回 false。
 */
static bool da16200_response_has_complete_error_line(const char * p_response)
{
    const char * p_match = p_response;

    while (NULL != (p_match = strstr(p_match, "ERROR")))
    {
        bool const line_start = (p_match == p_response) || ('\n' == p_match[-1]);

        if (line_start && (NULL != strchr(p_match, '\n')))
        {
            return true;
        }

        p_match++;
    }

    return false;
}

/*
 * 功能：发送一条 AT 指令，并阻塞等待完整的 OK 或 ERROR 响应。
 * 参数：p_command 为以回车换行结束的 AT 指令。
 * 参数：p_response 为调用者提供的响应缓冲区。
 * 参数：response_size 为响应缓冲区容量。
 * 参数：timeout_ms 为发送和接收阶段的超时时间。
 * 注意：本函数会清空旧接收数据，不可在中断中调用，也不是线程安全函数。
 * 返回值：FSP_SUCCESS 表示收到 OK，其他值表示参数、超时、溢出或模块错误。
 */
fsp_err_t DA16200_SendCommandAndGetResponse(const char * p_command,
                                            char * p_response,
                                            uint16_t response_size,
                                            uint32_t timeout_ms)
{
    uint8_t  received_byte;
    uint16_t response_length = 0U;
    uint32_t elapsed_ms = 0U;
    fsp_err_t err;

    if ((NULL == p_command) || (NULL == p_response) ||
        ('\0' == p_command[0]) || (response_size < 2U))
    {
        return FSP_ERR_ASSERTION;
    }

    p_response[0] = '\0';
    da16200_clear_rx_ring();
    g_da16200_rx_overflow = false;
    g_da16200_uart_error = FSP_SUCCESS;
    g_da16200_tx_done = false;
    g_da16200_rx_char_count = 0U;
    g_da16200_rx_drop_count = 0U;

    g_printf("TX: %s", p_command);
    err = da16200_send_raw((const uint8_t *) p_command, (uint16_t) strlen(p_command));
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    while (!g_da16200_tx_done)
    {
        if (FSP_SUCCESS != g_da16200_uart_error)
        {
            return g_da16200_uart_error;
        }

        if (elapsed_ms >= timeout_ms)
        {
            return FSP_ERR_TIMEOUT;
        }

        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
        elapsed_ms++;
    }

    elapsed_ms = 0U;
    while (elapsed_ms < timeout_ms)
    {
        if (FSP_SUCCESS != g_da16200_uart_error)
        {
            return g_da16200_uart_error;
        }

        if (g_da16200_rx_overflow)
        {
            g_printf("RX RING OVERFLOW: dropped=%lu, received=%lu\r\n",
                     (unsigned long) g_da16200_rx_drop_count,
                     (unsigned long) g_da16200_rx_char_count);
            return FSP_ERR_RXBUF_OVERFLOW;
        }

        if (da16200_read_rx_byte(&received_byte))
        {

            if (response_length >= (response_size - 1U))
            {
                g_printf("RX BUFFER OVERFLOW: %s\r\n", p_response);
                return FSP_ERR_RXBUF_OVERFLOW;
            }

            p_response[response_length++] = (char) received_byte;
            p_response[response_length] = '\0';

            if (da16200_response_has_terminal_line(p_response, "OK"))
            {
                g_printf("RX: %s\r\n", p_response);
                return FSP_SUCCESS;
            }

            if (da16200_response_has_complete_error_line(p_response))
            {
                g_printf("RX ERROR: %s\r\n", p_response);
                return FSP_ERR_ASSERTION;
            }
        }
        else
        {
            R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
            elapsed_ms++;
        }
    }

    g_printf("RX TIMEOUT: %s [chars=%lu]\r\n",
             p_response,
             (unsigned long) g_da16200_rx_char_count);
    return FSP_ERR_TIMEOUT;
}

/*
 * 功能：发送一条 AT 指令，并检查响应中是否包含指定关键字。
 * 参数：p_expected 为期望出现在响应中的字符串。
 * 返回值：收到完整响应且找到关键字时返回 FSP_SUCCESS。
 */
fsp_err_t DA16200_SendCommandAndWait(const char * p_command,
                                     const char * p_expected,
                                     uint32_t timeout_ms)
{
    char response[DA16200_STR_LEN_512] = {0};
    fsp_err_t err;

    if ((NULL == p_expected) || ('\0' == p_expected[0]))
    {
        return FSP_ERR_ASSERTION;
    }

    err = DA16200_SendCommandAndGetResponse(p_command, response, sizeof(response), timeout_ms);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    return (NULL != strstr(response, p_expected)) ? FSP_SUCCESS : FSP_ERR_ASSERTION;
}

/*
 * 功能：查询 E103-W12 当前的 Wi-Fi 工作模式。
 * 参数：p_mode 用于返回 Station、SoftAP 或 Station+SoftAP 模式。
 * 返回值：FSP_SUCCESS 表示查询和解析成功。
 */
fsp_err_t DA16200_QueryWifiMode(da16200_wifi_mode_t * p_mode)
{
    char response[DA16200_STR_LEN_512] = {0};
    const char * p_mode_text;
    fsp_err_t err;

    if (NULL == p_mode)
    {
        return FSP_ERR_ASSERTION;
    }

    err = DA16200_SendCommandAndGetResponse("AT+CWMODE=?\r\n",
                                             response,
                                             sizeof(response),
                                             5000U);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    p_mode_text = strstr(response, "+CWMODE:");
    if ((NULL == p_mode_text) ||
        ((p_mode_text[8] < '0') || (p_mode_text[8] > '2')))
    {
        return FSP_ERR_ASSERTION;
    }

    *p_mode = (da16200_wifi_mode_t) (p_mode_text[8] - '0');
    return FSP_SUCCESS;
}

/*
 * 功能：设置 E103-W12 的 Wi-Fi 工作模式。
 * 注意：模式写入 NVRAM 后需要重启模块才能生效。
 * 返回值：FSP_SUCCESS 表示模块接受了设置指令。
 */
fsp_err_t DA16200_SetWifiMode(da16200_wifi_mode_t mode)
{
    const char * p_command;

    switch (mode)
    {
        case DA16200_WIFI_MODE_STA:
        {
            p_command = "AT+CWMODE=0\r\n";
            break;
        }

        case DA16200_WIFI_MODE_SOFT_AP:
        {
            p_command = "AT+CWMODE=1\r\n";
            break;
        }

        case DA16200_WIFI_MODE_STA_AP:
        {
            p_command = "AT+CWMODE=2\r\n";
            break;
        }

        default:
        {
            return FSP_ERR_ASSERTION;
        }
    }

    return DA16200_SendCommandAndWait(p_command,
                                      "OK",
                                      5000U);
}

/*
 * 功能：发送复位指令，等待模块重新启动，再通过 AT 指令确认通信恢复。
 * 注意：本函数包含毫秒级软件延时，是阻塞函数，不可在中断中调用。
 */
fsp_err_t DA16200_ResetAndWaitReady(void)
{
    fsp_err_t err;
    err = DA16200_SendCommandAndWait("AT+RST\r\n", "OK", 5000U);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    R_BSP_SoftwareDelay(3000U, BSP_DELAY_UNITS_MILLISECONDS);

    return DA16200_SendCommandAndWait("AT\r\n", "OK", 5000U);
}

/*
 * 功能：配置 E103-W12 SoftAP 的 SSID、密码、信道和国家代码。
 * 参数：p_cfg 指向 SoftAP 配置结构体。
 * 注意：配置内容会写入模块 NVRAM。
 * 返回值：FSP_SUCCESS 表示配置成功。
 */
fsp_err_t DA16200_ConfigSoftAp(const da16200_softap_cfg_t * p_cfg)
{
    //fsp_err_t err;
    char command[DA16200_STR_LEN_256] = {0};
    size_t ssid_length;
    size_t password_length;
    int command_length;

    if ((NULL == p_cfg) ||
        (NULL == p_cfg->p_ssid) ||
        (NULL == p_cfg->p_password) ||
        (NULL == p_cfg->p_country))
    {
        return FSP_ERR_ASSERTION;
    }

    ssid_length = strlen(p_cfg->p_ssid);
    password_length = strlen(p_cfg->p_password);

    if ((0U == ssid_length) || (ssid_length > 32U) ||
        (password_length < 8U) || (password_length > 63U) ||
        (strlen(p_cfg->p_country) != 2U))
    {
        return FSP_ERR_ASSERTION;
    }

    if ((NULL != strchr(p_cfg->p_ssid, ',')) ||
        (NULL != strchr(p_cfg->p_ssid, '\'')) ||
        (NULL != strchr(p_cfg->p_password, ',')) ||
        (NULL != strchr(p_cfg->p_password, '\'')))
    {
        return FSP_ERR_ASSERTION;
    }

    command_length = snprintf(command,
                              sizeof(command),
                              "AT+CWSAP=%s,3,1,%s,%u,%s\r\n",
                              p_cfg->p_ssid,
                              p_cfg->p_password,
                              (unsigned int) p_cfg->channel,
                              p_cfg->p_country);

    if ((command_length < 0) || ((size_t) command_length >= sizeof(command)))
    {
        return FSP_ERR_ASSERTION;
    }

    return DA16200_SendCommandAndWait(command, "+CWSAP", 5000U);
}

/*
 * 功能：根据已经保存的 SoftAP 配置启动无线热点。
 * 返回值：FSP_SUCCESS 表示模块返回 OK。
 */
fsp_err_t DA16200_StartSoftAp(void)
{
    return DA16200_SendCommandAndWait("AT+CWOAP\r\n", "OK", 5000U);
}

/*
 * 功能：非阻塞读取 DA16200 接收环形缓冲区中的异步数据。
 * 参数：p_data 为输出缓冲区，capacity 为最多允许读取的字节数。
 * 返回值：实际读取的字节数，返回 0 表示当前没有数据。
 */
size_t DA16200_ReadAsync(uint8_t * p_data, size_t capacity)
{
    size_t length = 0U;

    if ((NULL == p_data) || (0U == capacity))
    {
        return 0U;
    }

    while ((length < capacity) &&
           da16200_read_rx_byte(&p_data[length]))
    {
        length++;
    }

    return length;
}

/*
 * 功能：阻塞等待 SCI6 产生 UART_EVENT_TX_COMPLETE 发送完成事件。
 * 参数：timeout_ms 为最大等待时间。
 * 返回值：FSP_SUCCESS 表示发送完成，其他值表示超时或 UART 错误。
 */
static fsp_err_t da16200_wait_tx_complete(uint32_t timeout_ms)
{
    uint32_t elapsed_ms = 0U;

    while (!g_da16200_tx_done)
    {
        if (FSP_SUCCESS != g_da16200_uart_error)
        {
            return g_da16200_uart_error;
        }

        if (elapsed_ms >= timeout_ms)
        {
            return FSP_ERR_TIMEOUT;
        }

        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
        elapsed_ms++;
    }

    return FSP_SUCCESS;
}

/*
 * 功能：通过 E103-W12 的 TCP Server 会话向指定远端客户端发送短文本。
 * 参数：p_remote_ip 和 remote_port 指定已连接客户端的地址与端口。
 * 参数：p_text 为不包含逗号、回车和换行的短文本。
 * 注意：本函数使用 CID 0，只适用于模块作为 TCP Server 的场景。
 */
fsp_err_t DA16200_TcpServerSendText(const char * p_remote_ip,
                                    uint16_t remote_port,
                                    const char * p_text)
{
    char command[DA16200_STR_LEN_256] = {0};
    size_t text_length;
    int command_length;
    fsp_err_t err;

    if ((NULL == p_remote_ip) || (NULL == p_text) ||
        ('\0' == p_remote_ip[0]) || ('\0' == p_text[0]) ||
        (0U == remote_port))
    {
        return FSP_ERR_ASSERTION;
    }

    text_length = strlen(p_text);

    if ((text_length > 128U) ||
        (NULL != strchr(p_text, ',')) ||
        (NULL != strchr(p_text, '\r')) ||
        (NULL != strchr(p_text, '\n')))
    {
        return FSP_ERR_ASSERTION;
    }

    command_length = snprintf(command,
                              sizeof(command),
                              "AT+CIPSEND=0,%u,%s,%u,%s\r\n",
                              (unsigned int) text_length,
                              p_remote_ip,
                              (unsigned int) remote_port,
                              p_text);

    if ((command_length < 0) || ((size_t) command_length >= sizeof(command)))
    {
        return FSP_ERR_ASSERTION;
    }

    g_printf("TCP TX to %s:%u: %s\r\n",
             p_remote_ip,
             (unsigned int) remote_port,
             p_text);

    err = da16200_send_raw((const uint8_t *) command,
                           (uint16_t) command_length);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    return da16200_wait_tx_complete(1000U);
}        


fsp_err_t DA16200_ConnectWifi(const char * p_ssid,
                              const char * p_password,
                              uint32_t timeout_ms)
{
    char command[DA16200_STR_LEN_256] = {0};
    char response[DA16200_STR_LEN_512] = {0};
    const char * p_result;
    size_t ssid_length;
    size_t password_length;
    uint16_t response_length = 0U;
    uint32_t elapsed_ms = 0U;
    uint8_t received_byte;
    int command_length;
    fsp_err_t err;

    if(NULL == p_ssid || NULL == p_password || timeout_ms == 0U)
    {
        return FSP_ERR_ASSERTION;
    }

/*固件只能处理WPA/WPA2热点
*SSID长度32字节，密码8~63字节
*/
    ssid_length = strlen(p_ssid);
    password_length = strlen(p_password);
    if ((0U == ssid_length) ||
        (ssid_length > 32U) ||
        (password_length < 8U) ||
        (password_length > 63U))
    {
        return FSP_ERR_ASSERTION;
    }

/*
* 逗号是 AT 指令参数分隔符。
* 回车和换行会提前终止 AT 指令。
*/
    if ((NULL != strchr(p_ssid, ',')) ||
        (NULL != strchr(p_ssid, '\r')) ||
        (NULL != strchr(p_ssid, '\n')) ||
        (NULL != strchr(p_password, ',')) ||
        (NULL != strchr(p_password, '\r')) ||
        (NULL != strchr(p_password, '\n')))
    {
        return FSP_ERR_ASSERTION;
    }

    command_length = snprintf(command,
                              sizeof(command),
                              "AT+CWJAPA=%s,%s\r\n",
                              p_ssid,
                              p_password);    

if ((command_length < 0) ||
        ((size_t) command_length >= sizeof(command)))
    {
        return FSP_ERR_ASSERTION;
    }

    /*
     * 清除上一条命令留下的接收数据和状态。
     */
    da16200_clear_rx_ring();
    g_da16200_rx_overflow = false;
    g_da16200_uart_error = FSP_SUCCESS;
    g_da16200_tx_done = false;
    g_da16200_rx_char_count = 0U;
    g_da16200_rx_drop_count = 0U;

    /*
     * 日志只显示 SSID
     */
    g_printf("TX: AT+CWJAPA=%s,<password hidden>\r\n", p_ssid);

    err = da16200_send_raw((const uint8_t *) command,
                           (uint16_t) command_length);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    err = da16200_wait_tx_complete(1000U);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    /*
     * 继续读取异步结果。
     * 最初收到的 OK 只表示命令已被接受。
     */
    while (elapsed_ms < timeout_ms)
    {
        if (FSP_SUCCESS != g_da16200_uart_error)
        {
            return g_da16200_uart_error;
        }

        if (g_da16200_rx_overflow)
        {
            return FSP_ERR_RXBUF_OVERFLOW;
        }

        if (da16200_read_rx_byte(&received_byte))
        {
            if (response_length >= (sizeof(response) - 1U))
            {
                return FSP_ERR_RXBUF_OVERFLOW;
            }

            response[response_length++] = (char) received_byte;
            response[response_length] = '\0';

            /*
             * 等待成功结果所在行完整接收。
             */
            p_result = strstr(response, "+CWJAP:1");
            if ((NULL != p_result) &&
                (NULL != strchr(p_result, '\n')))
            {
                g_printf("DA16200: Wi-Fi connected\r\n");
                return FSP_SUCCESS;
            }

            /*
             * +CWJAP:0 表示连接过程失败。
             */
            p_result = strstr(response, "+CWJAP:0");
            if ((NULL != p_result) &&
                (NULL != strchr(p_result, '\n')))
            {
                g_printf("DA16200: Wi-Fi connection failed\r\n");
                return FSP_ERR_ASSERTION;
            }

            /*
             * ERROR 表示命令格式、热点或认证等阶段出错。
             */
            if (da16200_response_has_complete_error_line(response))
            {
                g_printf("DA16200: Wi-Fi command rejected\r\n");
                return FSP_ERR_ASSERTION;
            }
        }
        else
        {
            R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
            elapsed_ms++;
        }
    }

    g_printf("DA16200: Wi-Fi connection timeout\r\n");
    return FSP_ERR_TIMEOUT;
}

/*
 * 功能：查询 Station 接口是否已经连接 Wi-Fi。
 * 参数：p_connected 用于返回连接状态。
 * 返回值：查询和解析成功时返回 FSP_SUCCESS。
 */
fsp_err_t DA16200_QueryStaConnected(bool * p_connected)
{
    char response[DA16200_STR_LEN_128] = {0};
    const char * p_status;
    fsp_err_t err;

    if(NULL == p_connected)
    {
        return FSP_ERR_ASSERTION;
    }

    *p_connected = false;

    err = DA16200_SendCommandAndGetResponse("AT+CWSTA\r\n",
                                            response,
                                            (uint16_t) sizeof(response),
                                            5000U);

    if (FSP_SUCCESS != err)
    {
        return err;
    }

    p_status = strstr(response, "+CWSTA:");
    if ((NULL == p_status) ||
        (('0' != p_status[7]) && ('1' != p_status[7])))
    {
        return FSP_ERR_ASSERTION;
    }

    *p_connected = ('1' == p_status[7]);
    return FSP_SUCCESS;
}

/*
 * 功能：确保 DA16200 最终处于 Wi-Fi 已连接状态。
 * 参数：p_ssid 为热点名称，p_password 为密码。
 * 参数：timeout_ms 为 DA16200_ConnectWifi 的最大等待时间。
 * 返回值：确认 CWSTA=1 时返回 FSP_SUCCESS。
 */
fsp_err_t DA16200_EnsureWifiConnected(const char * p_ssid,
                                      const char * p_password,
                                      uint32_t timeout_ms)
{
    bool connected =  false;
    fsp_err_t err;

    // 查询当前连接状态
    err = DA16200_QueryStaConnected(&connected);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    if(connected)
    {
        return FSP_SUCCESS;
    }
    
    // 尝试连接 Wi-Fi
    err = DA16200_ConnectWifi(p_ssid, p_password, timeout_ms);
    if (FSP_SUCCESS != err)
    {
        g_printf("DA16200: Wi-Fi join failed, err=%d\r\n", err);
        return err;
    }

    // 等待 CWSTA=1，最多尝试 5 次，每次间隔 500ms
    for (uint32_t attempt = 0U; attempt < 5U; attempt++)
    {
        err = DA16200_QueryStaConnected(&connected);
        
        // 查询失败或尚未连接时，延时后继续重试
        if ((FSP_SUCCESS == err) && connected)
        {
            g_printf("DA16200: Wi-Fi connection verified\r\n");
            return FSP_SUCCESS;
        }

        R_BSP_SoftwareDelay(500U,
                            BSP_DELAY_UNITS_MILLISECONDS);
    }

    // 查询超时
    g_printf("DA16200: CWSTA verification timeout\r\n");
    return FSP_ERR_TIMEOUT;
}

/*
 * 功能：从 TCP 客户端建立连接的响应中解析 CID。
 */
static bool da16200_parse_tcp_client_cid(const char * p_response,
                                         uint8_t * p_cid)
{
    const char * p_value;
    if ((NULL == p_response) || (NULL == p_cid))
    {
        return false;
    }

    p_value = strstr(p_response, "+TRTC:");
    if(NULL != p_value)
    {
         p_value += strlen("+TRTC:");
    }
    else
    {
        p_value = strstr(p_response, "+CIPSTART:");
        if (NULL == p_value)
        {
            return false;
        }

        p_value += strlen("+CIPSTART:");
    }

    /*
     * DA16200 最多支持八个套接字，因此有效 CID 为 0～7。
     */
    if ((p_value[0] < '0') || (p_value[0] > '7'))
    {
        return false;
    }

    *p_cid = (uint8_t) (p_value[0] - '0');
    return true;
}

/*
 * 功能：连接局域网内的 TCP 服务端，并返回模块分配的 CID。
 * 参数：p_server_ip 为电脑的局域网 IPv4 地址。
 * 参数：server_port 为电脑监听的 TCP 端口。
 * 参数：p_cid 用于返回 DA16200 分配的连接编号。
 */
fsp_err_t DA16200_TcpClientOpen(const char * p_server_ip,
                                uint16_t server_port,
                                uint8_t * p_cid)
{
    char command[DA16200_STR_LEN_128] = {0};
    char response[DA16200_STR_LEN_512] = {0};
    uint16_t response_length = 0U;
    uint32_t elapsed_ms = 0U;
    uint8_t received_byte;
    int command_length;
    fsp_err_t err;

    if ((NULL == p_server_ip) ||
        (NULL == p_cid) ||
        ('\0' == p_server_ip[0]) ||
        (0U == server_port))
    {
        return FSP_ERR_ASSERTION;
    }

    if ((NULL != strchr(p_server_ip, ',')) ||
        (NULL != strchr(p_server_ip, '\r')) ||
        (NULL != strchr(p_server_ip, '\n')))
    {
        return FSP_ERR_ASSERTION;
    }

    *p_cid = 0xFFU;

    // 生成 AT+CIPSTART=<IP>,<端口>,0
    command_length = snprintf(command,
                              sizeof(command),
                              "AT+CIPSTART=%s,%u,0\r\n",
                              p_server_ip,
                              (unsigned int) server_port);
    if ((command_length < 0) ||
        ((size_t) command_length >= sizeof(command)))
    {
        return FSP_ERR_ASSERTION;
    }

    err = DA16200_SendCommandAndGetResponse(command,
                                            response,
                                            (uint16_t) sizeof(response),
                                            10000U);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    if (da16200_parse_tcp_client_cid(response, p_cid))
    {
        g_printf("DA16200: TCP client connected, CID=%u\r\n",
                (unsigned int) *p_cid);
        return FSP_SUCCESS;
    }

    memset(response, 0, sizeof(response));
    response_length = 0U;

    while (elapsed_ms < 5000U)
    {
        if (FSP_SUCCESS != g_da16200_uart_error)
        {
            return g_da16200_uart_error;
        }

        if (g_da16200_rx_overflow)
        {
            return FSP_ERR_RXBUF_OVERFLOW;
        }

        if (da16200_read_rx_byte(&received_byte))
        {
            if (response_length >= (sizeof(response) - 1U))
            {
                return FSP_ERR_RXBUF_OVERFLOW;
            }

            response[response_length++] = (char) received_byte;
            response[response_length] = '\0';

            if (da16200_parse_tcp_client_cid(response, p_cid))
            {
                g_printf("DA16200: TCP client connected, CID=%u\r\n",
                         (unsigned int) *p_cid);
                return FSP_SUCCESS;
            }
        }
        else
        {
            R_BSP_SoftwareDelay(1U,BSP_DELAY_UNITS_MILLISECONDS);
            elapsed_ms++;
        }
    }

    g_printf("DA16200: TCP client CID timeout\r\n");
    return FSP_ERR_TIMEOUT;

}

/*
 * 功能：关闭 DA16200 当前保存的全部 socket 会话。
 * 调用上下文：线程或主循环上下文，不可在中断中调用。
 * 返回值：FSP_SUCCESS 表示模块返回 OK。
 */
fsp_err_t DA16200_TcpCloseAll(void)
{
    char response[DA16200_STR_LEN_128] = {0};
    fsp_err_t err;

    err = DA16200_SendCommandAndGetResponse("AT+CIPCLOSEALL\r\n",
                                            response,
                                            (uint16_t) sizeof(response),
                                            5000U);
    if (FSP_SUCCESS != err)
    {
        g_printf("DA16200: close all sockets failed, err=%d\r\n",
                 (int) err);
        return err;
    }

    g_printf("DA16200: all sockets closed\r\n");
    return FSP_SUCCESS;
}
/*
 * 功能：通过已经建立的 TCP Client 会话发送短文本。
 * 参数：cid 为 DA16200_TcpClientOpen 返回的连接编号。
 * 参数：p_text 为不包含逗号、回车和换行的短文本。
 * 调用环境：线程或主循环上下文，不可在中断中调用。
 * 限制：当前版本只适用于短文本测试，不适用于二进制图像。
 */
fsp_err_t DA16200_TcpClientSendText(uint8_t cid,
                                    const char * p_text)
{
    char command[DA16200_STR_LEN_256] = {0};
    size_t text_length;
    int command_length;
    fsp_err_t err;

    if ((cid > 7U) ||
        (NULL == p_text) ||
        ('\0' == p_text[0]))
    {
        return FSP_ERR_ASSERTION;
    }

    text_length = strlen(p_text);
    if ((text_length > 128U) ||
        (NULL != strchr(p_text, ',')) ||
        (NULL != strchr(p_text, '\r')) ||
        (NULL != strchr(p_text, '\n')))
    {
        return FSP_ERR_ASSERTION;
    }

    command_length = snprintf(command,
                              sizeof(command),
                              "AT+CIPSEND=%u,%u,0,0,%s\r\n",
                              (unsigned int) cid,
                              (unsigned int) text_length,
                              p_text);
    if ((command_length < 0) ||
        ((size_t) command_length >= sizeof(command)))
    {
        return FSP_ERR_ASSERTION;
    }

    err = da16200_send_raw((const uint8_t *) command,
                           (uint16_t) command_length);   
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    return da16200_wait_tx_complete(1000U);
}


/*
 * 功能：处理 SCI6 UART 中断事件。
 * 接收事件：把接收到的字节写入环形缓冲区。
 * 发送完成事件：设置发送完成标志。
 * 错误事件：记录奇偶校验、帧或接收溢出错误。
 * 调用环境：由 FSP UART 驱动在中断上下文中调用。
 */
void UART6_CallBack(uart_callback_args_t *p_args)
{
    if (NULL == p_args)
        {
            return;
        }

    switch (p_args->event)
    {
        case UART_EVENT_RX_CHAR:
        {
            g_da16200_rx_char_count++;
            if (!RingBuffer_Write(&g_da16200_rx_ring, (uint8_t) p_args->data))
            {
                g_da16200_rx_overflow = true;
                g_da16200_rx_drop_count++;
            }
            break;
        }

        case UART_EVENT_TX_COMPLETE:
        {
            g_da16200_tx_done = true;
            break;
        }

        case UART_EVENT_ERR_PARITY:
        case UART_EVENT_ERR_FRAMING:
        case UART_EVENT_ERR_OVERFLOW:
        {
            g_da16200_uart_error = FSP_ERR_ASSERTION;
            break;
        }

        default:
        {
            break;
        }
    }
}

