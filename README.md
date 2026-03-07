# Project Name

MimiClaw (branch: `dev/seeed-xiao-s3`) - deploy guide for `lzx` improvements only.

Scope in this doc: `8354932`, `a7b5798`, `127f49e`, `6cee685`, `d826dd9`, `e5c445e`, `feab0fb`, `46dff47`.

What is covered:
- Zhipu coding/vision/asr path + runtime model/provider switching
- WeCom webhook + Discord runtime config
- Built-in HTTP/WS UI and bench trigger path
- Media tools on XIAO ESP32S3 Sense (camera/mic) + camera tuning tool
- Stability/session/response behavior updates from the same commit range

# Quick Start

## 1) Clone and switch branch

```bash
git clone https://github.com/memovai/mimiclaw.git
cd mimiclaw
git switch dev/seeed-xiao-s3
```

## 2) Install ESP-IDF (one-time)

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

## 3) Configure secrets (minimum for lzx features)

```bash
cp main/mimi_secrets.h.example main/mimi_secrets.h
```

Edit `main/mimi_secrets.h`:

```c
#define MIMI_SECRET_WIFI_SSID       "YOUR_WIFI"
#define MIMI_SECRET_WIFI_PASS       "YOUR_WIFI_PASSWORD"
#define MIMI_SECRET_API_KEY         "YOUR_API_KEY"
#define MIMI_SECRET_MODEL_PROVIDER  "zhipu"     // zhipu | openai | anthropic
#define MIMI_SECRET_MODEL           "GLM-4-FlashX-250414"

#define MIMI_SECRET_TG_TOKEN        ""          // optional
#define MIMI_SECRET_DISCORD_TOKEN   ""          // optional
#define MIMI_SECRET_WECOM_WEBHOOK   ""          // optional
```

## 4) Build + flash

```bash
idf.py set-target esp32s3
idf.py fullclean
idf.py build
```

Find serial port:

```bash
# Linux
ls /dev/ttyACM* /dev/ttyUSB*

# macOS
ls /dev/cu.usb*
```

Flash and open monitor:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

## 5) Runtime overrides from CLI (`mimi>`)

```text
mimi> set_wifi YOUR_WIFI YOUR_WIFI_PASSWORD
mimi> set_api_key YOUR_API_KEY
mimi> set_model_provider zhipu
mimi> set_model GLM-4-FlashX-250414
mimi> config_show
mimi> restart
```

# Example

## A) Bench + Web UI/WS path

```text
mimi> bench all
mimi> bench llm
mimi> chat hello
```

Open UI:

```text
http://<device_ip>:18789/
```

WS endpoint used by UI:

```text
ws://<device_ip>:18789/ws
```

## B) Media tools (XIAO ESP32S3 Sense)

```text
mimi> tool_exec observe_scene '{"prompt":"Describe the image."}'
mimi> tool_exec listen_and_transcribe '{"duration_ms":3000}'
mimi> tool_exec device_cli '{"command":"cam_get"}'
mimi> tool_exec device_cli '{"command":"cam_set","framesize":"VGA","quality":15}'
```

## C) WeCom / Discord runtime config

```text
mimi> set_wecom_webhook https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=YOUR_KEY
mimi> set_discord_token YOUR_DISCORD_BOT_TOKEN
mimi> discord_channel_add 123456789012345678
mimi> discord_channel_list
```

# How It Works

1. Firmware boots, loads build-time secrets, then applies NVS runtime overrides.
2. Message channels (Telegram/Discord/CLI/WebSocket) feed the same agent loop.
3. Selected provider (`zhipu/openai/anthropic`) handles chat completions.
4. `observe_scene` and `listen_and_transcribe` call media driver + Zhipu vision/asr APIs.
5. Bench runs from CLI (`bench`) or WS `type=bench`; UI is served on port `18789`.

# Links

- `main/mimi_secrets.h.example`
- `main/cli/serial_cli.c`
- `main/tools/tool_registry.c`
- `main/tools/tool_media.c`
- `main/media/xiao_s3_media.c`
- `main/gateway/ws_server.c`
- `main/bench/bench.c`
- `main/mimi_config.h`
