# TODO — MIPI CSI 项目待办事项

> 更新日期: 2026-06-03

---

| 优先级 | 编号 | 状态 | 问题描述 |
|--------|------|------|---------|
| **P0** | #4 | 🔲 TODO | 加入 AI 推理功能 |
| **P1** | #2 | 🔲 TODO | 屏幕分辨率缩放功能尚未实现 |
| **P2** | #1 | 🔲 TODO | 上电后有一定概率需要长按复位键约 4 秒才能正常工作（根因不明） |
| **P3** | #3 | 🔲 TODO | 运行不稳定：高速旋转摄像头（大幅改变图像内容时）可能导致显存溢出卡死 |

---

## #4 [P0] 加入 AI 推理功能

- **目标**: 在现有 MIPI CSI → VIN → SDRAM → GLCDC 流水线中集成 Ethos-U55 NPU，实现实时 AI 推理（如目标检测、分类等）。
- **关键点**:
  - VIN 输出的 RGB565 帧缓冲已在 SDRAM 中，NPU 可直接读取。
  - 需评估 NPU 推理延迟 vs 帧率 (60fps → 16.7ms/frame)，确定是否每帧推理还是跳帧推理。
  - 参考 Renesas [Vision AI Application Note](https://www.renesas.com/en/document/apn/building-vision-ai-application-using-ra8p1-mcu-ethos-u55-npu)。
- **依赖**: 无硬依赖，可独立开发。

---

## #2 [P1] 屏幕分辨率缩放功能尚未实现

- **现象**: 当前代码硬编码 `RES_1024x600`，注释掉了原有的终端交互菜单（分辨率选择 + 模式选择）。VGA (640×480) 和 QVGA (320×240) 无法通过用户输入切换。
- **现有基础**:
  - `vin_scale_image()` 已实现运行时缩放参数计算（UDS mask / clip size 动态调整）。
  - `camera_profiles[]` 已定义三种分辨率的宽高。
  - `g_vin_cfg_run_time` 和 `g_vin_cfg_run_time_extend` 运行时可修改。
- **待解决**:
  - 恢复终端菜单交互（或改用其他触发方式，如按键）。
  - 缩放后 GLCDC 的 `hsize`/`vsize`/`hstride` 需要同步更新。
  - 缩放后 `R_GLCDC_BufferChange()` 的帧缓冲地址需要对应更新。
  - 验证 VIN 的 YCbCr→RGB565 转换 + UDS 缩放在非 1:1 比例下的画质。
- **受影响文件**: `src/mipi_csi.c`, `src/glcdc_display.c`, `src/camera_sensor.h`

---

## #1 [P2] 上电后概率性需要长按复位才能启动

- **现象**: 上电后有一定概率系统不工作，需要长按复位键约 4 秒后才能正常运行。
- **根因**: 尚不明确。怀疑方向：
  - **SDRAM 初始化时序**: `R_BSP_SdramInit(true)` 在某些上电斜率下可能未正确完成 SDRAM 预充电/刷新配置，导致帧缓冲区不可靠。
  - **摄像头 XCLK 稳定时间**: GPT12 产生的 24MHz 可能在摄像头 VDD 稳定前就输出了，OV5640 PLL 未能正常锁定。
  - **I2C 总线初始状态**: 上电瞬间 I3C 禁用引脚 (P00_13/P01_09/P00_00) 的初始电平可能不够快，导致 I2C Ch1 短暂处于 I3C 模式，摄像头寄存器的第一批写入失败。
  - **电源轨上电顺序**: 多路电源 (MCU Core / IO / SDRAM / Camera) 的上电斜率不同，可能导致某些外设在 MCU 初始化时处于不确定状态。
- **建议排查手段**:
  - 在 `camera_open()` 中每个 I2C 写操作增加重试机制（当前是单次写+回读，失败直接 ASSERT）。
  - 在 `R_BSP_WarmStart()` 的 `SDRAM_INIT` 后增加更长延时。
  - 用示波器同时抓 XCLK、CAMERA_RESET、I2C SDA 三路信号，比对正常启动和异常启动的时序差异。
  - 检查 BSP 中 SDRAM Power-On Delay 配置是否足够。

---

## #3 [P3] 画面剧烈变化时显存溢出卡死

- **现象**: 高速旋转摄像头（大幅改变图像内容）时系统卡死，推测与显存/总线带宽溢出有关。
- **分析**:
  - OV5640 输出的是 JPEG 压缩后的 MIPI 数据还是原始 YUV 数据？如果是 JPEG，复杂纹理压缩率低→实际数据量增大→MIPI 带宽瞬时超限。
  - VIN 三缓冲在 SDRAM 中，每帧 1.2MB × 3 = 3.6MB。如果 GLCDC 读取一帧的时间 > VIN 写入一帧的时间，三缓冲会逐渐被填满，最终覆盖还未被 GLCDC 读完的缓冲。
  - MIPI CSI RX FIFO overflow：CSI 接收速率 > VIN 处理速率时，硬件 FIFO 溢出，可能触发错误中断但当前 `mipi_csi0_callback()` 是空处理。
- **建议排查手段**:
  - 在 `mipi_csi0_callback()` 中添加对 `MIPI_CSI_EVENT_FRAME_DATA` 和 `VIRTUAL_CHANNEL` 事件中 overflow 位 (OVF) 的检测与计数。
  - 在 `vin_callback()` 中添加帧计数，检测是否丢帧。
  - 降低 FPS (修改 `FPS_TARGET` 从 60→30)，观察是否改善。
  - 检查 OV5640 的 MIPI 输出格式配置——确认是 YUV422 而非 JPEG，确保数据速率恒定。
  - 检查 GLCDC 的 Underflow 中断是否频繁触发 (当前 IPL=12，中断已使能但未做处理)。
- **受影响模块**: MIPI CSI / VIN / SDRAM 带宽 / GLCDC

---

## 优先级汇总

```
P0 (紧急):  #4  加入 AI 推理功能
P1 (高):    #2  屏幕分辨率缩放
P2 (中):    #1  上电概率性无法启动
P3 (低):    #3  画面剧烈变化时卡死
```

---
