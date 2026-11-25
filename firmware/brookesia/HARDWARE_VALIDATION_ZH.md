# ESP32-S3-Touch-AMOLED-1.75C 硬件验证

[English](HARDWARE_VALIDATION.md)

每次修改源码后，在 1.75C 板上完成本清单。出现以下情况即判定检查失败：重启、看门狗
复位、栈溢出、白屏、画面只刷新一部分，或音频占用冲突。

每项检查都给出要找的串口日志。日志来自
`idf.py -C firmware/brookesia -p <PORT> monitor`。打开串口会复位开发板。

## 1. 烧录和启动

1. 烧录全部分区：

   ```bash
   idf.py -C firmware/brookesia -p <PORT> flash monitor
   ```

2. 确认加载画面依次走完各阶段，显示 **Ready**，再淡出到完整的 466 x 466 启动器。
3. 确认日志中 10 个应用各有一行 `Installed <name>`。
4. 确认日志有以下几行，且没有 `E (` 开头的行：

   ```text
   Brookesia firmware is ready
   display_power: Loaded brightness 100%, timeout 60 s
   display_power: PWR key GPIO3 level at start: 0
   ```

   上面的亮度和超时是未保存过设置的板子上的默认值。

## 2. 息屏

1. 打开 **Settings > Display**，在 **Screen timeout** 下选择 **15 seconds**。
   确认勾号移到这一行。
2. 不要操作开发板。确认屏幕在 10 s 变暗，在 15 s 熄灭。日志显示：

   ```text
   display_power: Screen Awake -> Dimmed (idle 10xxx ms, timeout 15 s)
   display_power: Screen Dimmed -> Off (idle 15xxx ms, timeout 15 s)
   ```

3. 在熄灭的屏幕上点一个启动器图标。确认屏幕亮起，且应用没有打开。
4. 等屏幕再次熄灭，从屏幕边缘向内滑动。确认屏幕亮起，且没有触发返回或主页手势。
5. 确认亮屏后显示的是当前内容。例如状态栏时钟必须显示当前时间。
6. 一根手指按住屏幕 20 s。确认屏幕保持全亮。
7. 短按 PWR，确认屏幕熄灭。再短按 PWR，确认屏幕亮起。
8. 拖动亮度滑块，确认拖动时亮度跟着变化。
9. 复位开发板。确认日志显示你设置的亮度和 `timeout 15 s`。
10. 选择 **Never**，2 分钟不操作。确认屏幕保持全亮。

## 3. Button Test

1. 两个按键都松开时打开 **Button Test**。确认 PWR 显示 `RAW LOW`，BOOT 显示
   `RAW HIGH`。两张卡片都显示 `RELEASED` 和 `WAITING`。
2. 短按 PWR。确认按住时 PWR 卡片显示 `RAW HIGH` 和 `PRESSED`，之后显示
   `PASS LATCHED`。这次按键也会熄屏，再短按 PWR 即可亮屏。
3. 按 BOOT。确认 BOOT 卡片显示 `RAW LOW` 和 `PRESSED`，之后显示 `PASS LATCHED`。
4. 确认底部显示 `ALL BUTTONS PASSED`。
5. 退出并重新打开 Button Test 五次。确认启动器保持响应，且每次新打开都清除两个结果。

## 4. 音频

1. 打开 **SpecAnalyzer**，对着麦克风说话。确认频谱跳动。
2. 打开 **MusicPlayer**。若编译时 `spiffs/music/` 为空，确认应用能打开并提示没有曲目。
   若有曲目，播放一首并确认有声音。
3. 在 MusicPlayer、SpecAnalyzer 和 AIChats 之间切换。每个应用要么拿到音频，要么提示
   占用中，都不能导致重启。

## 5. 长时间测试

在观察串口日志的同时，把每个启动器应用打开、关闭至少 20 次。没有 panic、看门狗复位、
栈溢出或堆断言即为通过。把固件 SHA256、电池读数、Wi-Fi RSSI 和串口日志与结果一起记录。
