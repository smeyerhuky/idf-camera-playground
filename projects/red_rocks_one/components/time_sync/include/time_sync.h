#pragma once

#include "esp_err.h"
#include <time.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *ssid;
    const char *password;
    const char *ntp_server;
    long gmt_offset_sec;
    int daylight_offset_sec;
    int max_retry;
} time_sync_config_t;

esp_err_t time_sync_init(const time_sync_config_t *config);

esp_err_t time_sync_connect_wifi(void);

esp_err_t time_sync_sync_time(void);

esp_err_t time_sync_disconnect_wifi(void);

esp_err_t time_sync_get_local_time(struct tm *timeinfo);

bool time_sync_is_time_set(void);

esp_err_t time_sync_format_timestamp(char *buffer, size_t buffer_size, const char *format);

esp_err_t time_sync_get_date_path(char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif