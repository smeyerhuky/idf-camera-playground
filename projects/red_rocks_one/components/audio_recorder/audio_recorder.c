#include "audio_recorder.h"
#include "driver/i2s_pdm.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "audio_recorder";

static i2s_chan_handle_t s_rx_handle = NULL;
static bool s_is_recording = false;
static audio_config_t s_config = {0};

esp_err_t audio_recorder_init(const audio_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(&s_config, config, sizeof(audio_config_t));
    
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    
    esp_err_t ret = i2s_new_channel(&chan_cfg, NULL, &s_rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(config->sample_rate),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = config->pdm_clk_gpio,
            .din = config->pdm_data_gpio,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };
    
    ret = i2s_channel_init_pdm_rx_mode(s_rx_handle, &pdm_rx_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize PDM RX mode: %s", esp_err_to_name(ret));
        i2s_del_channel(s_rx_handle);
        s_rx_handle = NULL;
        return ret;
    }
    
    ESP_LOGI(TAG, "Audio recorder initialized with sample rate: %lu Hz", config->sample_rate);
    return ESP_OK;
}

esp_err_t audio_recorder_deinit(void)
{
    if (s_is_recording) {
        audio_recorder_stop_recording();
    }
    
    if (s_rx_handle) {
        esp_err_t ret = i2s_del_channel(s_rx_handle);
        s_rx_handle = NULL;
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t audio_recorder_start_recording(void)
{
    if (!s_rx_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_is_recording) {
        return ESP_OK;
    }
    
    esp_err_t ret = i2s_channel_enable(s_rx_handle);
    if (ret == ESP_OK) {
        s_is_recording = true;
        ESP_LOGI(TAG, "Audio recording started");
    } else {
        ESP_LOGE(TAG, "Failed to enable I2S channel: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t audio_recorder_stop_recording(void)
{
    if (!s_rx_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!s_is_recording) {
        return ESP_OK;
    }
    
    esp_err_t ret = i2s_channel_disable(s_rx_handle);
    if (ret == ESP_OK) {
        s_is_recording = false;
        ESP_LOGI(TAG, "Audio recording stopped");
    } else {
        ESP_LOGE(TAG, "Failed to disable I2S channel: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t audio_recorder_read_samples(int16_t *buffer, size_t buffer_size, size_t *bytes_read)
{
    if (!s_rx_handle || !buffer || !bytes_read) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_is_recording) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = i2s_channel_read(s_rx_handle, buffer, buffer_size, bytes_read, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read I2S data: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t audio_recorder_create_wav_header(wav_header_t *header, uint32_t sample_rate, 
                                          uint16_t channels, uint32_t data_size)
{
    if (!header) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(header->riff, "RIFF", 4);
    header->file_size = data_size + sizeof(wav_header_t) - 8;
    memcpy(header->wave, "WAVE", 4);
    memcpy(header->fmt, "fmt ", 4);
    header->fmt_size = 16;
    header->audio_format = 1;
    header->num_channels = channels;
    header->sample_rate = sample_rate;
    header->bits_per_sample = 16;
    header->byte_rate = sample_rate * channels * (header->bits_per_sample / 8);
    header->block_align = channels * (header->bits_per_sample / 8);
    memcpy(header->data, "data", 4);
    header->data_size = data_size;
    
    return ESP_OK;
}

bool audio_recorder_is_recording(void)
{
    return s_is_recording;
}