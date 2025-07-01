#include "avi_recorder.h"
#include "sdcard_module.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "avi_recorder";

static avi_recording_state_t s_recording_state = {0};
static bool s_initialized = false;

// AVI FOURCC codes
#define FOURCC_RIFF 0x46464952  // "RIFF"
#define FOURCC_AVI  0x20495641  // "AVI "
#define FOURCC_LIST 0x5453494C  // "LIST"
#define FOURCC_HDRL 0x6C726468  // "hdrl"
#define FOURCC_AVIH 0x68697661  // "avih"
#define FOURCC_STRL 0x6C727473  // "strl"
#define FOURCC_STRH 0x68727473  // "strh"
#define FOURCC_STRF 0x66727473  // "strf"
#define FOURCC_MOVI 0x69766F6D  // "movi"
#define FOURCC_VIDS 0x73646976  // "vids"
#define FOURCC_MJPG 0x47504A4D  // "MJPG"
#define FOURCC_00DC 0x63643030  // "00dc"

static esp_err_t write_avi_header(void)
{
    // Calculate total header size
    size_t header_size = sizeof(avi_header_t) + sizeof(avi_list_header_t) + 
                        sizeof(avi_main_header_t) + sizeof(avi_stream_list_t) +
                        sizeof(avi_stream_header_t) + sizeof(avi_stream_format_t) +
                        sizeof(avi_movie_list_t);
    
    uint8_t *header_buffer = malloc(header_size);
    if (!header_buffer) {
        return ESP_ERR_NO_MEM;
    }
    
    uint8_t *ptr = header_buffer;
    
    // AVI Header
    avi_header_t *avi_hdr = (avi_header_t*)ptr;
    memcpy(avi_hdr->riff, "RIFF", 4);
    avi_hdr->file_size = 0; // Will be updated when recording stops
    memcpy(avi_hdr->avi, "AVI ", 4);
    ptr += sizeof(avi_header_t);
    
    // Header List
    avi_list_header_t *list_hdr = (avi_list_header_t*)ptr;
    memcpy(list_hdr->list, "LIST", 4);
    list_hdr->list_size = sizeof(avi_main_header_t) + sizeof(avi_stream_list_t) + 
                         sizeof(avi_stream_header_t) + sizeof(avi_stream_format_t) + 4;
    memcpy(list_hdr->hdrl, "hdrl", 4);
    ptr += sizeof(avi_list_header_t);
    
    // Main AVI Header
    avi_main_header_t *main_hdr = (avi_main_header_t*)ptr;
    memcpy(main_hdr->avih, "avih", 4);
    main_hdr->avih_size = 56;
    main_hdr->us_per_frame = 1000000 / s_recording_state.fps;
    main_hdr->max_bytes_per_sec = s_recording_state.width * s_recording_state.height * 3 * s_recording_state.fps;
    main_hdr->padding = 0;
    main_hdr->flags = 0x00000810; // AVIF_HASINDEX | AVIF_WASCAPTUREFILE
    main_hdr->total_frames = 0; // Will be updated
    main_hdr->initial_frames = 0;
    main_hdr->streams = 1;
    main_hdr->suggested_buffer_size = s_recording_state.width * s_recording_state.height * 3;
    main_hdr->width = s_recording_state.width;
    main_hdr->height = s_recording_state.height;
    memset(main_hdr->reserved, 0, sizeof(main_hdr->reserved));
    ptr += sizeof(avi_main_header_t);
    
    // Stream List
    avi_stream_list_t *stream_list = (avi_stream_list_t*)ptr;
    memcpy(stream_list->list, "LIST", 4);
    stream_list->list_size = sizeof(avi_stream_header_t) + sizeof(avi_stream_format_t) + 8;
    memcpy(stream_list->strl, "strl", 4);
    ptr += sizeof(avi_stream_list_t);
    
    // Stream Header
    avi_stream_header_t *stream_hdr = (avi_stream_header_t*)ptr;
    memcpy(stream_hdr->strh, "strh", 4);
    stream_hdr->strh_size = 56;
    memcpy(stream_hdr->stream_type, "vids", 4);
    memcpy(stream_hdr->codec, "MJPG", 4);
    stream_hdr->flags = 0;
    stream_hdr->priority = 0;
    stream_hdr->language = 0;
    stream_hdr->initial_frames = 0;
    stream_hdr->scale = 1;
    stream_hdr->rate = s_recording_state.fps;
    stream_hdr->start = 0;
    stream_hdr->length = 0; // Will be updated
    stream_hdr->suggested_buffer_size = s_recording_state.width * s_recording_state.height * 3;
    stream_hdr->quality = 0xFFFFFFFF;
    stream_hdr->sample_size = 0;
    stream_hdr->frame.left = 0;
    stream_hdr->frame.top = 0;
    stream_hdr->frame.right = s_recording_state.width;
    stream_hdr->frame.bottom = s_recording_state.height;
    ptr += sizeof(avi_stream_header_t);
    
    // Stream Format
    avi_stream_format_t *stream_fmt = (avi_stream_format_t*)ptr;
    memcpy(stream_fmt->strf, "strf", 4);
    stream_fmt->strf_size = 40;
    stream_fmt->size = 40;
    stream_fmt->width = s_recording_state.width;
    stream_fmt->height = s_recording_state.height;
    stream_fmt->planes = 1;
    stream_fmt->bit_count = 24;
    stream_fmt->compression = FOURCC_MJPG;
    stream_fmt->size_image = s_recording_state.width * s_recording_state.height * 3;
    stream_fmt->x_pels_per_meter = 0;
    stream_fmt->y_pels_per_meter = 0;
    stream_fmt->clr_used = 0;
    stream_fmt->clr_important = 0;
    ptr += sizeof(avi_stream_format_t);
    
    // Movie List Header
    avi_movie_list_t *movie_list = (avi_movie_list_t*)ptr;
    memcpy(movie_list->list, "LIST", 4);
    movie_list->list_size = 0; // Will be updated
    memcpy(movie_list->movi, "movi", 4);
    
    esp_err_t ret = sdcard_module_write_file(s_recording_state.filename, header_buffer, header_size);
    free(header_buffer);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "AVI header written successfully");
    } else {
        ESP_LOGE(TAG, "Failed to write AVI header");
    }
    
    return ret;
}

static esp_err_t update_avi_header(void)
{
    // Read current file to update headers
    // This is a simplified version - in a full implementation you'd seek to specific positions
    ESP_LOGI(TAG, "Recording complete: %lu frames, %lu bytes", 
             s_recording_state.frame_count, s_recording_state.total_bytes);
    return ESP_OK;
}

esp_err_t avi_recorder_init(uint32_t width, uint32_t height, uint32_t fps)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "AVI recorder already initialized");
        return ESP_OK;
    }
    
    memset(&s_recording_state, 0, sizeof(s_recording_state));
    s_recording_state.width = width;
    s_recording_state.height = height;
    s_recording_state.fps = fps;
    s_recording_state.recording = false;
    
    s_initialized = true;
    ESP_LOGI(TAG, "AVI recorder initialized: %lux%lu @ %lu fps", width, height, fps);
    
    return ESP_OK;
}

esp_err_t avi_recorder_start(const char* filename)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "AVI recorder not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_recording_state.recording) {
        ESP_LOGW(TAG, "Already recording, stopping previous recording");
        avi_recorder_stop();
    }
    
    strncpy(s_recording_state.filename, filename, sizeof(s_recording_state.filename) - 1);
    s_recording_state.filename[sizeof(s_recording_state.filename) - 1] = '\0';
    
    s_recording_state.frame_count = 0;
    s_recording_state.total_bytes = 0;
    
    esp_err_t ret = write_avi_header();
    if (ret == ESP_OK) {
        s_recording_state.recording = true;
        ESP_LOGI(TAG, "Started AVI recording: %s", filename);
    }
    
    return ret;
}

esp_err_t avi_recorder_add_frame(const uint8_t* jpeg_data, size_t jpeg_size)
{
    if (!s_recording_state.recording) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Create chunk header for this frame
    avi_chunk_header_t chunk_header;
    memcpy(chunk_header.chunk_id, "00dc", 4);
    chunk_header.chunk_size = jpeg_size;
    
    // Write chunk header
    esp_err_t ret = sdcard_module_append_file(s_recording_state.filename, 
                                             &chunk_header, sizeof(chunk_header));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write chunk header");
        return ret;
    }
    
    // Write JPEG data
    ret = sdcard_module_append_file(s_recording_state.filename, jpeg_data, jpeg_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write frame data");
        return ret;
    }
    
    // Add padding if needed (AVI requires even-aligned chunks)
    if (jpeg_size % 2 != 0) {
        uint8_t padding = 0;
        sdcard_module_append_file(s_recording_state.filename, &padding, 1);
        s_recording_state.total_bytes += 1;
    }
    
    s_recording_state.frame_count++;
    s_recording_state.total_bytes += sizeof(chunk_header) + jpeg_size;
    
    if (s_recording_state.frame_count % 30 == 0) {
        ESP_LOGI(TAG, "Recorded %lu frames (%lu bytes)", 
                 s_recording_state.frame_count, s_recording_state.total_bytes);
    }
    
    return ESP_OK;
}

esp_err_t avi_recorder_stop(void)
{
    if (!s_recording_state.recording) {
        ESP_LOGW(TAG, "Not currently recording");
        return ESP_OK;
    }
    
    s_recording_state.recording = false;
    
    esp_err_t ret = update_avi_header();
    
    ESP_LOGI(TAG, "Stopped AVI recording: %s (%lu frames, %lu bytes)", 
             s_recording_state.filename, s_recording_state.frame_count, s_recording_state.total_bytes);
    
    return ret;
}

esp_err_t avi_recorder_deinit(void)
{
    if (s_recording_state.recording) {
        avi_recorder_stop();
    }
    
    s_initialized = false;
    memset(&s_recording_state, 0, sizeof(s_recording_state));
    
    ESP_LOGI(TAG, "AVI recorder deinitialized");
    return ESP_OK;
}

bool avi_recorder_is_recording(void)
{
    return s_recording_state.recording;
}

avi_recording_state_t* avi_recorder_get_state(void)
{
    return &s_recording_state;
}