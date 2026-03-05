#include "media/media_driver.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "media";

static esp_err_t stub_camera_capture(const char *path, char *out_path, size_t out_size)
{
    (void)path;
    if (out_path && out_size > 0) out_path[0] = '\0';
    ESP_LOGW(TAG, "Camera capture not supported in this build");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t stub_audio_record(const char *path, int duration_ms, char *out_path, size_t out_size)
{
    (void)path;
    (void)duration_ms;
    if (out_path && out_size > 0) out_path[0] = '\0';
    ESP_LOGW(TAG, "Audio record not supported in this build");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t stub_camera_set_framesize(int framesize)
{
    (void)framesize;
    ESP_LOGW(TAG, "Camera control not supported in this build");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t stub_camera_set_quality(int quality)
{
    (void)quality;
    ESP_LOGW(TAG, "Camera control not supported in this build");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t stub_camera_get_status(int *framesize, int *quality)
{
    if (framesize) *framesize = 0;
    if (quality) *quality = 0;
    ESP_LOGW(TAG, "Camera control not supported in this build");
    return ESP_ERR_NOT_SUPPORTED;
}

static bool stub_false(void) { return false; }

static media_driver_t s_driver = {
    .camera_capture = stub_camera_capture,
    .audio_record = stub_audio_record,
    .camera_set_framesize = stub_camera_set_framesize,
    .camera_set_quality = stub_camera_set_quality,
    .camera_get_status = stub_camera_get_status,
    .camera_ready = stub_false,
    .mic_ready = stub_false,
};

#if CONFIG_IDF_TARGET_ESP32S3
#include "media/xiao_s3_media.h"
#endif

esp_err_t media_init(void)
{
    ESP_LOGI(TAG, "Media subsystem initialized (stub driver)");
#if CONFIG_IDF_TARGET_ESP32S3
    media_xiao_s3_init();
#endif
    return ESP_OK;
}

esp_err_t media_register_driver(const media_driver_t *driver)
{
    if (!driver) return ESP_ERR_INVALID_ARG;
    s_driver = *driver;
    ESP_LOGI(TAG, "Media driver registered");
    return ESP_OK;
}

esp_err_t media_camera_capture(const char *path, char *out_path, size_t out_size)
{
    if (!s_driver.camera_capture) return ESP_ERR_NOT_SUPPORTED;
    return s_driver.camera_capture(path, out_path, out_size);
}

esp_err_t media_audio_record(const char *path, int duration_ms, char *out_path, size_t out_size)
{
    if (!s_driver.audio_record) return ESP_ERR_NOT_SUPPORTED;
    return s_driver.audio_record(path, duration_ms, out_path, out_size);
}

esp_err_t media_camera_set_framesize(int framesize)
{
    if (!s_driver.camera_set_framesize) return ESP_ERR_NOT_SUPPORTED;
    return s_driver.camera_set_framesize(framesize);
}

esp_err_t media_camera_set_quality(int quality)
{
    if (!s_driver.camera_set_quality) return ESP_ERR_NOT_SUPPORTED;
    return s_driver.camera_set_quality(quality);
}

esp_err_t media_camera_get_status(int *framesize, int *quality)
{
    if (!s_driver.camera_get_status) return ESP_ERR_NOT_SUPPORTED;
    return s_driver.camera_get_status(framesize, quality);
}

bool media_camera_ready(void)
{
    return s_driver.camera_ready ? s_driver.camera_ready() : false;
}

bool media_mic_ready(void)
{
    return s_driver.mic_ready ? s_driver.mic_ready() : false;
}
