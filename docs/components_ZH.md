# 组件

[English](components.md) | [简体中文](components_ZH.md)

示例使用 Waveshare 托管板级组件 `waveshare/esp32_s3_touch_amoled_1_75c`
（`^3.0.0`）。当前版本支持 ESP-IDF 5.5 及以上；CI 使用 ESP-IDF `v5.5.5`
和 `v6.0.2` 验证产品示例。

仓库中的本地组件具有不同的所有权和用途，因此有意保留：

- `brookesia_app_squareline_demo` 是 Brookesia 示例使用的产品 UI 功能代码。
- `brookesia_core` 是嵌入式上游框架。除非执行明确的上游同步，否则应保留其源码、
  归属说明和文档。
- `bsp_extra` 是构建在托管 BSP 和编解码器 API 之上的板级音频胶水代码。

未来的组件工作应优先采用 Waveshare 和 Espressif 的托管组件，但前提是存在语义等价、
硬件兼容且覆盖所选 CI 矩阵的组件。

开发板专用的示例组合、临时兼容代码和仅供示例使用的资源应留在对应示例附近。
可复用的 BSP、显示、触摸、传感器、音频和总线修复应尽可能先进入共享组件源。
仅凭组件名称或目录位置不足以证明可以删除组件。

## 硬件交叉核对边界

对仓库原理图进行只读比对后，确认 Arduino 开发板头文件中的 LCD QSPI 数据 GPIO4–GPIO7、
SCLK GPIO38、CS GPIO12、复位 GPIO1 以及 466×466 分辨率。还确认触摸 I2C SDA GPIO15/
SCL GPIO14、中断 GPIO11 和复位 GPIO2；复位定义已修正，LCD 复位与触摸复位不再混淆。
音频值仍为 BCLK GPIO9、LRCK GPIO45、DIN GPIO10、MCLK GPIO16、DOUT GPIO8 和 PA GPIO46。
顶部物理 PWR 按键经反相 MOSFET 接入 GPIO3，底部 BOOT 按键接入 GPIO0。
本地头文件未重复定义的 QMI8658、AXP 和其他信号仍应以原理图和托管 BSP 为准。构建成功并不等同于
实体硬件上的运行行为已得到验证。

## 底层硬件架构要点

### 物理按键与 PMIC 中断连接
- **PWR 按键 (GPIO3)**：顶部按键通过 BSS138 N 沟道 MOSFET 反相连接。按下时 `gpio_get_level(GPIO_NUM_3)` 返回 `1`，释放时返回 `0`。
- **BOOT 按键 (GPIO0)**：底部按键为标准低电平有效启动开关。
- **PMIC 中断连接**：AXP2101 的 IRQ 引脚未连接至 ESP32。固件必须直接读取 GPIO3 电平检测电源键，不能依赖 PMIC 中断状态。

### 电源轨拓扑与低功耗优化
- **连接电源轨**：板上仅连接了 DCDC1（3.3V 系统逻辑）与 ALDO1（3.3V 音频编解码器模拟供电）。
- **悬空电源轨**：DCDC2–5、ALDO2–4、BLDO1–2、CPUSLDO 与 DLDO1–2 未连接任何外设电路。
- **功耗优化**：系统启动后应关闭所有未连接的电源轨，以消除不必要的电池静态功耗。

### CO5300 显示控制器刷新窗口对齐
- **坐标对齐**：CO5300 控制器要求局部刷新区域必须严格按偶数对齐（`x1 &= ~1`，`y1 &= ~1`，`x2 |= 1`，`y2 |= 1`）。
- **异常现象**：若使用奇数边界坐标，会导致画面水平错位条纹或控制器总线挂起。

### CST9217 触摸深度休眠与唤醒时序
- **深度休眠**：通过 I2C 发送 `0xD105` 指令使 CST9217 进入超低功耗待机模式。
- **硬件唤醒**：将触摸复位引脚（GPIO2）拉低 10 ms，随后拉高保持 50 ms，方可恢复 I2C 通信。

### 时钟与调制解调器休眠限制
- **RTC 晶振**：开发板未板载外部 32.768 kHz RTC 晶振。
- **BLE 轻度休眠时钟**：在启用 BLE Modem 轻度休眠时，控制器必须保持 40 MHz 主晶振运行（`CONFIG_BT_CTRL_LPCLK_SEL_MAIN_XTAL=y` 与 `CONFIG_BT_CTRL_MAIN_XTAL_PU_DURING_LIGHT_SLEEP=y`）。
- **GPIO 引脚状态保持**：避免开启 `CONFIG_ESP_SLEEP_GPIO_RESET_WORKAROUND`，以防休眠期间引脚悬空导致显示屏或触摸芯片意外复位。

### 音频编解码器增益步进
- **板载芯片**：扬声器 DAC 功放采用 Everest Semi ES8311，麦克风 ADC 采用 Everest Semi ES7210 双通道 ADC。
- **PGA 量化**：ES7210 可编程增益放大器以 3 dB 为步进（例如配置 33 dB 会自动调整至 34.5 dB）。

### 无线网络并发下的内部 DMA 内存调度
- **内存碎片**：Wi-Fi 与蓝牙并发运行会导致内部 SRAM 碎片化，连续可用 DMA 内存通常不足 16 KB。
- **乒乓缓冲**：建议在内部 SRAM 静态分配两个小型 DMA 缓冲区（如 7 KB），并通过 FreeRTOS 任务分片传输，以避免整屏或大块刷新时的 DMA 内存分配失败。

