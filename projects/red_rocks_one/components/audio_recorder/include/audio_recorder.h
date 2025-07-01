#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int pdm_clk_gpio;
    int pdm_data_gpio;
    uint32_t sample_rate;
    int buffer_count;
    int buffer_len;
} audio_config_t;

typedef struct {
    uint8_t riff[4];
    uint32_t file_size;
    uint8_t wave[4];
    uint8_t fmt[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint8_t data[4];
    uint32_t data_size;
} __attribute__((packed)) wav_header_t;

esp_err_t audio_recorder_init(const audio_config_t *config);

esp_err_t audio_recorder_deinit(void);

esp_err_t audio_recorder_start_recording(void);

esp_err_t audio_recorder_stop_recording(void);

esp_err_t audio_recorder_read_samples(int16_t *buffer, size_t buffer_size, size_t *bytes_read);

esp_err_t audio_recorder_create_wav_header(wav_header_t *header, uint32_t sample_rate, 
                                          uint16_t channels, uint32_t data_size);

bool audio_recorder_is_recording(void);

#ifdef __cplusplus
}
#endif