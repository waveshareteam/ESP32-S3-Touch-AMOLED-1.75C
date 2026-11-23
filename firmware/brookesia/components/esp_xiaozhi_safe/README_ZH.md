# ESP Xiaozhi 安全覆盖层

[English](README.md)

本目录内置 Espressif `esp_xiaozhi` 0.1.1，并由
`components/XiaozhiApp/idf_component.yml` 通过 `override_path` 选用。

本地修改把 WebSocket 文本和二进制发送从无限等待改为两秒超时，拒绝超时及短写
结果，并把聊天事件投递等待限制为 100 ms。这样可以避免网络停滞或事件队列已满时，
永久占用 Xiaozhi 音频发送互斥锁以及共享的 ES8311/ES7210 音频会话。

通用组件 API 见内置的上游[英文 README](esp_xiaozhi/README.md)和
[中文 README](esp_xiaozhi/README_CN.md)。这些上游文档应与本产品专用的安全说明
分开维护。
