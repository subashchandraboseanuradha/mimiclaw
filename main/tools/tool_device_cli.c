#include "tools/tool_device_cli.h"
#include "media/media_driver.h"
#include "media/camera_settings.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cJSON.h"

static const char *json_get_string(cJSON *root, const char *key)
{
    cJSON *item = cJSON_GetObjectItem(root, key);
    if (item && cJSON_IsString(item)) return item->valuestring;
    return NULL;
}

static int json_get_int(cJSON *root, const char *key, int def)
{
    cJSON *item = cJSON_GetObjectItem(root, key);
    if (item && cJSON_IsNumber(item)) return item->valueint;
    return def;
}

static void write_error(char *output, size_t output_size, const char *msg)
{
    snprintf(output, output_size, "{\"ok\":false,\"error\":\"%s\"}", msg ? msg : "error");
}

esp_err_t tool_device_cli_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = input_json ? cJSON_Parse(input_json) : NULL;
    const char *command = root ? json_get_string(root, "command") : NULL;
    const char *framesize_str = root ? json_get_string(root, "framesize") : NULL;
    int quality = root ? json_get_int(root, "quality", -1) : -1;

    if (!command || !command[0]) {
        command = "cam_get";
    }

    if (strcmp(command, "cam_get") == 0) {
        int framesize = 0;
        int q = 0;
        esp_err_t err = media_camera_get_status(&framesize, &q);
        if (err != ESP_OK) {
            write_error(output, output_size, "camera not ready");
            if (root) cJSON_Delete(root);
            return err;
        }
        snprintf(output, output_size,
                 "{\"ok\":true,\"framesize\":\"%s\",\"quality\":%d}",
                 media_framesize_name(framesize), q);
        if (root) cJSON_Delete(root);
        return ESP_OK;
    }

    if (strcmp(command, "cam_set") != 0) {
        write_error(output, output_size, "unsupported command");
        if (root) cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    (void)framesize_str;
    (void)quality;
    write_error(output, output_size, "cam_set disabled; camera settings are fixed to preserve image quality");
    if (root) cJSON_Delete(root);
    return ESP_ERR_NOT_SUPPORTED;
}
