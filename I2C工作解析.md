# I2C 工作解析 — write_reg_16bit / read_reg_16bit 完整链路

> **MCU**: RA8P1 | **I2C 外设**: IIC Ch1 | **从设备**: OV5640 (0x3C) + PI4IOE5V6408 | **FSP**: v6.4.0

---

## 1. 整体架构

```
┌──────────────────────────────────────────────────────────────┐
│                     Application Layer                        │
│                                                              │
│  camera_write_array(ov5640_mipi)     // 遍历 100+ 寄存器      │
│       │                                                      │
│       ├─ write_reg_16bit(addr, data)  // 写一条               │
│       └─ read_reg_16bit(addr, &val)   // 回读校验              │
└──────────────────┬───────────────────────────────────────────┘
                   │
┌──────────────────▼───────────────────────────────────────────┐
│                     I2C 封装层 (i2c_control.c)                │
│                                                              │
│  write_reg_16bit()   write_reg_8bit()   读写 OV5640          │
│  read_reg_16bit()    read_reg_8bit()    (16-bit 寄存器地址)    │
│                                                              │
│  write_reg_8bit()                     读写 PI4IOE5V6408      │
│  read_reg_8bit()                      (8-bit 寄存器地址)      │
└──────────────────┬───────────────────────────────────────────┘
                   │
┌──────────────────▼───────────────────────────────────────────┐
│                     FSP HAL 层 (r_iic_master)                 │
│                                                              │
│  R_IIC_MASTER_Open()     初始化 I2C Ch1                      │
│  R_IIC_MASTER_Write()    发送数据 (控制 START/STOP)            │
│  R_IIC_MASTER_Read()     接收数据                             │
│  R_IIC_MASTER_SlaveAddressSet()  切换从机地址                  │
└──────────────────┬───────────────────────────────────────────┘
                   │
┌──────────────────▼───────────────────────────────────────────┐
│                     硬件层 (IIC Peripheral)                   │
│                                                              │
│  寄存器: IIC1_ICDR (数据), IIC1_ICSR (状态), IIC1_ICCR (控制)  │
│  引脚:   P05_11 (SDA), P05_12 (SCL)                          │
│  中断:   TXI (发送), RXI (接收), TEI (发送结束), ERI (错误)    │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. write_reg_16bit() — 写 OV5640 寄存器

### 2.1 函数签名与数据包构造

```c
fsp_err_t write_reg_16bit(uint16_t address, uint8_t data);
```

| 参数 | 含义 | 示例值 |
|------|------|--------|
| `address` | OV5640 寄存器地址 (16-bit) | `0x3008` (SYS_CTRL0) |
| `data` | 写入的 8-bit 数据 | `0x42` (SW_PWDN) |

**I2C 数据包结构** (3 字节)：

```
┌──────────────┬──────────────┬──────────────┐
│  i2c_buf[0]  │  i2c_buf[1]  │  i2c_buf[2]  │
│  Addr[15:8]  │  Addr[7:0]   │  Data[7:0]   │
├──────────────┼──────────────┼──────────────┤
│    0x30      │    0x08      │    0x42      │
│  寄存器地址高  │  寄存器地址低  │  写入的数据    │
└──────────────┴──────────────┴──────────────┘
```

### 2.2 逐行拆解

```c
fsp_err_t write_reg_16bit(uint16_t address, uint8_t data)
{
    // ── Step 1: 声明局部变量 ──
    fsp_err_t err = FSP_SUCCESS;
    uint8_t i2c_buffer[3];

    // ── Step 2: 将 16-bit 地址拆成高/低字节，拼入发送缓冲 ──
    i2c_buffer[0] = (uint8_t)((address >> 8) & 0xFF);  // 高字节 → 先发
    i2c_buffer[1] = (uint8_t)(address & 0xFF);         // 低字节 → 后发
    i2c_buffer[2] = data;                              // 数据  → 最后

    // ── Step 3: 复位事件标志 ──
    g_i2c_event_for_peripheral = (i2c_master_event_t)RESET_VALUE;

    // ── Step 4: 调用 HAL 发送 ──
    // 参数: 控制块指针, 数据缓冲, 长度=3, restart=false(发送完产生STOP)
    err = R_IIC_MASTER_Write(&g_i2c_master_for_peripheral_ctrl,
                              i2c_buffer, 3, false);
    if (err != FSP_SUCCESS) return err;

    // ── Step 5: 阻塞等待 I2C 发送完成 ──
    err = i2c_master_wait_event(I2C_MASTER_EVENT_TX_COMPLETE);
    return err;
}
```

### 2.3 HAL 内部发生了什么

```
R_IIC_MASTER_Write(ctrl, buf, 3, restart=false)
│
├─ 将 3 字节写入 IIC 发送 FIFO (IIC1_ICDRT)
├─ 写 IIC 控制寄存器 → 产生 START 条件
├─ 发送从机地址字节: (0x3C << 1) | 0 = 0x78  (W)
│    └─ 等待从机 ACK (SDA 被拉低)
├─ 发送 buf[0] = 0x30 → 等待 ACK
├─ 发送 buf[1] = 0x08 → 等待 ACK
├─ 发送 buf[2] = 0x42 → 等待 ACK
├─ (restart=false) → 产生 STOP 条件
├─ 触发 TXI (Transmit End Interrupt)
└─ IIC ISR → g_i2c_master_for_peripheral_callback()
       └─ g_i2c_event_for_peripheral = I2C_MASTER_EVENT_TX_COMPLETE
```

### 2.4 I2C 总线波形

```
        ┌─ Addr ─┐ ┌ AddrH ┐ ┌ AddrL ┐ ┌ Data ─┐
SDA ────╲         ╲─────────╲─────────╲─────────╲──────
         ╲  0x78   ╲  0x30   ╲  0x08   ╲  0x42   ╲
SCL ─────┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└┘└────
       S │A│ │A│  │A│ │A│  │A│ │A│  │A│ │A│  P
       T │C│ │C│  │C│ │C│  │C│ │C│  │C│ │C│  S
       A │K│ │K│  │K│ │K│  │K│ │K│  │K│ │K│  T
       R │ │ │ │  │ │ │ │  │ │ │ │  │ │ │ │  O
       T │ │ │ │  │ │ │ │  │ │ │ │  │ │ │ │  P

   字节时序:
   ┌───────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬───────┐
   │ START │ 0x78 │ ACK  │ 0x30 │ ACK  │ 0x08 │ ACK  │ 0x42 │ ACK   │STOP│
   │       │ W=0  │      │      │      │      │      │      │       │    │
   └───────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴───────┴────┘

   传输耗时 ≈ (1 + 8 + 1) × 5 字节 × 1/393kHz ≈ 45 × 2.54μs ≈ 115μs
```

### 2.5 阻塞等待机制

```c
static fsp_err_t i2c_master_wait_event(const i2c_master_event_t i2c_event)
{
    fsp_err_t err  = FSP_SUCCESS;
    uint8_t time_out = UINT8_MAX;  // = 255

    while (i2c_event != g_i2c_event_for_peripheral)  // 轮询事件标志
    {
        // 检查是否发生了 Abort
        if (I2C_MASTER_EVENT_ABORTED == g_i2c_event_for_peripheral)
        {
            return FSP_ERR_TRANSFER_ABORTED;  // 总线异常，直接失败
        }

        time_out--;                                    // 倒计时
        R_BSP_SoftwareDelay(I2C_TIMEOUT_UNIT,         // 每次等 10μs
                            BSP_DELAY_UNITS_MICROSECONDS);

        if (RESET_VALUE == time_out)                   // 255 × 10μs ≈ 2.55ms
        {
            return FSP_ERR_TIMEOUT;                    // 超时，对方无响应
        }
    }
    return FSP_SUCCESS;  // 事件匹配，成功返回
}
```

**关键细节**：
- 这是**纯软件轮询**，没有用信号量或 RTOS 阻塞。I2C 中断修改 `g_i2c_event_for_peripheral`，while 循环检测到变化后退出。
- 超时时间 ≈ 255 × 10μs ≈ **2.55ms**。对于 393kHz I2C 传输 3 字节（仅需 ~115μs），这个超时足够。
- 如果从机 (OV5640) 没有响应 ACK（比如未复位），I2C 硬件会产生 NACK → Abort 事件，不会等到超时。

---

## 3. read_reg_16bit() — 读 OV5640 寄存器

### 3.1 为什么读比写复杂

OV5640 使用 **16-bit 寄存器地址**，读取一个寄存器需要**两阶段 I2C 事务**：

```
Phase A (Write):  告诉 OV5640 "我要读地址 0x3008"
Phase B (Read):   从 OV5640 读回该地址的 8-bit 数据
```

这是 I2C 标准的 **"复合事务" (Combined Transaction)**，两阶段之间用 **ReSTART** 衔接（不发 STOP）。

### 3.2 逐行拆解

```c
fsp_err_t read_reg_16bit(uint16_t address, uint8_t *p_data)
{
    fsp_err_t err = FSP_SUCCESS;
    uint8_t i2c_buffer[2];

    // ═══════════════════════════════════════════
    // Phase A: 发送 2 字节寄存器地址 (I2C Write)
    // ═══════════════════════════════════════════

    i2c_buffer[0] = (uint8_t)((address >> 8) & 0xFF);  // AddrH = 0x30
    i2c_buffer[1] = (uint8_t)(address & 0xFF);         // AddrL = 0x08

    g_i2c_event_for_peripheral = RESET_VALUE;

    // restart=true → 发送完 2 字节后产生 ReSTART，不发 STOP
    err = R_IIC_MASTER_Write(&g_i2c_master_for_peripheral_ctrl,
                              i2c_buffer, 2, true);
    if (err != FSP_SUCCESS) return err;

    // 等待 Phase A 完成
    err = i2c_master_wait_event(I2C_MASTER_EVENT_TX_COMPLETE);
    if (err != FSP_SUCCESS) return err;

    // ═══════════════════════════════════════════
    // Phase B: 接收 1 字节数据 (I2C Read)
    // ═══════════════════════════════════════════

    g_i2c_event_for_peripheral = RESET_VALUE;

    // 读取 1 字节，restart=false → 读完后产生 STOP
    err = R_IIC_MASTER_Read(&g_i2c_master_for_peripheral_ctrl,
                             p_data, 1, false);
    if (err != FSP_SUCCESS) return err;

    // 等待 Phase B 完成
    err = i2c_master_wait_event(I2C_MASTER_EVENT_RX_COMPLETE);
    return err;
}
```

### 3.3 I2C 总线波形 (复合事务)

```
 Phase A: Write (发寄存器地址)                   Phase B: Read (收数据)
┌────────────────────────────────┐       ┌──────────────────────────┐
│START│ 0x78(W) │0x30│0x08│      │       │ReSTART│ 0x79(R) │ DATA │STOP│
│     │  ACK    │ACK │ACK │      │       │       │  ACK    │ NACK │    │
└────────────────────────────────┘       └──────────────────────────┘
                  ↑       ↑                                ↑
             寄存器地址   寄存器地址                    OV5640 返回的数据
             高字节       低字节                      (写入 *p_data)

从机地址:
  0x78 = 0x3C << 1 | 0  (写)
  0x79 = 0x3C << 1 | 1  (读)

关键: Phase A 结束时产生的是 ReSTART (SDA 在 SCL 高时翻转)
     而不是 STOP (SDA 在 SCL 低时释放再拉高)
```

### 3.4 `restart` 参数的语义

| 函数调用 | restart 值 | 最后一个字节传输完后... |
|----------|-----------|----------------------|
| `R_IIC_MASTER_Write(..., 3, false)` | `false` | 产生 **STOP** (释放总线) |
| `R_IIC_MASTER_Write(..., 2, true)` | `true` | 产生 **ReSTART** (保持占用，切到读模式) |
| `R_IIC_MASTER_Read(..., 1, false)` | `false` | 读完最后一个字节发 **NACK + STOP** |

---

## 4. write_reg_8bit / read_reg_8bit — 读写 8-bit 地址设备

这两个函数用于访问 **PI4IOE5V6408 I/O 扩展器**（板载开关控制），该芯片使用 8-bit 寄存器地址。

### 4.1 与 16-bit 版本的区别

| | write_reg_16bit | write_reg_8bit |
|---|---|---|
| 地址宽度 | 16-bit (2 字节) | 8-bit (1 字节) |
| I2C 包长度 | 3 字节 | 2 字节 |
| 目标设备 | OV5640 摄像头 | PI4IOE5V6408 开关芯片 |
| 从机地址 | 0x3C | (不同地址，由 SlaveAddressSet 切换) |

### 4.2 write_reg_8bit 数据包

```c
// 地址 8-bit，数据 8-bit → I2C 包只有 2 字节
i2c_buffer[0] = address;     // 1 字节寄存器地址
i2c_buffer[1] = data;        // 1 字节数据

// 发送
R_IIC_MASTER_Write(ctrl, i2c_buffer, 2, false);
```

**总线波形**：
```
START │ 0xXX(W) │ ADDR │ DATA │ STOP
      │  ACK    │ ACK  │ ACK  │
```

### 4.3 read_reg_8bit 流程

```
Phase A: START │ Slave(W) │ Register_Addr │ ReSTART
Phase B: ReSTART │ Slave(R) │ DATA(NACK) │ STOP
```

---

## 5. DTC (Data Transfer Controller) 的角色

### 5.1 配置回顾

```c
// hal_data.c — I2C Ch1 配置了 DTC 通道
.p_transfer_tx = &g_transfer0,   // 激活源: IIC1_TXI
.p_transfer_rx = &g_transfer1,   // 激活源: IIC1_RXI
```

### 5.2 DTC 为什么不参与当前传输

```
当前模式: 阻塞式 I2C 传输

中断发生时:
  IIC TXI IRQ ─┬─→ DTC Trigger (已配置，但 HAL 内部也管理 FIFO)
               │
               └─→ IIC ISR → g_i2c_master_for_peripheral_callback()
                     └─→ 设置事件标志 → 解除 i2c_master_wait_event() 的阻塞

实际的 FIFO 数据搬运由 IIC 硬件和 HAL ISR 完成，DTC 在此为"影子通道"，
用于未来如果切换到非阻塞大批量传输时自动搬运数据。
```

```
┌─────────────────────────────────────────────────────────────┐
│  IIC TX FIFO (硬件)                                         │
│  ┌─────┬─────┬─────┬─────┐                                 │
│  │ 0x30│ 0x08│ 0x42│     │  HAL 写入 3 字节               │
│  └─────┴─────┴─────┴─────┘                                 │
│    ↓     ↓     ↓                                            │
│  [IIC Shift Register] → SDA 引脚串行输出                    │
│                                                             │
│  DTC 在这里可以做但没做的事情:                                │
│  ┌──────────────────────────────────────────────┐          │
│  │ 如果启用 DTC TX:                              │          │
│  │   TXI 触发 → DTC 自动从内存搬运下一个字节到 FIFO │          │
│  │   适用于连续发送大批量数据 (如整个帧缓冲)         │          │
│  └──────────────────────────────────────────────┘          │
└─────────────────────────────────────────────────────────────┘
```

---

## 6. 从机地址切换

摄像头和开关芯片共用 I2C Ch1，通过运行时切换从机地址区分：

```c
// 切换到摄像头
R_IIC_MASTER_SlaveAddressSet(&g_i2c_master_for_peripheral_ctrl,
                              REG_CAM_I2C_SLAVE_ADDR,  // 0x3C
                              I2C_MASTER_ADDR_MODE_7BIT);

// 切换到开关芯片 (switch_init.c 中)
R_IIC_MASTER_SlaveAddressSet(&g_i2c_master_for_peripheral_ctrl,
                              SWITCH_I2C_SLAVE_ADDR,    // PI4IOE5V6408 地址
                              I2C_MASTER_ADDR_MODE_7BIT);
```

---

## 7. 完整时间线 (一条寄存器的写+读校验)

以写入 `{0x3008, 0x42}` 为例，从 C 代码到波形返回的完整时间线：

```
时刻 (μs)
0         50       100      150      200      250      300      350
│─────────│────────│────────│────────│────────│────────│────────│
│                                                                 │
│ [CPU] 进入 camera_write_array()                                  │
│ [CPU] p_array->reg=0x3008, p_array->val=0x42                    │
│                                                                 │
│ ─── write_reg_16bit(0x3008, 0x42) ───                           │
│ [CPU] 构造 i2c_buffer = {0x30, 0x08, 0x42}                      │
│ [CPU] R_IIC_MASTER_Write(ctrl, buf, 3, false)                   │
│         → 写入 FIFO + 触发 START                                 │
│                                                                 │
│ [I2C] START → W(0x78)→ACK→0x30→ACK→0x08→ACK→0x42→ACK→STOP     │
│         ←── 约 115μs @ 393kHz ──→                               │
│                                                     │           │
│ [IRQ] TXI 中断 → callback → g_event = TX_COMPLETE   │           │
│                                                     │           │
│ [CPU] i2c_master_wait_event() 检测到完成，返回 SUCCESS           │
│                                                                 │
│ ─── read_reg_16bit(0x3008, &value) ───                          │
│                                                                 │
│ [CPU] 构造 i2c_buffer = {0x30, 0x08}                            │
│ [CPU] R_IIC_MASTER_Write(ctrl, buf, 2, restart=true)            │
│                                                                 │
│ [I2C] START → W(0x78)→ACK→0x30→ACK→0x08→ACK→ReSTART           │
│         ←── 约 90μs ──→                                         │
│                                                                 │
│ [IRQ] TXI → callback → g_event = TX_COMPLETE                    │
│ [CPU] 等待通过，进入 Phase B                                     │
│ [CPU] R_IIC_MASTER_Read(ctrl, &value, 1, restart=false)         │
│                                                                 │
│ [I2C] ReSTART → R(0x79)→ACK→DATA(0x42)→NACK→STOP              │
│         ←── 约 65μs ──→                                         │
│                                                     │           │
│ [IRQ] RXI 中断 → callback → g_event = RX_COMPLETE   │           │
│                                                     │           │
│ [CPU] value == 0x42?                                         │
│       ✓ YES → p_array++ → 处理下一条寄存器                    │
│       ✗ NO  → FSP_ERR_ASSERTION → handle_error → 停机         │
│                                                                 │
│ 总耗时: ~200-300μs / 寄存器（含 I2C 传输 + CPU 开销）            │
│ 100+ 条寄存器: ~30ms (其中有几条包含 50ms delay)                 │
│ 实际 camera_open() 总耗时: ~200ms                               │
└─────────────────────────────────────────────────────────────────┘
```

---

## 8. 中断回调链路

```
         ┌──────────────────────────────────┐
         │     IIC Peripheral (硬件)          │
         │                                   │
         │  TXI ──→ 发送 FIFO 空 → 需新数据   │
         │  RXI ──→ 接收 FIFO 满 → 有新数据   │
         │  TEI ──→ STOP 发出后               │
         │  ERI ──→ NACK / 仲裁丢失 / 超时    │
         └──────────────┬───────────────────┘
                        │ 中断请求
                        ▼
         ┌──────────────────────────────────┐
         │  NVIC (嵌套向量中断控制器)         │
         │  IIC1_TXI → VECTOR_NUMBER_xxx     │
         │  IIC1_RXI → VECTOR_NUMBER_xxx     │
         │  IIC1_TEI → VECTOR_NUMBER_xxx     │
         │  IIC1_ERI → VECTOR_NUMBER_xxx     │
         │  I2C IPL = 0 (最高优先级!)         │
         └──────────────┬───────────────────┘
                        │
                        ▼
         ┌──────────────────────────────────┐
         │  FSP IIC HAL 内部 ISR             │
         │  - 处理 FIFO 读写                  │
         │  - 调用用户注册的回调函数           │
         └──────────────┬───────────────────┘
                        │
                        ▼
         ┌──────────────────────────────────────────┐
         │  g_i2c_master_for_peripheral_callback()   │
         │                                          │
         │  void callback(i2c_master_callback_args_t *p)  │
         │  {                                        │
         │      g_i2c_event = p->event;  // 仅保存事件 │
         │      // TX_COMPLETE / RX_COMPLETE / ABORTED │
         │  }                                        │
         └──────────────┬───────────────────────────┘
                        │
                        ▼
         ┌──────────────────────────────────────────┐
         │  i2c_master_wait_event()                  │
         │  while (g_event != expected) { ... }     │
         │  检测到事件匹配 → 退出 while → return OK  │
         └──────────────────────────────────────────┘
```

---

## 9. I2C 配置参数汇总

| 配置项 | 值 | 说明 |
|--------|-----|------|
| 通道 | **IIC Channel 1** | P05_11(SDA), P05_12(SCL) |
| 速率 | **Fast-mode** (~393kHz) | BRH=15, BRL=15, CKS=2 |
| 地址模式 | **7-Bit** | OV5640 = 0x3C, Switch = 另配 |
| 超时模式 | Short Mode | |
| SCL Low 超时 | Enabled | 卡总线时自动检测 |
| TX DMA | DTC g_transfer0 (未实际使用) | 激活源 IIC1_TXI |
| RX DMA | DTC g_transfer1 (未实际使用) | 激活源 IIC1_RXI |
| 中断优先级 | **0 (最高)** | 摄像头初始化时不能被打断 |
| 引脚驱动 | DRIVE_MID | 开漏输出特征 |
| 引脚上拉 | **NMOS_ENABLE + PULLUP_ENABLE** | 内部上拉使能 |

---

## 10. 避坑要点

### 10.1 I3C 冲突

```
RA8P1 的 P05_11/P05_12 默认可能配置为 I3C 模式。
必须通过 GPIO 禁用 I3C:

  P00_13 (I3C_SCL_PU) = HIGH   ← 禁用 I3C 时钟上拉
  P01_09 (I3C_SDA_PU) = HIGH   ← 禁用 I3C 数据上拉
  P00_00 (I3C_SEL)    = HIGH   ← 选择非 I3C 模式

如果不做这三步，I2C 通信会受 I3C 内部电路干扰，表现为
write_reg_16bit() 卡在 wait_event() 等到 TIMEOUT。
```

### 10.2 复位时序

```
OV5640 要求上电后先硬件复位，再通过 I2C 配置:

  1. XCLK (24MHz) 必须已稳定  ← GPT12 先初始化
  2. P07_09 LOW (≥1ms)        ← camera_hw_reset() 用了 20ms
  3. P07_09 HIGH (≥1ms)       ← camera_hw_reset() 用了 20ms
  4. 等待 ≥20ms 后开始 I2C 通信 ← camera_open() 在复位后做了 delay

如果复位时序不对，OV5640 不会响应 I2C (NACK 所有地址)。
R_IIC_MASTER_Write() 会在第一个 ACK 位就收到 NACK → ABORTED。
```

### 10.3 上拉电阻

```
Fast-mode I2C (400kHz) 的上升时间要求: tr ≤ 300ns

内部上拉电流 (RA8P1 PULLUP): ~50μA
总线电容 (PCB 走线 + 两个器件): ~20-50pF
上升时间 = R × C = (Vdd/I) × C = (3.3V/50μA) × 30pF ≈ 2μs ← 不满足!

结论: 内部上拉不够强。检查 EK-RA8P1 原理图，确认板上是否有
外部上拉电阻 (通常 2.2kΩ ~ 4.7kΩ)。如果波形上升沿太缓，
I2C 通信会间歇性失败。
```

### 10.4 写后回读校验的必要性

```c
// camera_write_array() 中每写入一条寄存器都会立即回读
err = write_reg_16bit(p_array->reg, p_array->val);
err = read_reg_16bit(p_array->reg, &value);
if (value != p_array->val) {
    // 写入值与回读值不匹配 → 直接报错停机
    return FSP_ERR_ASSERTION;
}
```

这是**防御性编程**：即使 I2C ACK 全部正常，也不能 100% 保证寄存器写入成功（OV5640 内部可能正在忙、处于错误状态、或寄存器是只读的）。回读校验能在第一时间发现异常，避免后续 MIPI 数据流异常时难以定位根因。

### 10.5 中断优先级设置

```
I2C IPL = 0 (最高) 的原因:

camera_open() 期间，需要连续写入 100+ 条寄存器。
如果 I2C 中断被其他中断 (如 GPT, SCI) 打断:

  1. I2C TX FIFO 可能 underrun (来不及填充)
  2. i2c_master_wait_event() 等待时间变长
  3. 最坏情况: 超时 (2.55ms) → 初始化失败

初始化完成后，主循环中不再有密集 I2C 操作，
高优先级不影响其他模块。
```

---

*文档生成时间: 2026-06-03 | 基于源码 i2c_control.c / camera_sensor.c 逐行分析*
