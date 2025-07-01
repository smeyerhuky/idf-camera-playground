#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// AVI file header structures
typedef struct {
    char riff[4];           // "RIFF"
    uint32_t file_size;     // Total file size - 8
    char avi[4];            // "AVI "
} __attribute__((packed)) avi_header_t;

typedef struct {
    char list[4];           // "LIST"
    uint32_t list_size;     // Size of list
    char hdrl[4];           // "hdrl"
} __attribute__((packed)) avi_list_header_t;

typedef struct {
    char avih[4];           // "avih"
    uint32_t avih_size;     // Size of avih chunk (56)
    uint32_t us_per_frame;  // Microseconds per frame
    uint32_t max_bytes_per_sec;  // Maximum data rate
    uint32_t padding;       // Padding granularity
    uint32_t flags;         // AVI flags
    uint32_t total_frames;  // Total frames in file
    uint32_t initial_frames; // Initial frames
    uint32_t streams;       // Number of streams
    uint32_t suggested_buffer_size; // Suggested buffer size
    uint32_t width;         // Width of video
    uint32_t height;        // Height of video
    uint32_t reserved[4];   // Reserved fields
} __attribute__((packed)) avi_main_header_t;

typedef struct {
    char list[4];           // "LIST"
    uint32_t list_size;     // Size of list
    char strl[4];           // "strl"
} __attribute__((packed)) avi_stream_list_t;

typedef struct {
    char strh[4];           // "strh"
    uint32_t strh_size;     // Size of strh chunk (56)
    char stream_type[4];    // "vids" for video
    char codec[4];          // "MJPG"
    uint32_t flags;         // Stream flags
    uint16_t priority;      // Priority
    uint16_t language;      // Language
    uint32_t initial_frames; // Initial frames
    uint32_t scale;         // Time scale
    uint32_t rate;          // Sample rate
    uint32_t start;         // Start time
    uint32_t length;        // Length in scale units
    uint32_t suggested_buffer_size; // Suggested buffer size
    uint32_t quality;       // Quality
    uint32_t sample_size;   // Sample size
    struct {
        int16_t left;
        int16_t top;
        int16_t right;
        int16_t bottom;
    } frame;
} __attribute__((packed)) avi_stream_header_t;

typedef struct {
    char strf[4];           // "strf"
    uint32_t strf_size;     // Size of strf chunk (40)
    uint32_t size;          // Size of this structure
    int32_t width;          // Width of video
    int32_t height;         // Height of video
    uint16_t planes;        // Number of planes
    uint16_t bit_count;     // Bits per pixel
    uint32_t compression;   // Compression type
    uint32_t size_image;    // Size of image
    int32_t x_pels_per_meter; // Horizontal resolution
    int32_t y_pels_per_meter; // Vertical resolution
    uint32_t clr_used;      // Colors used
    uint32_t clr_important; // Important colors
} __attribute__((packed)) avi_stream_format_t;

typedef struct {
    char list[4];           // "LIST"
    uint32_t list_size;     // Size of list
    char movi[4];           // "movi"
} __attribute__((packed)) avi_movie_list_t;

typedef struct {
    char chunk_id[4];       // "00dc" for frame data
    uint32_t chunk_size;    // Size of chunk data
} __attribute__((packed)) avi_chunk_header_t;

typedef struct {
    uint32_t frame_count;
    uint32_t total_bytes;
    uint32_t fps;
    uint32_t width;
    uint32_t height;
    bool recording;
    char filename[128];
} avi_recording_state_t;

// Public functions
esp_err_t avi_recorder_init(uint32_t width, uint32_t height, uint32_t fps);
esp_err_t avi_recorder_start(const char* filename);
esp_err_t avi_recorder_add_frame(const uint8_t* jpeg_data, size_t jpeg_size);
esp_err_t avi_recorder_stop(void);
esp_err_t avi_recorder_deinit(void);
bool avi_recorder_is_recording(void);
avi_recording_state_t* avi_recorder_get_state(void);

#ifdef __cplusplus
}
#endif