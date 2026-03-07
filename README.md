# Project Name

MimiClaw (`dev/seeed-xiao-s3`) - lzx 提交范围的部署文档。

适用提交范围：`8354932` ~ `46dff47`

本文只讲这批改进相关内容：
- Web UI + WebSocket + bench
- Zhipu/OpenAI/Anthropic 切换
- WeCom/Discord 运行时配置
- XIAO ESP32S3 Sense 媒体工具（拍照/录音/识别）

# Quick Start

## 0) 准备硬件（从零开始）

必需：
- 开发板：Seeed XIAO ESP32S3
  - 购买链接: https://www.seeedstudio.com/XIAO-ESP32S3-p-5627.html
  - Wiki: https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/
- USB-C 数据线（不是仅充电线）
- 2.4GHz Wi-Fi

如果你要用拍照/录音工具：
- 建议 XIAO ESP32S3 Sense（带摄像头/麦克风链路）
- 普通 XIAO ESP32S3 可能出现 `camera/audio not supported`

## 1) 拉代码并切分支

```bash
git clone https://github.com/memovai/mimiclaw.git
cd mimiclaw
git switch dev/seeed-xiao-s3
```

## 2) 安装 ESP-IDF（一次）

Ubuntu:

```bash
./scripts/setup_idf_ubuntu.sh
. "$HOME/.espressif/esp-idf-v5.5.2/export.sh"
```

macOS:

```bash
./scripts/setup_idf_macos.sh
. "$HOME/.espressif/esp-idf-v5.5.2/export.sh"
```

## 3) 填配置（API key / token）

```bash
cp main/mimi_secrets.h.example main/mimi_secrets.h
```

编辑 `main/mimi_secrets.h`（最小可跑配置）：

```c
#define MIMI_SECRET_WIFI_SSID       "YOUR_WIFI"
#define MIMI_SECRET_WIFI_PASS       "YOUR_WIFI_PASSWORD"

#define MIMI_SECRET_API_KEY         "YOUR_API_KEY"
#define MIMI_SECRET_MODEL_PROVIDER  "zhipu"     // zhipu | openai | anthropic
#define MIMI_SECRET_MODEL           "GLM-4-FlashX-250414"

#define MIMI_SECRET_TG_TOKEN        ""          // 可选
#define MIMI_SECRET_DISCORD_TOKEN   ""          // 可选
#define MIMI_SECRET_WECOM_WEBHOOK   ""          // 可选
```

API key 获取：
- Zhipu: https://open.bigmodel.cn/
- OpenAI: https://platform.openai.com/
- Anthropic: https://console.anthropic.com/

避坑：
- `MODEL_PROVIDER` 和 `MODEL` 要匹配；不匹配会请求失败。
- 只用 CLI 测试可不填 Telegram/Discord。
- 网络受限时，先在 CLI 里设置代理：`set_proxy HOST PORT [http|socks5]`。

## 4) 编译烧录

```bash
idf.py set-target esp32s3
idf.py fullclean
idf.py build
```

查串口：

```bash
# Linux
ls /dev/ttyACM* /dev/ttyUSB*

# macOS
ls /dev/cu.usb*
```

烧录并看日志：

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

避坑：
- 有双 Type-C 口的板子，烧录用原生 USB/JTAG 口。
- 不能输入命令时，切到 UART/COM 口开 monitor（看板子丝印）。

## 5) 运行时覆盖（不改代码，直接 CLI）

```text
mimi> set_wifi YOUR_WIFI YOUR_WIFI_PASSWORD
mimi> set_api_key YOUR_API_KEY
mimi> set_model_provider zhipu
mimi> set_model GLM-4-FlashX-250414
mimi> config_show
mimi> restart
```

# Example

## A) 先验证基础链路

```text
mimi> wifi_status
mimi> chat hello
mimi> bench all
mimi> bench llm
```

## B) 验证 Web UI / WS

```text
http://<device_ip>:18789/
ws://<device_ip>:18789/ws
```

## C) 验证媒体工具（Sense）

```text
mimi> tool_exec observe_scene '{"prompt":"Describe the image."}'
mimi> tool_exec listen_and_transcribe '{"duration_ms":3000}'
mimi> tool_exec device_cli '{"command":"cam_get"}'
mimi> tool_exec device_cli '{"command":"cam_set","framesize":"VGA","quality":15}'
```

## D) 验证渠道配置（可选）

```text
mimi> set_wecom_webhook https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=YOUR_KEY
mimi> set_discord_token YOUR_DISCORD_BOT_TOKEN
mimi> discord_channel_add 123456789012345678
mimi> discord_channel_list
```

# How It Works

1. 启动后读取 `mimi_secrets.h`，再加载 NVS 里的运行时覆盖项。
2. Telegram / Discord / CLI / WebSocket 消息进入同一个 agent loop。
3. `set_model_provider` 决定走 `zhipu` / `openai` / `anthropic`。
4. `observe_scene` 和 `listen_and_transcribe` 走媒体驱动 + 多模态 API。
5. UI 与 WS 在 `18789`；bench 可从 CLI 或 WS 触发。

# Links

- Seeed XIAO ESP32S3 购买: https://www.seeedstudio.com/XIAO-ESP32S3-p-5627.html
- Seeed Wiki (入门): https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/
- Secrets 模板: `main/mimi_secrets.h.example`
- CLI 命令定义: `main/cli/serial_cli.c`
- 工具注册: `main/tools/tool_registry.c`
- 媒体工具实现: `main/tools/tool_media.c`
- XIAO S3 媒体驱动: `main/media/xiao_s3_media.c`
- WS/UI 服务: `main/gateway/ws_server.c`
- Bench 实现: `main/bench/bench.c`
