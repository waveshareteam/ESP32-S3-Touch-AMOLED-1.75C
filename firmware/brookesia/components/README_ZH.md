# Brookesia 固件组件

[English](README.md)

本目录存放 ESP32-S3-Touch-AMOLED-1.75C 固件的应用、共享服务、板级支持包和锁定版本的
上游覆盖组件。下面每一类的维护边界不同。

项目 `CMakeLists.txt` 通过 `EXTRA_COMPONENT_DIRS` 引入
`examples/esp-idf/03_esp-brookesia/components/` 中的 Brookesia 核心和 SquareLine
演示，这里不复制它们。

## 应用

| 目录 | 启动器应用 |
| --- | --- |
| [`brookesia_app_calculator/`](brookesia_app_calculator/) | Calculator |
| [`ButtonTest/`](ButtonTest/) | PWR 键（GPIO3）和 BOOT 键（GPIO0）原始电平测试 |
| [`Crosshair/`](Crosshair/) | 圆形显示和触摸对位靶 |
| [`draw/`](draw/) | DrawPanel |
| [`Gravitysphere/`](Gravitysphere/) | 由 QMI8658 驱动的重力小球 |
| [`MusicPlayer/`](MusicPlayer/) | 从 SPIFFS 播放 MP3 和 WAV |
| [`Settings/`](Settings/) | Wi-Fi、声音、屏幕亮度和息屏、电池以及设备信息 |
| [`SpecAnalyzer/`](SpecAnalyzer/) | ES7210 麦克风实时频谱 |
| [`XiaozhiApp/`](XiaozhiApp/) | AIChats 语音和文字应用 |

## 共享服务

| 目录 | 职责 |
| --- | --- |
| [`bsp_extra/`](bsp_extra/) | ES8311 和 ES7210 音频、功放诊断、文件播放，以及独占的音频会话所有权 |
| [`display_power/`](display_power/) | 屏幕亮度和息屏：变暗、熄灭、触摸唤醒和 PWR 键切换 |
| [`storage_service/`](storage_service/) | SD 卡租约。1.75C 没有 SD 卡槽，所有租约都会失败，MusicPlayer 改用 SPIFFS。 |
| [`system_status/`](system_status/) | 供状态栏和 Settings 使用的 AXP2101 电池和 Wi-Fi 状态 |

应用通过这些公开 API 使用硬件。应用不得另开一个编解码器会话，不得直接设置屏幕亮度，
也不得自己维护 Wi-Fi 或电池状态。

## 本地覆盖组件

| 目录 | 上游基础和本地改动 |
| --- | --- |
| [`esp_audio_player_safe/`](esp_audio_player_safe/) | `chmorgan/esp-audio-player` 1.1.0，确定性地清理工作线程和排队文件 |
| [`esp_xiaozhi_safe/`](esp_xiaozhi_safe/) | Espressif `esp_xiaozhi` 0.1.1，网络和事件队列等待有上限 |

每个覆盖组件都保留上游组件的公开版本和 API。覆盖清单、源码、包装 README 和使用方的
`override_path` 要保持一致。在上游合入修复或不再需要修复之前，不要用组件库下载替换覆盖组件。

## 板级支持包

`waveshare__esp32_s3_touch_amoled_1_75/` 是微雪 1.75 的板级支持包，已按 1.75C 引脚修改，
保留 1.75 的组件名。1.75C 的改动如下：

- I2S MCLK 在 GPIO16，LCD 复位在 GPIO1，触摸复位在 GPIO2。
- 没有 TCA9554 IO 扩展芯片。`BSP_PWR_KEY_IO`（GPIO3）和 `BSP_BOOT_KEY_IO`（GPIO0）
  定义按键输入。
- 没有 microSD 卡槽。`BSP_CAPS_SDCARD` 为 `0`，`bsp_sdcard_mount()` 返回
  `ESP_ERR_NOT_SUPPORTED`。

上游组件或 `third_party/` 目录下的代码保留其上游许可证、署名、README 和命名。分发前，
请按各自的许可证审查媒体和提示音资源。

## 验证

修改应用或共享服务后，编译整个项目，并重做
[`../HARDWARE_VALIDATION_ZH.md`](../HARDWARE_VALIDATION_ZH.md) 中的相关章节。修改音频或
应用清理逻辑时，还要测试快速进出和应用间切换。编译通过不能证明板上的资源所有权正确。
