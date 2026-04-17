#include "media/xiao_s3_media.h"
#include "media/media_driver.h"
#include "mimi_config.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_camera.h"
#include "esp_camera_af.h"
#include "esp_psram.h"
#include "driver/i2s_pdm.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sensor.h"

static const char *TAG = "xiao_media";

/* XIAO ESP32S3 Sense camera pins (OV2640) */
#define CAM_PIN_XCLK   10
#define CAM_PIN_PCLK   13
#define CAM_PIN_VSYNC  38
#define CAM_PIN_HREF   47
#define CAM_PIN_SDA    40
#define CAM_PIN_SCL    39
#define CAM_PIN_D0     15  /* Y2 */
#define CAM_PIN_D1     17  /* Y3 */
#define CAM_PIN_D2     18  /* Y4 */
#define CAM_PIN_D3     16  /* Y5 */
#define CAM_PIN_D4     14  /* Y6 */
#define CAM_PIN_D5     12  /* Y7 */
#define CAM_PIN_D6     11  /* Y8 */
#define CAM_PIN_D7     48  /* Y9 */

/* XIAO ESP32S3 Sense PDM mic pins */
#define MIC_CLK_PIN    42
#define MIC_DATA_PIN   41

static bool s_cam_ready = false;
static bool s_mic_ready = false;
static volatile bool s_record_stop = false;
static int s_record_stop_gpio = -1; /* GPIO to poll for stop (active-high, stop when low) */

static bool camera_ready(void) { return s_cam_ready; }
static bool mic_ready(void) { return s_mic_ready; }

void media_xiao_s3_record_stop(void)       { s_record_stop = true; }
void media_xiao_s3_record_stop_reset(void) { s_record_stop = false; }
void media_xiao_s3_set_stop_gpio(int gpio_num) { s_record_stop_gpio = gpio_num; }

static esp_err_t camera_set_framesize_impl(int framesize)
{
    (void)framesize;
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t camera_set_quality_impl(int quality)
{
    (void)quality;
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t camera_get_status_impl(int *framesize, int *quality)
{
    if (!s_cam_ready) return ESP_ERR_INVALID_STATE;
    sensor_t *s = esp_camera_sensor_get();
    if (!s) return ESP_FAIL;
    if (framesize) *framesize = (int)s->status.framesize;
    if (quality) *quality = (int)s->status.quality;
    return ESP_OK;
}

static esp_err_t camera_capture_impl(const char *path, char *out_path, size_t out_size)
{
    if (!s_cam_ready) return ESP_ERR_INVALID_STATE;

#if defined(CONFIG_CAMERA_AF_SUPPORT) && CONFIG_CAMERA_AF_SUPPORT
    esp_camera_af_trigger();
    esp_camera_af_wait(2000);
#endif

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Failed to get camera frame buffer");
        return ESP_FAIL;
    }

    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", path);
        esp_camera_fb_return(fb);
        return ESP_FAIL;
    }

    size_t written = fwrite(fb->buf, 1, fb->len, f);
    fclose(f);
    esp_camera_fb_return(fb);

    if (written != fb->len) {
        ESP_LOGE(TAG, "Failed to write full image (%u/%u)", (unsigned)written, (unsigned)fb->len);
        return ESP_FAIL;
    }

    if (out_path && out_size > 0) {
        strncpy(out_path, path, out_size - 1);
        out_path[out_size - 1] = '\0';
    }
    ESP_LOGI(TAG, "Captured image: %s (%u bytes)", path, (unsigned)fb->len);
    return ESP_OK;
}

static esp_err_t write_wav_header(FILE *f, uint32_t sample_rate, uint16_t bits, uint16_t channels, uint32_t data_len)
{
    if (!f) return ESP_ERR_INVALID_ARG;
    uint32_t byte_rate = sample_rate * channels * bits / 8;
    uint16_t block_align = channels * bits / 8;
    uint32_t riff_size = 36 + data_len;

    fwrite("RIFF", 1, 4, f);
    fwrite(&riff_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);

    uint32_t subchunk1 = 16;
    uint16_t audio_format = 1; /* PCM */
    fwrite(&subchunk1, 4, 1, f);
    fwrite(&audio_format, 2, 1, f);
    fwrite(&channels, 2, 1, f);
    fwrite(&sample_rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits, 2, 1, f);

    fwrite("data", 1, 4, f);
    fwrite(&data_len, 4, 1, f);
    return ESP_OK;
}

static esp_err_t audio_record_impl(const char *path, int duration_ms, char *out_path, size_t out_size)
{
    const uint32_t sample_rate = 16000;
    const uint16_t bits = 16;
    const uint16_t channels = 1;
    const uint32_t bytes_per_sec = sample_rate * channels * (bits / 8);
    if (duration_ms <= 0) duration_ms = 3000;

    uint32_t target_bytes = (bytes_per_sec * (uint32_t)duration_ms) / 1000;
    if (target_bytes > (4 * 1024 * 1024)) target_bytes = 4 * 1024 * 1024;

    /* Write raw PCM to temp file, then assemble WAV after — SPIFFS can't seek to patch header */
    const char *tmp_path = "/spiffs/media/audio.raw";
    FILE *f = fopen(tmp_path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open temp audio file");
        return ESP_FAIL;
    }

    i2s_chan_handle_t rx_chan = NULL;
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    if (i2s_new_channel(&chan_cfg, NULL, &rx_chan) != ESP_OK) {
        fclose(f);
        ESP_LOGE(TAG, "Failed to create I2S channel");
        return ESP_FAIL;
    }

    i2s_pdm_rx_config_t rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = MIC_CLK_PIN,
            .din = MIC_DATA_PIN,
            .invert_flags = {0},
        },
    };
    if (i2s_channel_init_pdm_rx_mode(rx_chan, &rx_cfg) != ESP_OK) {
        i2s_del_channel(rx_chan);
        fclose(f);
        ESP_LOGE(TAG, "Failed to init PDM RX");
        return ESP_FAIL;
    }
    i2s_channel_enable(rx_chan);

    /* Discard first 200ms — PDM mic needs time to settle */
    {
        uint8_t tmp[2048];
        size_t dummy = 0;
        uint32_t discard = (sample_rate * 2 * 200) / 1000; /* 200ms worth of bytes */
        uint32_t discarded = 0;
        while (discarded < discard) {
            size_t rd = (discard - discarded < sizeof(tmp)) ? (discard - discarded) : sizeof(tmp);
            i2s_channel_read(rx_chan, tmp, rd, &dummy, pdMS_TO_TICKS(500));
            discarded += dummy ? dummy : rd;
        }
    }

    uint8_t *buf = calloc(1, 2048);
    if (!buf) {
        i2s_channel_disable(rx_chan);
        i2s_del_channel(rx_chan);
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    const int gain = 6;

    /* Do NOT clear s_record_stop here — caller resets it before starting watcher */
    uint32_t written = 0;
    int32_t peak_raw = 0;
    while (written < target_bytes) {
        if (s_record_stop) break;
        size_t bytes_read = 0;
        /* portMAX_DELAY — watcher task sets s_record_stop to unblock next iteration */
        esp_err_t err = i2s_channel_read(rx_chan, buf, 2048, &bytes_read, portMAX_DELAY);
        if (err != ESP_OK || bytes_read == 0) break;
        int16_t *samples = (int16_t *)buf;
        size_t num_samples = bytes_read / 2;
        for (size_t i = 0; i < num_samples; i++) {
            int32_t raw = samples[i];
            if (raw < 0 && -raw > peak_raw) peak_raw = -raw;
            if (raw > peak_raw) peak_raw = raw;
            int32_t amplified = raw * gain;
            if (amplified > 32767)  amplified = 32767;
            if (amplified < -32768) amplified = -32768;
            samples[i] = (int16_t)amplified;
        }
        fwrite(buf, 1, bytes_read, f);
        written += bytes_read;
    }
    ESP_LOGI(TAG, "Audio peak raw=%d post-gain=%d (gain=%d)", (int)peak_raw, (int)(peak_raw * gain > 32767 ? 32767 : peak_raw * gain), gain);

    free(buf);
    i2s_channel_disable(rx_chan);
    i2s_del_channel(rx_chan);
    fclose(f);

    /* Assemble final WAV: header + raw PCM */
    FILE *raw = fopen(tmp_path, "rb");
    FILE *wav = fopen(path, "wb");
    if (!raw || !wav) {
        if (raw) fclose(raw);
        if (wav) fclose(wav);
        ESP_LOGE(TAG, "Failed to open files for WAV assembly");
        return ESP_FAIL;
    }
    write_wav_header(wav, sample_rate, bits, channels, written);
    uint8_t *copy_buf = malloc(2048);
    if (copy_buf) {
        size_t n;
        while ((n = fread(copy_buf, 1, 2048, raw)) > 0) fwrite(copy_buf, 1, n, wav);
        free(copy_buf);
    }
    fclose(raw);
    fclose(wav);
    remove(tmp_path);

    s_mic_ready = true;
    if (out_path && out_size > 0) {
        strncpy(out_path, path, out_size - 1);
        out_path[out_size - 1] = '\0';
    }
    ESP_LOGI(TAG, "Recorded audio: %s (%u bytes)", path, (unsigned)written);
    return ESP_OK;
}

esp_err_t media_xiao_s3_init(void)
{
    camera_config_t config = {
        .ledc_channel = LEDC_CHANNEL_0,
        .ledc_timer = LEDC_TIMER_0,
        .pin_d0 = CAM_PIN_D0,
        .pin_d1 = CAM_PIN_D1,
        .pin_d2 = CAM_PIN_D2,
        .pin_d3 = CAM_PIN_D3,
        .pin_d4 = CAM_PIN_D4,
        .pin_d5 = CAM_PIN_D5,
        .pin_d6 = CAM_PIN_D6,
        .pin_d7 = CAM_PIN_D7,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_pclk = CAM_PIN_PCLK,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_sccb_sda = CAM_PIN_SDA,
        .pin_sccb_scl = CAM_PIN_SCL,
        .pin_pwdn = -1,
        .pin_reset = -1,
        .xclk_freq_hz = 20000000,
        .frame_size = FRAMESIZE_VGA,
        .pixel_format = PIXFORMAT_JPEG,
        .grab_mode = CAMERA_GRAB_LATEST,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .jpeg_quality = 15,
        .fb_count = 1,
    };

    if (!esp_psram_is_initialized()) {
        config.frame_size = FRAMESIZE_QVGA;
        config.fb_location = CAMERA_FB_IN_DRAM;
        config.fb_count = 1;
        config.jpeg_quality = 30;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Camera init failed: %s", esp_err_to_name(err));
        s_cam_ready = false;
    } else {
        sensor_t *s = esp_camera_sensor_get();
        if (s) {
            ESP_LOGI(TAG, "Camera sensor PID: 0x%02x", s->id.PID);
        }
        s_cam_ready = true;
    }

    media_driver_t driver = {
        .camera_capture = camera_capture_impl,
        .audio_record = audio_record_impl,
        .camera_set_framesize = camera_set_framesize_impl,
        .camera_set_quality = camera_set_quality_impl,
        .camera_get_status = camera_get_status_impl,
        .camera_ready = camera_ready,
        .mic_ready = mic_ready,
    };
    media_register_driver(&driver);

    if (s_cam_ready) {
        ESP_LOGI(TAG, "XIAO S3 camera ready");
    }
    ESP_LOGI(TAG, "XIAO S3 mic ready (PDM pins %d/%d)", MIC_CLK_PIN, MIC_DATA_PIN);
    return ESP_OK;
}
