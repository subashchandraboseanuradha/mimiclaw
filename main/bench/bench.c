#include "bench/bench.h"
#include "mimi_config.h"
#include "llm/llm_proxy.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "lwip/netdb.h"
#include "cJSON.h"
#include <stdarg.h>

static const char *TAG = "bench";

static size_t append_line(char *out, size_t out_size, size_t off, const char *fmt, ...)
{
    if (off >= out_size) return off;
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(out + off, out_size - off, fmt, args);
    va_end(args);
    if (n < 0) return off;
    size_t added = (size_t)n;
    if (off + added >= out_size) {
        out[out_size - 1] = '\0';
        return out_size;
    }
    return off + added;
}

static void bench_cpu(char *out, size_t out_size, size_t *off)
{
    int64_t start = esp_timer_get_time();
    volatile uint32_t acc = 0;
    for (uint32_t i = 0; i < 1000000; i++) {
        acc += (i ^ (i >> 3)) + 7;
    }
    int64_t end = esp_timer_get_time();
    double ms = (end - start) / 1000.0;
    *off = append_line(out, out_size, *off, "[bench] cpu.loop_1e6_ms=%.2f acc=%u\n", ms, acc);
}

static void bench_mem(char *out, size_t out_size, size_t *off)
{
    size_t internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t total = esp_get_free_heap_size();
    *off = append_line(out, out_size, *off,
        "[bench] mem.internal_free=%u mem.psram_free=%u mem.total_free=%u\n",
        (unsigned)internal, (unsigned)psram, (unsigned)total);

    void *p = heap_caps_malloc(64 * 1024, MALLOC_CAP_SPIRAM);
    *off = append_line(out, out_size, *off,
        "[bench] mem.alloc_psram_64k=%s\n", p ? "ok" : "fail");
    free(p);
}

static void bench_net(char *out, size_t out_size, size_t *off)
{
    const char *host = "open.bigmodel.cn";
    struct addrinfo hints = {0};
    struct addrinfo *res = NULL;
    int64_t start = esp_timer_get_time();
    int rc = getaddrinfo(host, "443", &hints, &res);
    int64_t end = esp_timer_get_time();
    double dns_ms = (end - start) / 1000.0;
    *off = append_line(out, out_size, *off,
        "[bench] net.dns_ms=%.2f rc=%d\n", dns_ms, rc);
    if (res) freeaddrinfo(res);

    const char *url = "https://open.bigmodel.cn";
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 15000,
        .buffer_size = 1024,
        .buffer_size_tx = 1024,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        *off = append_line(out, out_size, *off, "[bench] net.http_init=fail\n");
        return;
    }
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    start = esp_timer_get_time();
    esp_err_t err = esp_http_client_perform(client);
    end = esp_timer_get_time();
    int status = esp_http_client_get_status_code(client);
    double http_ms = (end - start) / 1000.0;
    *off = append_line(out, out_size, *off,
        "[bench] net.http_ms=%.2f err=%s status=%d\n",
        http_ms, esp_err_to_name(err), status);
    esp_http_client_cleanup(client);
}

static void bench_llm(char *out, size_t out_size, size_t *off)
{
    cJSON *messages = cJSON_CreateArray();
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "role", "user");
    cJSON_AddStringToObject(msg, "content", "ping");
    cJSON_AddItemToArray(messages, msg);

    llm_response_t resp;
    int64_t start = esp_timer_get_time();
    esp_err_t err = llm_chat_tools("You are a benchmark.", messages, NULL, &resp);
    int64_t end = esp_timer_get_time();
    double ms = (end - start) / 1000.0;

    cJSON_Delete(messages);

    if (err == ESP_OK) {
        *off = append_line(out, out_size, *off,
            "[bench] llm.roundtrip_ms=%.2f resp_bytes=%u\n",
            ms, (unsigned)resp.text_len);
        llm_response_free(&resp);
    } else {
        *off = append_line(out, out_size, *off,
            "[bench] llm.roundtrip_ms=%.2f err=%s\n",
            ms, esp_err_to_name(err));
    }
}

esp_err_t bench_run(const char *name, char *out, size_t out_size)
{
    if (!out || out_size == 0) return ESP_ERR_INVALID_ARG;
    out[0] = '\0';
    size_t off = 0;

    if (!name || strcmp(name, "all") == 0) {
        bench_cpu(out, out_size, &off);
        bench_mem(out, out_size, &off);
        bench_net(out, out_size, &off);
        bench_llm(out, out_size, &off);
        ESP_LOGI(TAG, "Benchmark all done");
        return ESP_OK;
    }
    if (strcmp(name, "cpu") == 0) {
        bench_cpu(out, out_size, &off);
        return ESP_OK;
    }
    if (strcmp(name, "mem") == 0) {
        bench_mem(out, out_size, &off);
        return ESP_OK;
    }
    if (strcmp(name, "net") == 0) {
        bench_net(out, out_size, &off);
        return ESP_OK;
    }
    if (strcmp(name, "llm") == 0) {
        bench_llm(out, out_size, &off);
        return ESP_OK;
    }

    append_line(out, out_size, off, "[bench] unknown benchmark: %s\n", name);
    return ESP_ERR_INVALID_ARG;
}
