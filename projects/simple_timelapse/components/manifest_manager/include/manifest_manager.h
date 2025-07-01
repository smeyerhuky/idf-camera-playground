#pragma once

#include "esp_err.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char filename[64];
    char full_path[128];
    time_t timestamp;
    size_t file_size;
    int duration_ms;
} video_entry_t;

esp_err_t manifest_init(void);

esp_err_t manifest_add_video(const char *relative_path, const char *filename, 
                           size_t file_size, int duration_ms);

esp_err_t manifest_save_to_sd(void);

esp_err_t manifest_load_from_sd(void);

int manifest_get_video_count(void);

esp_err_t manifest_get_video_entry(int index, video_entry_t *entry);

esp_err_t manifest_cleanup_old_entries(int max_days);

#ifdef __cplusplus
}
#endif