# Project Name

MimiClaw (`dev/seeed-xiao-s3`) - lzx 提交范围的部署文档。
默认最小可运行路径：`Zhipu API + Discord`。

适用提交范围：`8354932` ~ `46dff47`

本文只讲这批改进相关内容：
- Web UI + WebSocket + bench
- 默认 Zhipu + Discord，其他渠道可选
- OpenAI/Anthropic/WeCom/Telegram 可选配置
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

## 3) 填配置（默认：Zhipu + Discord）

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

#define MIMI_SECRET_DISCORD_TOKEN   "YOUR_DISCORD_BOT_TOKEN"
#define MIMI_SECRET_TG_TOKEN        ""          // 可选
#define MIMI_SECRET_WECOM_WEBHOOK   ""          // 可选
```

API key 获取：
- Zhipu: https://open.bigmodel.cn/
- OpenAI（可选）: https://platform.openai.com/
- Anthropic（可选）: https://console.anthropic.com/

避坑：
- `MODEL_PROVIDER` 和 `MODEL` 要匹配；不匹配会请求失败。
- Discord 通道未配置时，机器人不会在目标频道回消息。
- 只用 CLI 测试可不填 Discord/Telegram。
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
mimi> set_discord_token YOUR_DISCORD_BOT_TOKEN
mimi> discord_channel_add 123456789012345678
mimi> discord_channel_list
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

## D) 其他渠道（可选，自行开启）

```text
mimi> set_tg_token 123456:ABCDEF...
mimi> set_wecom_webhook https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=YOUR_KEY
```

## E) 价值场景（从“能跑”到“有用”）

1) 远程图像/音频采集 + 云端模型分析  
Discord 发一句自然语言命令，设备本地采集，再交给云端模型理解：

```text
mimi> tool_exec observe_scene '{"prompt":"门口现在有什么异常？"}'
mimi> tool_exec listen_and_transcribe '{"duration_ms":5000}'
```

2) 智能家居中控（下一步扩展）  
当前可先做“感知+理解”，再增加一个灯光/窗帘控制工具（HTTP/MQTT）就能闭环：

```text
# 当前阶段：先让设备判断环境
mimi> tool_exec observe_scene '{"prompt":"房间是否太暗？给出开灯建议。"}'
```

3) 宠物第一视角问答  
设备挂在宠物项圈或背包，远程问“它现在在看什么/听到什么”：

```text
mimi> tool_exec observe_scene '{"prompt":"这只宠物现在在做什么？"}'
mimi> tool_exec listen_and_transcribe '{"duration_ms":3000}'
```

4) 宠物运动分析（组合 IMU 的方向）  
结合三轴传感器数据可继续扩展：
- 热量估算（按活动强度和时长）
- 步态检测（异常步态预警）
- 行为识别（奔跑/静止/趴卧）

5) 远程巡检节点  
把它当成低功耗观察点，定时采集并推送摘要到 Discord：
- 是否有人/有声响
- 环境状态是否异常
- 需要时再人工介入

# How It Works

1. 启动后读取 `mimi_secrets.h`，再加载 NVS 里的运行时覆盖项。
2. Discord / CLI / WebSocket（以及可选 Telegram）消息进入同一个 agent loop。
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
