# 03_esp-brookesia

[English](README.md) | [简体中文](README_ZH.md)

## 功能

在 AMOLED 屏上运行 ESP-Brookesia 手机风格界面：带应用图标的主屏、带时钟的状态栏，
以及随示例提供的 SquareLine 演示应用。

## 硬件需求

- ESP32-S3-Touch-AMOLED-1.75C 开发板
- 用于烧录和监视的 USB 线

## 怎么运行

```bash
idf.py set-target esp32s3 build flash monitor
```

依赖在 [`main/idf_component.yml`](main/idf_component.yml) 中声明，首次构建时自动下载。
`brookesia_core` 和 `brookesia_app_squareline_demo` 是 [`components/`](components/) 下的本地组件。

## 预期现象

屏幕显示 Brookesia 主屏，包含状态栏和应用图标。滑动和点击可以在各屏之间切换，
点击 SquareLine 演示图标进入演示应用。状态栏时钟每秒刷新一次。串口监视器启动时打印
`Display ESP-Brookesia phone demo`。（待硬件确认）

## 常见问题

- **修改界面后出现栈溢出或内存分配失败。** Brookesia 创建屏幕所需的栈超过适配器默认值，
  所以示例把 LVGL 任务栈设为 40 KB 并放在 PSRAM 中（`main/main.cpp` 的
  `LVGL_TASK_STACK_SIZE`）。除非已经在全部受支持的 ESP-IDF 版本上重新做过硬件验证，
  否则请保持栈大小和 `sdkconfig.defaults` 里的内存相关选项不变。
- **更新 LVGL、显示、触摸、音频或 Brookesia 依赖。** 这是仓库里最大的界面示例，更新共用依赖前，
  请先确认它在全部受支持的 ESP-IDF 版本上仍能构建。如果后续上游 Brookesia 版本改变了支持的
  ESP-IDF 范围，请同步更新示例清单和本文档。
- **构建通过但开发板表现异常。** 构建通过只说明编译和打包没有问题，显示、触摸、PMU 和
  传感器初始化仍需在开发板上验证。
