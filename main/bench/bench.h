#pragma once

#include "esp_err.h"
#include <stddef.h>

/**
 * Run a benchmark by name and write results to out buffer.
 * Supported names: all, cpu, mem, net, llm
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for unknown name
 */
esp_err_t bench_run(const char *name, char *out, size_t out_size);
