# 持续集成

[English](ci.md) | [简体中文](ci_ZH.md)

`Build Examples` 工作流发现、构建并打包每个第一方示例。GitHub Releases 中发布的
示例固件来自该工作流；发布固件不通过手工编译产生。

## 发现边界

- ESP-IDF 工程是 `examples/esp-idf/` 下同时包含 `CMakeLists.txt` 和 `main/` 的直接子目录。
- Arduino 草图是 `examples/arduino/examples/` 下包含顶层 `.ino` 文件的第一方目录。
- `examples/arduino/libraries/**`、本地组件自带示例和 `Firmware/**` 均被排除。

`workflow_dispatch` 选择器接受 `all`、示例目录名或仓库相对路径。

## 验证矩阵

以下版本已于 2026-08-13 根据上游发布记录重新验证：

| 框架 | 版本 | 示例数 | 固件产物数 |
| --- | --- | ---: | ---: |
| ESP-IDF | `v5.5.5` | 5 | 5 |
| ESP-IDF | `v6.0.2` | 5 | 5 |
| Arduino-ESP32 | `3.3.11` | 7 | 7 |

ESP-IDF 目标为 `esp32s3`。Arduino 使用
`esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,FlashMode=qio,PartitionScheme=app3M_fat9M_16MB,USBMode=hwcdc,CDCOnBoot=cdc`
和随仓库提供的库：16 MB Flash、8 MB OPI PSRAM、16 MB 分区方案，并为板载原生 USB
端口启用硬件 USB CDC 与启动时 CDC。

完整工作流包含轻量策略任务、两个发现任务以及最多 17 个构建/打包任务。矩阵任务不会
快速失败，因此单个失败不会隐藏其他示例的结果。

每个拉取请求和分支推送会先运行轻量策略任务。其重命名感知分类器使用完整的
base/head 差异；差异不可用或为空时会失败关闭。Markdown 改动不会启动示例构建；
直接修改示例源码只选择该示例；共享输入或工作流输入选择对应的完整矩阵。
`Firmware/**` 改动会单独报告，且绝不会进入常规示例矩阵。

## 产物约定

每个成功构建上传一个 `*-combined.zip`，内容包括：

- `bin/` 下的原始偏移寻址二进制；
- 从 `0x0` 烧录的 `bin/<artifact-name>-combined.bin`；
- 包含框架、版本、工程、目标、Git SHA、文件大小和 SHA256 校验和的 `manifest.json`；
- `flash_combined.sh`、`flash_combined.bat` 和 `flash_combined_args.txt`；
- 分段镜像烧录所用的 `flash.sh`、`flash.bat` 和 `flash_args.txt`；
- 固件包 README。

打包器会拒绝重叠的二进制区域。合并镜像以 `0xFF` 填充未使用的地址间隙，并把每个
二进制保留在 ESP-IDF 或 Arduino 提供的偏移位置。

## 版本策略

CI 跟踪 ESP-IDF v5.5 系列的最新稳定补丁版、最新稳定 ESP-IDF v6 版本，以及仓库支持的
最新稳定 Arduino-ESP32 版本。升级版本时应完成：

1. 审查上游发布说明和迁移指南；
2. 运行完整 CI 矩阵；
3. 对受影响的演示进行硬件验证；
4. 更新文档和发布说明。

## 发布门禁

只有满足以下条件才可发布：

1. 拉取请求的策略任务和按变更范围选择的全部构建/打包任务成功；
2. 必要的硬件验证或维护者批准已完成；
3. 拉取请求已合并，发布标签指向合并后的提交；
4. 标签触发的完整 17 项 CI 矩阵成功；
5. 标签运行的所有压缩包通过 `prepare_release_assets.py` 验证；
6. GitHub Release 包含 17 个合并 ZIP 和 `manifest-combined-assets.json`。

维护命令见[发布脚本](../releases/README_ZH.md)。
