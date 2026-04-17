# MimiClaw — Subash's Custom Build

Forked from [luoluoter/mimiclaw](https://github.com/luoluoter/mimiclaw) · branch `main` · commit `8390390`

This build targets the **Seeed XIAO ESP32S3 Sense** with an external TTP223 touch sensor.

---

## Hardware

| Part | Details |
|------|---------|
| Board | Seeed XIAO ESP32S3 Sense |
| Camera | OV5640 (on-board, autofocus) |
| Microphone | PDM mic (on-board, pins 41/42) |
| Touch sensor | TTP223 capacitive module → GPIO 4 (active-high) |

> The built-in boot button was broken, so an external TTP223 was wired to GPIO 4.

---

## What changed from upstream

### 1. LLM — OpenRouter + Gemini provider added (`main/llm/llm_proxy.c`)

Upstream only supported Zhipu, Anthropic, and OpenAI.

Added:
- **OpenRouter** provider (OpenAI-compatible format, free models available)
- **Gemini** provider (Google's native API — different request/response format, key in URL)

Set in `mimi_secrets.h`:
```c
#define MIMI_SECRET_MODEL           "google/gemini-2.5-flash"
#define MIMI_SECRET_MODEL_PROVIDER  "openrouter"
#define MIMI_SECRET_API_KEY         "your_openrouter_key"
```

### 2. ASR — Deepgram Nova-3 added (`main/tools/tool_media.c`)

Upstream used Zhipu ASR. Added **Deepgram** (much better accuracy for English).

Deepgram is auto-selected when `MIMI_SECRET_DEEPGRAM_KEY` is set.  
Uses raw WAV POST (`Authorization: Token <key>`) instead of multipart form.

```c
#define MIMI_SECRET_DEEPGRAM_KEY    "your_deepgram_key"
```

Free tier available at [deepgram.com](https://deepgram.com).

### 3. Web search — Tavily added (`main/tools/tool_web_search.c`)

Upstream used Brave Search. Added **Tavily** (1000 free searches/month, no credit card).

Auto-selected when key starts with `tvly-`.

```c
#define MIMI_SECRET_SEARCH_KEY      "tvly-your_key"
```

Free tier at [tavily.com](https://tavily.com).

### 4. Touch-to-talk — GPIO 4 TTP223 sensor (`main/mimi.c`)

- **Single tap** → captures photo → describes scene via vision API → sends to Telegram
- **Hold** → starts recording immediately while held → releases stops recording → Deepgram transcribes → AI responds

Recording starts the moment the long-press threshold (500ms) is reached. A watcher task on core 0 polls GPIO 4 and sets the stop flag when the finger lifts.

### 5. Camera autofocus (`main/media/xiao_s3_media.c`, `sdkconfig.defaults.esp32s3`)

Added `esp_camera_af_trigger()` / `esp_camera_af_wait(2000)` before each capture.  
Enabled via `CONFIG_CAMERA_AF_SUPPORT=y` in sdkconfig.

### 6. PDM mic gain (`main/media/xiao_s3_media.c`)

The XIAO S3 Sense PDM mic outputs a low-level signal. Added 6× software gain on 16-bit samples with clipping protection.

### 7. WAV header fix for SPIFFS (`main/media/xiao_s3_media.c`)

SPIFFS does not support `fseek` to overwrite data. Upstream wrote a placeholder header then tried to patch it — this left `data_len=0` in the WAV header, causing ASR to fail.

Fixed by writing raw PCM to a temp file first, then assembling the final WAV with the correct header after recording ends.

### 8. Telegram polling timeout (`main/telegram/telegram_bot.c`)

Reduced long-poll timeout from 30s to 5s to avoid holding the network mutex too long and starving LLM/vision calls.

### 9. Silence detection before ASR call

Reads 512 samples from the recorded WAV before sending to Deepgram. If peak amplitude < 200 (silence), skips the API call and sends feedback to Telegram instead.

---

## Quick start

### Requirements

- ESP-IDF **v5.5.2** (v6.0 is not compatible — `json` component was renamed to `cjson`)
- Seeed XIAO ESP32S3 Sense board

### Setup

```bash
git clone https://github.com/subashchandraboseanuradha/miniclaw.git
cd miniclaw
cp main/mimi_secrets.h.example main/mimi_secrets.h
# Edit mimi_secrets.h with your keys
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor   # Windows: COM3
```

### Minimum config to get started

```c
// mimi_secrets.h
#define MIMI_SECRET_WIFI_SSID           "your_wifi"
#define MIMI_SECRET_WIFI_PASS           "your_password"
#define MIMI_SECRET_TG_TOKEN            "your_telegram_bot_token"
#define MIMI_SECRET_API_KEY             "your_openrouter_key"
#define MIMI_SECRET_MODEL               "google/gemini-2.5-flash"
#define MIMI_SECRET_MODEL_PROVIDER      "openrouter"
#define MIMI_SECRET_DEEPGRAM_KEY        "your_deepgram_key"
#define MIMI_SECRET_SEARCH_KEY          "tvly-your_tavily_key"
```

### Free API services used

| Service | Free tier | Purpose |
|---------|-----------|---------|
| [OpenRouter](https://openrouter.ai) | Free models (Gemini 2.5 Flash etc.) | LLM |
| [Deepgram](https://deepgram.com) | Free tier | Speech-to-text |
| [Tavily](https://tavily.com) | 1000 searches/month | Web search |
| [Telegram](https://t.me/BotFather) | Free | Chat interface |

---

## Touch sensor wiring

```
TTP223 module
  VCC → 3.3V
  GND → GND
  OUT → GPIO 4
```

The TTP223 output is active-high (HIGH when touched).
