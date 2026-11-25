# 1.75C 的 Brookesia 固件

[English](README.md)

本项目是 ESP32-S3-Touch-AMOLED-1.75C 的 Brookesia 手机风格固件，带息屏功能。
它移植自 ESP32-S3-Touch-AMOLED-1.75 仓库的
[`firmware/brookesia`](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/tree/5481619/firmware/brookesia)
（commit 5481619）。

它不是 1.75C 的出厂固件。出厂镜像和烧录方法见 [`../README_ZH.md`](../README_ZH.md)。

## 硬件

固件使用板载的 466 x 466 CO5300 QSPI AMOLED、CST9217 触摸芯片、ES8311 扬声器
编解码器、ES7210 麦克风 ADC、AXP2101 电源芯片、QMI8658 运动传感器和 ESP32-S3
Wi-Fi。

1.75C 与 1.75 的区别如下：

| 项目 | 1.75 | 1.75C |
| --- | --- | --- |
| I2S MCLK | GPIO42 | GPIO16 |
| LCD 复位 | GPIO39 | GPIO1 |
| 触摸复位 | GPIO40 | GPIO2 |
| PWR 键输入 | TCA9554 EXIO4 | GPIO3，高电平有效 |
| microSD 卡槽 | 有 | 无 |

[`components/waveshare__esp32_s3_touch_amoled_1_75/`](components/waveshare__esp32_s3_touch_amoled_1_75/)
中的板级支持包保留 1.75 的组件名，引脚已改为 1.75C。其 SD 函数返回
`ESP_ERR_NOT_SUPPORTED`。

## 应用

启动器有 10 个应用：

- SquareLine 演示。
- Gravitysphere：小球跟随 QMI8658 加速度计，在圆形屏幕边缘内滚动。
- Crosshair：全屏圆形对位靶。
- Button Test：显示 PWR 键（GPIO3）和 BOOT 键（GPIO0）的原始电平。
- Calculator。
- DrawPanel。
- SpecAnalyzer：ES7210 麦克风的实时频谱。
- MusicPlayer：播放 `/spiffs/music` 中的 MP3 和 WAV 文件。
- Settings：包含 **WLAN**、**Sound**、**Display**、**Power** 和 **About** 页面。
- AIChats：小智语音助手，带 WakeNet 唤醒词、VAD 和 Opus 音频。

1.75 固件还有 Gallery、Recorder、VideoPlayer、Settings 存储页和 AIChats 聊天记录。
它们都需要 microSD 卡，所以本移植删除了它们。

SPIFFS 镜像不含音乐。要给 MusicPlayer 添加曲目，请在编译前把 MP3 或 WAV 文件放进
`spiffs/music/`。6 MB 的 `storage` 分区还存放 AIChats 字体。

## 息屏

无人触摸时，屏幕先变暗，再熄灭。这一行为由
[`display_power`](components/display_power/) 组件负责。

| 事件 | 结果 |
| --- | --- |
| 无触摸时间达到超时减去变暗提前量 | 屏幕变暗到 10% 亮度。 |
| 无触摸时间达到超时 | 屏幕熄灭。 |
| 在变暗或熄灭时触摸屏幕 | 屏幕亮起。应用和手势收不到这次触摸。 |
| 按 PWR 50 ms 到 1.5 s | 屏幕熄灭；若屏幕已变暗或熄灭，则亮起。 |
| 按住 PWR 4 s | AXP2101 关机。 |

变暗提前量为 10 s；若超时的三分之一更短，则取三分之一。超时 15 s 时，10 s 变暗，
15 s 熄灭。超时 60 s 时，50 s 变暗。

在 **Settings > Display > Screen timeout** 中设置超时。可选 15 秒、30 秒、1 分钟、
2 分钟、5 分钟、10 分钟和 Never。同一页的亮度滑块范围为 20% 到 100%。

固件把两个值存在 NVS 命名空间 `display_power` 中：

| 键 | 类型 | 默认值 |
| --- | --- | --- |
| `brightness` | `u8`，百分比 | `100` |
| `timeout_s` | `u32`，秒，`0` 表示 Never | `60` |

屏幕熄灭时，LVGL 不绘制画面。亮屏时整屏重绘一次。

## 已知限制

- 在 Button Test 中按 PWR 也会熄屏。再按一次 PWR 即可亮屏。Button Test 仍会记录这次按键。
- 开机画面为 100% 亮度。启动器就绪后（复位后约 7 s）才应用保存的亮度。
- 分区表只用了 32 MB Flash 中的 16 MB。

## 编译和烧录

使用 ESP-IDF v5.5.4。本项目使用 `examples/esp-idf/03_esp-brookesia/components/`
中的 Brookesia 核心和 SquareLine 组件，所以要在本仓库内编译。

1. 替换出厂固件前先备份。出厂固件的源码未公开，备份是恢复它的唯一途径。

   ```bash
   esptool.py --chip esp32s3 -p <PORT> read_flash 0 0x2000000 factory-backup.bin
   ```

2. 编译项目。

   ```bash
   idf.py -C firmware/brookesia set-target esp32s3 build
   ```

3. 首次烧录全部分区。AIChats 要求分区表、`srmodels.bin`、应用和 `storage.bin` 一致。

   ```bash
   idf.py -C firmware/brookesia -p <PORT> flash monitor
   ```

4. 修改代码后，只烧录应用。NVS 中的设置会保留。

   ```bash
   idf.py -C firmware/brookesia -p <PORT> app-flash
   ```

分区表预留 8 MB 应用分区、6 MB `storage` SPIFFS 分区和 960 KB ESP-SR 模型分区。

要恢复出厂固件，把备份写到偏移 `0x0`：

```bash
esptool.py --chip esp32s3 -p <PORT> write_flash 0x0 factory-backup.bin
```

## 启动器图标

Crosshair 和 Button Test 的图标是 112 x 112 的 PNG 文件。LVGL v9 的
`LVGLImage.py` 脚本把它们转为 `ARGB8888` C 数据。编译解析完依赖后，用以下命令重新生成：

```text
python managed_components/lvgl__lvgl/scripts/LVGLImage.py --ofmt C --cf ARGB8888 --name img_app_crosshair --output components/Crosshair/assets components/Crosshair/assets/img_app_crosshair.png
python managed_components/lvgl__lvgl/scripts/LVGLImage.py --ofmt C --cf ARGB8888 --name img_app_button_test --output components/ButtonTest/assets components/ButtonTest/assets/img_app_button_test.png
```

## 硬件验证

编译通过只能证明代码能编译。修改源码后，请完成
[`HARDWARE_VALIDATION_ZH.md`](HARDWARE_VALIDATION_ZH.md) 中的板上检查。组件边界见
[组件指南](components/README_ZH.md)。
