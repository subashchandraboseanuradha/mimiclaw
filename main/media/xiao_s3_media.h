#pragma once

#include "esp_err.h"

esp_err_t media_xiao_s3_init(void);

/* Signal the active audio recording to stop (call on button release). */
void media_xiao_s3_record_stop(void);
void media_xiao_s3_record_stop_reset(void);

/* Set a GPIO pin that the recording loop polls — recording stops when it goes low.
   Pass -1 to disable. Call before media_audio_record(). */
void media_xiao_s3_set_stop_gpio(int gpio_num);
