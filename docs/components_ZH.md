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
本地头文件未重复定义的 QMI8658、AXP 和其他信号仍应以原理图和托管 BSP 为准。构建成功并不等同于
实体硬件上的运行行为已得到验证。
