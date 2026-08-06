# RA8P1 摄像头、AI、显示与 DA16200 阻塞式上传链路说明

> 工作标注：摄像头捕获,AI判断,云端上传,本地显示阻塞式链路验证成功喵

## 1. 验证目标

本阶段在 Renesas RA8P1 Cortex-M85 上完成了以下最小闭环：

1. MIPI-CSI 摄像头采集图像。
2. VIN 将图像写入三缓冲 RGB565 帧缓冲区。
3. CPU 将完整帧复制到 GLCDC 后缓冲区。
4. 图像预处理后交给 Ethos-U55/TFLM 模型执行目标检测。
5. D/AVE 2D 在本地显示缓冲区叠加检测框和标签。
6. 当最高检测置信度大于 `0.5` 时，将干净原图裁剪、缩放并编码为 JPEG。
7. RA8P1 通过 SCI6 控制 DA16200，把图像协议头和 JPEG 数据分块发送到电脑端 TCP 服务。
8. Python 接收端校验 JPEG，保存到 `ERROR` 文件夹，并由网页实时显示图像、置信度和接收历史。

当前实现采用阻塞式 JPEG 编码和阻塞式 AT/TCP 发送，目标是验证端到端数据链路正确性，不代表最终实时架构。

## 2. 总体数据流

```text
MIPI 摄像头
    │ YCbCr422 / MIPI-CSI
    ▼
VIN 三缓冲区（1024×600 RGB565，stride=1024 pixels）
    │ D-Cache invalidate
    ▼
GLCDC 后缓冲区稳定副本
    ├──► 600×600 中央裁剪 ─► 256×256 模型输入 ─► AI 推理/NMS
    │                                                │
    │                                                ├──► D/AVE 2D 检测框 ─► GLCDC
    │                                                │
    │                                                └──► score > 0.5
    │                                                       │
    └──► 600×600 中央裁剪 ─► 240×240 RGB888 ─► JPEG         │
                                                            ▼
                                                DA16200 TCP 分块上传
                                                            │
                                                            ▼
                                              Python 接收端 / ERROR / 网页
```

## 3. 图像与内存约定

| 项目 | 当前配置 |
|---|---:|
| VIN 输出尺寸 | 1024×600 |
| VIN 输出格式 | RGB565 |
| 每行步长 | 1024 pixels / 2048 bytes |
| 单帧大小 | 1,228,800 bytes |
| AI 裁剪区域 | `(x=212, y=0, w=600, h=600)` |
| JPEG 输出尺寸 | 240×240 RGB888 |
| JPEG 质量 | 60 |
| JPEG 最大容量 | 64 KiB |
| TCP 数据块 | 最大 1024 bytes |

VIN 是 DMA 数据生产者，Cortex-M85 D-Cache 中可能保留同一 SDRAM 地址的旧副本。因此 CPU 读取 VIN 完成帧前必须执行 D-Cache invalidate。初始化缓冲区并交给 VIN 前使用 clean+invalidate，避免 CPU 的初始化写回覆盖 DMA 新数据。

JPEG 编码不直接读取正在轮转的 VIN 缓冲区，而是读取已经复制完成的 GLCDC 后缓冲区。该副本在编码和发送期间不会被 VIN 改写，从而避免 JPEG 出现跨帧撕裂。

## 4. DA16200 对外 API

接口位于：

- `src/DA16200/da16200_AT.h`
- `src/DA16200/da16200_AT.c`

### 4.1 `DA16200_Open`

```c
fsp_err_t DA16200_Open(void);
```

用途：打开 FSP 生成的 SCI6 实例，并初始化 DA16200 UART 收发状态。

调用约束：

- 系统启动阶段调用一次。
- 当前驱动不是线程安全实现。
- 不可在中断上下文调用。
- 当前串口参数为 `115200-8-N-1`。

### 4.2 `DA16200_SendCommand`

```c
fsp_err_t DA16200_SendCommand(
    const char * p_command,
    char * p_response,
    size_t response_capacity,
    uint32_t timeout_ms);
```

用途：发送普通 AT 命令，等待最终 `OK`、`ERROR` 或超时，并将文本响应保存到调用者缓冲区。

注意事项：

- `p_command` 不应包含用户密码日志输出逻辑。
- 响应缓冲区必须留出结尾 `\0` 空间。
- 超时期间函数阻塞当前执行流。
- 不应在 ISR 中调用。

典型用途包括固件版本查询、Station 状态查询、Wi-Fi 连接和 TCP 会话建立。

### 4.3 `DA16200_TcpClientConnect`

```c
fsp_err_t DA16200_TcpClientConnect(
    const char * p_server_ip,
    uint16_t server_port,
    uint8_t * p_cid);
```

用途：使用 `AT+CIPSTART` 建立 TCP Client，并返回 DA16200 分配的连接 ID。

成功后必须保存 `CID`，后续二进制发送使用相同会话编号。当前业务代码使用 `0..7` 作为有效 CID 范围，`0xFF` 表示没有可用连接。

### 4.4 `DA16200_TcpClientSendBinaryChunk`

```c
fsp_err_t DA16200_TcpClientSendBinaryChunk(
    uint8_t cid,
    const uint8_t * p_data,
    uint16_t data_length,
    uint32_t timeout_ms);
```

用途：通过 DA16200 的 `<ESC>H` 二进制发送协议传输一个数据块。

函数内部流程：

1. 发送二进制控制请求头。
2. 等待模块确认进入数据接收状态。
3. 原样发送 `data_length` 字节二进制载荷。
4. 等待模块确认该数据块已经提交给 TCP 会话。

注意事项：

- 二进制载荷允许包含 `0x00`，因此不能使用 `strlen()` 计算长度。
- AT 控制行使用 `CRLF` 结束。
- 当前实现为阻塞式调用。
- 一个 JPEG 会调用该接口多次，先发送 24 字节图像头，再发送若干个最大 1024 字节的数据块。

## 5. JPEG 模块对外 API

接口位于：

- `src/ImageUpload/Image_JPEG_Encoder.h`
- `src/ImageUpload/Image_JPEG_Encoder.c`

### 5.1 `ImageJpeg_ConvertRgb565ToRgb888Scalar`

用途：使用标量 C 实现裁剪、最近邻缩放和 RGB565 到 RGB888 转换，主要作为正确性参考。

RGB565 展开关系如下：

```c
red_5   = (rgb565 >> 11U) & 0x1FU;
green_6 = (rgb565 >> 5U)  & 0x3FU;
blue_5  =  rgb565         & 0x1FU;
```

位复制扩展用于映射到 8 位颜色通道：

```c
r8 = (red_5 << 3U) | (red_5 >> 2U);
g8 = (green_6 << 2U) | (green_6 >> 4U);
b8 = (blue_5 << 3U) | (blue_5 >> 2U);
```

### 5.2 `ImageJpeg_ConvertRgb565ToRgb888Helium`

用途：使用 Cortex-M85 Helium/MVE 指令并行处理 RGB565 像素，输出必须与标量参考实现逐字节一致。

调用者必须保证：

- 源地址有效。
- `source_stride_pixels` 是像素数而非字节数。
- 裁剪区域完全位于源图像内部。
- RGB888 输出缓冲区至少为 `output_width × output_height × 3` 字节。

### 5.3 `ImageJpeg_EncodeRgb565`

用途：执行 RGB565 转换并调用 `stb_image_write` 将 RGB888 工作区编码成 JPEG。

该接口由调用者提供工作区和 JPEG 输出缓冲区，适合需要自行管理内存的场景。发生输出容量不足时返回错误，不允许越界或发布半个 JPEG。

### 5.4 `ImageJpeg_EncodeAndPublishRgb565`

```c
fsp_err_t ImageJpeg_EncodeAndPublishRgb565(
    const uint16_t * p_source_rgb565,
    const image_jpeg_encode_cfg_t * p_cfg,
    size_t * p_jpeg_size);
```

用途：使用模块内部 SDRAM 工作区完成转换和编码，并在成功后发布最新 JPEG 长度。

该函数与 `ImageJpeg_GetEncodedData()` 形成生产者/消费者关系。下一次编码开始前，已发布 JPEG 数据保持有效。当前实现不支持并发编码和读取。

### 5.5 `ImageJpeg_GetEncodedData`

```c
fsp_err_t ImageJpeg_GetEncodedData(
    const uint8_t ** pp_jpeg_data,
    size_t * p_jpeg_size);
```

用途：取得最近一次完整编码成功的 JPEG 只读地址和长度。

调用者不得修改返回的内部缓冲区，也不得在下一次 JPEG 编码进行时继续发送旧指针内容。

## 6. 24 字节图像传输协议

所有多字节整数均使用网络大端序。

| 偏移 | 大小 | 字段 | 含义 |
|---:|---:|---|---|
| 0 | 4 | magic | 固定为 `RJPG` |
| 4 | 1 | version | 当前为 `1` |
| 5 | 1 | header_size | 固定为 `24` |
| 6 | 2 | flags | 当前保留为 `0` |
| 8 | 4 | frame_id | 单调递增帧编号 |
| 12 | 2 | width | JPEG 宽度 |
| 14 | 2 | height | JPEG 高度 |
| 16 | 4 | jpeg_length | JPEG 字节数 |
| 20 | 2 | confidence_milli | 置信度乘以 1000 |
| 22 | 2 | reserved | 保留为 `0` |

例如置信度 `0.735` 在线上传输为整数 `735`，电脑端显示时除以 `1000`。

## 7. AI 触发与防重复机制

模型完成推理后，代码先解码输出并执行 NMS，再遍历保留框寻找最高置信度。

触发条件为：

```c
(detection_count > 0) && (max_confidence > 0.5f)
```

为了避免同一个目标在每个推理帧重复上传，引入事件锁存：

- 第一次异常帧将 `error_event_latched` 置为 `true`，并上传一张图。
- 异常目标持续存在时不再上传。
- 连续 10 个推理帧没有异常后，锁存解除。
- 下一次异常事件可以再次上传。

锁存在阻塞发送之前置位，即使本次网络发送失败，也不会在下一帧形成高速重试风暴。

## 8. Python 接收端

入口文件为 `tools/da16200_web_receiver/app.py`。

接收端包含两个服务线程：

- TCP 服务监听 `0.0.0.0:5000`，接收 RA8P1 图像帧。
- HTTP 服务监听 `0.0.0.0:8000`，提供状态接口和监控页面。

TCP 接收流程：

1. 使用 `recv_exact()` 接收固定 24 字节协议头。
2. 校验 magic、版本、头长度和 JPEG 长度上限。
3. 按 `jpeg_length` 精确接收 JPEG。
4. 校验 JPEG 的 SOI `FF D8` 和 EOI `FF D9`。
5. 保存到 `tools/da16200_web_receiver/ERROR/`。
6. 更新线程安全的 `ReceiverState`。

网页每 500 ms 请求 `/api/status`，自动更新：

- DA16200 在线状态。
- TCP 对端地址。
- 累计字节和异常图像数量。
- 最新异常图像和置信度。
- 当前进程接收的异常图像历史。

## 9. 阻塞式链路的限制

当前链路已经验证功能正确，但存在以下工程限制：

1. 软件 JPEG 编码占用 Cortex-M85 执行时间。
2. 115200 bit/s UART 的理论有效载荷速度远低于 Wi-Fi TCP 带宽。
3. 每个 1024 字节块都包含 AT 请求和确认等待。
4. 上传期间主循环暂停，AI 推理、本地显示刷新和下一帧处理都会延后。
5. TCP 断线后目前缺少完整的自动重连状态机。
6. 事件锁存状态与网络连接状态尚未完全解耦。

因此，本阶段结论是“阻塞式链路验证成功”，而不是“实时并行链路已经完成”。

## 10. 后续非阻塞化建议

推荐按以下顺序演进：

1. 将异常帧复制到独立快照缓冲区。
2. 把 JPEG 编码拆成低优先级任务或状态机步骤。
3. 为 DA16200 建立发送队列和分块发送状态机。
4. 使用固定大小描述符管理图像所有权和生命周期。
5. 增加 TCP 断线检测、退避重连和发送失败统计。
6. 将 UART 波特率提高到模块和硬件布线可靠支持的值。
7. 分别测量采集、预处理、NPU、JPEG、UART 和 TCP 各阶段耗时。

非阻塞版本必须明确缓冲区状态，例如 `FREE → CAPTURED → ENCODING → READY_TO_SEND → SENDING → FREE`，并保证 VIN、CPU、显示控制器和发送任务不会同时覆盖同一缓冲区。

## 11. 当前验证结论

- 摄像头真实帧能够稳定采集并显示。
- AI 推理和 NMS 能产生可用置信度。
- 置信度大于 `0.5` 时能够触发一次异常图像上传。
- DA16200 能够通过 TCP 正确发送包含二进制零值的 JPEG 数据。
- 电脑端能够按协议重组、校验、归档并实时展示图像。
- 同一异常事件不会逐帧重复上传，恢复正常 10 帧后能够重新触发。

摄像头捕获,AI判断,云端上传,本地显示阻塞式链路验证成功喵
