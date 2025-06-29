#include "manifest_manager.h"
#include "sdcard_module.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "manifest";
static const char *MANIFEST_FILE = "timelapse_data/manifest.json";

static video_entry_t *s_video_entries = NULL;
static int s_video_count = 0;
static int s_video_capacity = 0;

esp_err_t manifest_init(void)
{
    s_video_count = 0;
    s_video_capacity = 16;
    s_video_entries = malloc(s_video_capacity * sizeof(video_entry_t));
    
    if (!s_video_entries) {
        ESP_LOGE(TAG, "Failed to allocate memory for manifest");
        return ESP_ERR_NO_MEM;
    }
    
    esp_err_t ret = manifest_load_from_sd();
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "No existing manifest found, starting fresh");
    }
    
    return ESP_OK;
}

esp_err_t manifest_add_video(const char *relative_path, const char *filename, 
                           size_t file_size, int duration_ms)
{
    if (!relative_path || !filename) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (s_video_count >= s_video_capacity) {
        s_video_capacity *= 2;
        video_entry_t *new_entries = realloc(s_video_entries, 
                                           s_video_capacity * sizeof(video_entry_t));
        if (!new_entries) {
            ESP_LOGE(TAG, "Failed to expand manifest capacity");
            return ESP_ERR_NO_MEM;
        }
        s_video_entries = new_entries;
    }
    
    video_entry_t *entry = &s_video_entries[s_video_count];
    strncpy(entry->filename, filename, sizeof(entry->filename) - 1);
    entry->filename[sizeof(entry->filename) - 1] = '\0';
    
    snprintf(entry->full_path, sizeof(entry->full_path), "%s/%s", relative_path, filename);
    
    time(&entry->timestamp);
    entry->file_size = file_size;
    entry->duration_ms = duration_ms;
    
    s_video_count++;
    
    ESP_LOGI(TAG, "Added video to manifest: %s (size: %zu bytes, duration: %d ms)", 
             entry->full_path, file_size, duration_ms);
    
    return ESP_OK;
}

esp_err_t manifest_save_to_sd(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *videos_array = cJSON_CreateArray();
    
    if (!root || !videos_array) {
        if (root) cJSON_Delete(root);
        if (videos_array) cJSON_Delete(videos_array);
        return ESP_ERR_NO_MEM;
    }
    
    cJSON_AddItemToObject(root, "videos", videos_array);
    cJSON_AddNumberToObject(root, "total_count", s_video_count);
    cJSON_AddNumberToObject(root, "created_timestamp", time(NULL));
    
    for (int i = 0; i < s_video_count; i++) {
        cJSON *video_obj = cJSON_CreateObject();
        if (!video_obj) continue;
        
        cJSON_AddStringToObject(video_obj, "filename", s_video_entries[i].filename);
        cJSON_AddStringToObject(video_obj, "path", s_video_entries[i].full_path);
        cJSON_AddNumberToObject(video_obj, "timestamp", s_video_entries[i].timestamp);
        cJSON_AddNumberToObject(video_obj, "size", s_video_entries[i].file_size);
        cJSON_AddNumberToObject(video_obj, "duration_ms", s_video_entries[i].duration_ms);
        
        cJSON_AddItemToArray(videos_array, video_obj);
    }
    
    char *json_string = cJSON_Print(root);
    cJSON_Delete(root);
    
    if (!json_string) {
        ESP_LOGE(TAG, "Failed to generate JSON");
        return ESP_ERR_NO_MEM;
    }
    
    esp_err_t ret = sdcard_module_write_file(MANIFEST_FILE, json_string, strlen(json_string));
    free(json_string);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Manifest saved with %d video entries", s_video_count);
    } else {
        ESP_LOGE(TAG, "Failed to save manifest to SD card");
    }
    
    return ret;
}

esp_err_t manifest_load_from_sd(void)
{
    size_t file_size = 32768;
    char *buffer = malloc(file_size);
    if (!buffer) {
        return ESP_ERR_NO_MEM;
    }
    
    esp_err_t ret = sdcard_module_read_file(MANIFEST_FILE, buffer, &file_size);
    if (ret != ESP_OK) {
        free(buffer);
        return ret;
    }
    
    cJSON *root = cJSON_Parse(buffer);
    free(buffer);
    
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse manifest JSON");
        return ESP_ERR_INVALID_ARG;
    }
    
    cJSON *videos_array = cJSON_GetObjectItem(root, "videos");
    if (!cJSON_IsArray(videos_array)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    
    int array_size = cJSON_GetArraySize(videos_array);
    s_video_count = 0;
    
    for (int i = 0; i < array_size && s_video_count < s_video_capacity; i++) {
        cJSON *video_obj = cJSON_GetArrayItem(videos_array, i);
        if (!cJSON_IsObject(video_obj)) continue;
        
        cJSON *filename = cJSON_GetObjectItem(video_obj, "filename");
        cJSON *path = cJSON_GetObjectItem(video_obj, "path");
        cJSON *timestamp = cJSON_GetObjectItem(video_obj, "timestamp");
        cJSON *size = cJSON_GetObjectItem(video_obj, "size");
        cJSON *duration = cJSON_GetObjectItem(video_obj, "duration_ms");
        
        if (!cJSON_IsString(filename) || !cJSON_IsString(path) || 
            !cJSON_IsNumber(timestamp) || !cJSON_IsNumber(size) ||
            !cJSON_IsNumber(duration)) {
            continue;
        }
        
        video_entry_t *entry = &s_video_entries[s_video_count];
        strncpy(entry->filename, filename->valuestring, sizeof(entry->filename) - 1);
        strncpy(entry->full_path, path->valuestring, sizeof(entry->full_path) - 1);
        entry->timestamp = (time_t)timestamp->valuedouble;
        entry->file_size = (size_t)size->valuedouble;
        entry->duration_ms = (int)duration->valuedouble;
        
        s_video_count++;
    }
    
    cJSON_Delete(root);
    ESP_LOGI(TAG, "Loaded %d video entries from manifest", s_video_count);
    return ESP_OK;
}

int manifest_get_video_count(void)
{
    return s_video_count;
}

esp_err_t manifest_get_video_entry(int index, video_entry_t *entry)
{
    if (!entry || index < 0 || index >= s_video_count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(entry, &s_video_entries[index], sizeof(video_entry_t));
    return ESP_OK;
}

esp_err_t manifest_cleanup_old_entries(int max_days)
{
    time_t now;
    time(&now);
    time_t cutoff = now - (max_days * 24 * 60 * 60);
    
    int new_count = 0;
    for (int i = 0; i < s_video_count; i++) {
        if (s_video_entries[i].timestamp >= cutoff) {
            if (new_count != i) {
                memcpy(&s_video_entries[new_count], &s_video_entries[i], sizeof(video_entry_t));
            }
            new_count++;
        } else {
            ESP_LOGI(TAG, "Removing old entry: %s", s_video_entries[i].full_path);
        }
    }
    
    s_video_count = new_count;
    ESP_LOGI(TAG, "Cleaned up manifest, %d entries remaining", s_video_count);
    return ESP_OK;
}