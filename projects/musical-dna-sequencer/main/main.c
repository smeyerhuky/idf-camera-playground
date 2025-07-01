#include <stdio.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_dsp.h"
#include "esp_spiffs.h"

static const char *TAG = "musical-dna-sequencer";

// Audio Configuration
#define I2S_SAMPLE_RATE     44100
#define I2S_NUM             I2S_NUM_0
#define I2S_BCK_PIN         26
#define I2S_WS_PIN          25
#define I2S_DATA_PIN        27
#define I2S_BIT_WIDTH       32

// FFT Configuration
#define N_SAMPLES           1024
#define N_FFT               512
#define FREQ_BINS           (N_FFT / 2)
#define SAMPLE_WINDOW       23    // 1024 samples @ 44.1kHz = ~23ms

// Music Analysis Parameters
#define BEAT_HISTORY        8
#define CHORD_TEMPLATES     24    // 12 major + 12 minor
#define GENRE_FEATURES      13    // MFCC-like features
#define VISUALIZATION_FPS   30

// Display Configuration
#define TFT_WIDTH           320
#define TFT_HEIGHT          480
#define SPECTRUM_BARS       64

// LED Configuration  
#define LED_STRIP_PIN       21
#define LED_COUNT           144
#define LED_FREQ_BANDS      16

// Music Analysis Structures
typedef struct {
    float magnitude;
    float phase;
    int64_t timestamp;
} freq_bin_t;

typedef struct {
    float energy;
    float spectral_centroid;
    float spectral_rolloff;
    float zero_crossing_rate;
    float mfcc[13];
    int64_t timestamp;
} audio_features_t;

typedef struct {
    float tempo;
    int time_signature;
    float beat_confidence;
    int64_t last_beat;
    float beat_intervals[BEAT_HISTORY];
    int beat_index;
} beat_tracker_t;

typedef struct {
    int root_note;      // 0-11 (C to B)
    int chord_type;     // 0=major, 1=minor
    float confidence;
    float chroma[12];
} chord_info_t;

typedef struct {
    float features[GENRE_FEATURES];
    int predicted_genre;
    float confidence;
    char genre_name[16];
} genre_analysis_t;

// Global Variables
static i2s_chan_handle_t rx_handle = NULL;
static QueueHandle_t fft_queue;
static float window_func[N_SAMPLES];
static float fft_input[N_SAMPLES * 2];  // Real + imaginary
static float fft_output[N_SAMPLES];
static freq_bin_t spectrum[FREQ_BINS];
static beat_tracker_t beat_tracker = {0};
static chord_info_t current_chord = {0};
static genre_analysis_t genre_info = {0};

// Chord templates (major and minor triads in 12-TET)
static const float chord_templates[CHORD_TEMPLATES][12] = {
    // Major chords (C, C#, D, D#, E, F, F#, G, G#, A, A#, B)
    {1,0,0,0,1,0,0,1,0,0,0,0}, // C major
    {0,1,0,0,0,1,0,0,1,0,0,0}, // C# major
    {0,0,1,0,0,0,1,0,0,1,0,0}, // D major
    {0,0,0,1,0,0,0,1,0,0,1,0}, // D# major
    {0,0,0,0,1,0,0,0,1,0,0,1}, // E major
    {1,0,0,0,0,1,0,0,0,1,0,0}, // F major
    {0,1,0,0,0,0,1,0,0,0,1,0}, // F# major
    {0,0,1,0,0,0,0,1,0,0,0,1}, // G major
    {1,0,0,1,0,0,0,0,1,0,0,0}, // G# major
    {0,1,0,0,1,0,0,0,0,1,0,0}, // A major
    {0,0,1,0,0,1,0,0,0,0,1,0}, // A# major
    {0,0,0,1,0,0,1,0,0,0,0,1}, // B major
    // Minor chords
    {1,0,0,1,0,0,0,1,0,0,0,0}, // C minor
    {0,1,0,0,1,0,0,0,1,0,0,0}, // C# minor
    {0,0,1,0,0,1,0,0,0,1,0,0}, // D minor
    {0,0,0,1,0,0,1,0,0,0,1,0}, // D# minor
    {0,0,0,0,1,0,0,1,0,0,0,1}, // E minor
    {1,0,0,0,0,1,0,0,1,0,0,0}, // F minor
    {0,1,0,0,0,0,1,0,0,1,0,0}, // F# minor
    {0,0,1,0,0,0,0,1,0,0,1,0}, // G minor
    {0,0,0,1,0,0,0,0,1,0,0,1}, // G# minor
    {1,0,0,0,1,0,0,0,0,1,0,0}, // A minor
    {0,1,0,0,0,1,0,0,0,0,1,0}, // A# minor
    {0,0,1,0,0,0,1,0,0,0,0,1}, // B minor
};

static const char* chord_names[CHORD_TEMPLATES] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
    "Cm", "C#m", "Dm", "D#m", "Em", "Fm", "F#m", "Gm", "G#m", "Am", "A#m", "Bm"
};

static const char* genre_names[] = {
    "Rock", "Pop", "Jazz", "Classical", "Electronic", "Hip-Hop", "Country", "Blues"
};

// Function prototypes
static esp_err_t i2s_init(void);
static void init_hann_window(void);
static void audio_processing_task(void *pvParameters);
static void music_analysis_task(void *pvParameters);
static void visualization_task(void *pvParameters);
static void perform_fft(float *input, float *output);
static void extract_audio_features(float *spectrum, audio_features_t *features);
static void detect_beats(audio_features_t *features);
static void analyze_chords(float *spectrum);
static void classify_genre(audio_features_t *features);
static float calculate_spectral_centroid(float *spectrum);
static float calculate_spectral_rolloff(float *spectrum);

// Initialize I2S
static esp_err_t i2s_init(void) {
    ESP_LOGI(TAG, "Initializing I2S for audio input");
    
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));
    
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_BIT_WIDTH, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCK_PIN,
            .ws = I2S_WS_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_DATA_PIN,
            .invert_flags = {0},
        },
    };
    
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
    
    ESP_LOGI(TAG, "I2S initialized at %d Hz", I2S_SAMPLE_RATE);
    return ESP_OK;
}

// Initialize Hann window for FFT
static void init_hann_window(void) {
    for (int i = 0; i < N_SAMPLES; i++) {
        window_func[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (N_SAMPLES - 1)));
    }
    ESP_LOGI(TAG, "Hann window initialized");
}

// Perform FFT using ESP-DSP
static void perform_fft(float *input, float *output) {
    // Apply window and prepare complex input
    for (int i = 0; i < N_SAMPLES; i++) {
        fft_input[i * 2 + 0] = input[i] * window_func[i];  // Real part
        fft_input[i * 2 + 1] = 0.0f;                        // Imaginary part
    }
    
    // Perform FFT using ESP-DSP
    dsps_fft2r_fc32(fft_input, N_SAMPLES);
    dsps_bit_rev_fc32(fft_input, N_SAMPLES);
    dsps_cplx2reC_fc32(fft_input, N_SAMPLES);
    
    // Calculate magnitude spectrum
    for (int i = 0; i < FREQ_BINS; i++) {
        float real = fft_input[i * 2];
        float imag = fft_input[i * 2 + 1];
        output[i] = sqrtf(real * real + imag * imag);
        
        // Update spectrum bins with timestamp
        spectrum[i].magnitude = output[i];
        spectrum[i].phase = atan2f(imag, real);
        spectrum[i].timestamp = esp_timer_get_time();
    }
}

// Extract audio features for analysis
static void extract_audio_features(float *spec, audio_features_t *features) {
    // Calculate total energy
    features->energy = 0.0f;
    for (int i = 1; i < FREQ_BINS; i++) {  // Skip DC component
        features->energy += spec[i] * spec[i];
    }
    features->energy = sqrtf(features->energy);
    
    // Calculate spectral centroid
    features->spectral_centroid = calculate_spectral_centroid(spec);
    
    // Calculate spectral rolloff (85% of energy)
    features->spectral_rolloff = calculate_spectral_rolloff(spec);
    
    // Placeholder for zero crossing rate (would need time domain data)
    features->zero_crossing_rate = 0.1f;
    
    // Simple MFCC-like features (mel-scale frequency bins)
    for (int i = 0; i < 13; i++) {
        int start_bin = (i * FREQ_BINS) / 13;
        int end_bin = ((i + 1) * FREQ_BINS) / 13;
        
        float energy = 0.0f;
        for (int j = start_bin; j < end_bin; j++) {
            energy += spec[j];
        }
        features->mfcc[i] = log10f(energy + 1e-10f);  // Log energy
    }
    
    features->timestamp = esp_timer_get_time();
}

// Calculate spectral centroid
static float calculate_spectral_centroid(float *spectrum) {
    float weighted_sum = 0.0f;
    float magnitude_sum = 0.0f;
    
    for (int i = 1; i < FREQ_BINS; i++) {
        float freq = (float)i * I2S_SAMPLE_RATE / (2.0f * FREQ_BINS);
        weighted_sum += freq * spectrum[i];
        magnitude_sum += spectrum[i];
    }
    
    return (magnitude_sum > 0) ? weighted_sum / magnitude_sum : 0.0f;
}

// Calculate spectral rolloff (frequency below which 85% of energy lies)
static float calculate_spectral_rolloff(float *spectrum) {
    float total_energy = 0.0f;
    for (int i = 1; i < FREQ_BINS; i++) {
        total_energy += spectrum[i];
    }
    
    float energy_threshold = 0.85f * total_energy;
    float cumulative_energy = 0.0f;
    
    for (int i = 1; i < FREQ_BINS; i++) {
        cumulative_energy += spectrum[i];
        if (cumulative_energy >= energy_threshold) {
            return (float)i * I2S_SAMPLE_RATE / (2.0f * FREQ_BINS);
        }
    }
    
    return I2S_SAMPLE_RATE / 2.0f;  // Nyquist frequency
}

// Beat detection using energy-based algorithm
static void detect_beats(audio_features_t *features) {
    static float energy_history[8] = {0};
    static int history_index = 0;
    static int64_t last_beat_time = 0;
    
    // Update energy history
    energy_history[history_index] = features->energy;
    history_index = (history_index + 1) % 8;
    
    // Calculate average energy
    float avg_energy = 0.0f;
    for (int i = 0; i < 8; i++) {
        avg_energy += energy_history[i];
    }
    avg_energy /= 8.0f;
    
    // Beat detection threshold
    float beat_threshold = 1.3f * avg_energy;
    int64_t now = esp_timer_get_time();
    
    // Minimum time between beats (avoid double detection)
    if (features->energy > beat_threshold && (now - last_beat_time) > 200000) {  // 200ms
        // Beat detected
        if (beat_tracker.beat_index > 0) {
            float interval = (now - beat_tracker.last_beat) / 1000000.0f;  // Convert to seconds
            beat_tracker.beat_intervals[beat_tracker.beat_index % BEAT_HISTORY] = interval;
            
            // Calculate tempo from recent intervals
            float avg_interval = 0.0f;
            int count = (beat_tracker.beat_index < BEAT_HISTORY) ? beat_tracker.beat_index : BEAT_HISTORY;
            for (int i = 0; i < count; i++) {
                avg_interval += beat_tracker.beat_intervals[i];
            }
            avg_interval /= count;
            beat_tracker.tempo = 60.0f / avg_interval;  // BPM
        }
        
        beat_tracker.last_beat = now;
        beat_tracker.beat_index++;
        beat_tracker.beat_confidence = (features->energy - avg_energy) / avg_energy;
        
        last_beat_time = now;
        
        ESP_LOGI(TAG, "Beat detected! Tempo: %.1f BPM, Confidence: %.2f", 
                 beat_tracker.tempo, beat_tracker.beat_confidence);
    }
}

// Chord analysis using chroma vectors
static void analyze_chords(float *spectrum) {
    // Calculate chroma vector (12-bin chromagram)
    float chroma[12] = {0};
    
    for (int i = 1; i < FREQ_BINS; i++) {
        float freq = (float)i * I2S_SAMPLE_RATE / (2.0f * FREQ_BINS);
        if (freq > 80.0f && freq < 2000.0f) {  // Focus on musical range
            // Convert frequency to pitch class
            float pitch = 12.0f * log2f(freq / 440.0f) + 69.0f;  // MIDI note number
            int pitch_class = ((int)roundf(pitch)) % 12;
            if (pitch_class < 0) pitch_class += 12;
            
            chroma[pitch_class] += spectrum[i];
        }
    }
    
    // Normalize chroma vector
    float chroma_sum = 0.0f;
    for (int i = 0; i < 12; i++) {
        chroma_sum += chroma[i];
    }
    if (chroma_sum > 0) {
        for (int i = 0; i < 12; i++) {
            chroma[i] /= chroma_sum;
            current_chord.chroma[i] = chroma[i];
        }
    }
    
    // Match against chord templates
    float best_match = 0.0f;
    int best_chord = 0;
    
    for (int c = 0; c < CHORD_TEMPLATES; c++) {
        float correlation = 0.0f;
        for (int i = 0; i < 12; i++) {
            correlation += chroma[i] * chord_templates[c][i];
        }
        
        if (correlation > best_match) {
            best_match = correlation;
            best_chord = c;
        }
    }
    
    // Update chord info if confidence is high enough
    if (best_match > 0.3f) {  // Threshold for chord detection
        current_chord.root_note = best_chord % 12;
        current_chord.chord_type = (best_chord >= 12) ? 1 : 0;  // 0=major, 1=minor
        current_chord.confidence = best_match;
        
        static int64_t last_chord_log = 0;
        int64_t now = esp_timer_get_time();
        if (now - last_chord_log > 2000000) {  // Log every 2 seconds
            ESP_LOGI(TAG, "Chord: %s (%.2f confidence)", 
                     chord_names[best_chord], current_chord.confidence);
            last_chord_log = now;
        }
    }
}

// Simple genre classification based on spectral features
static void classify_genre(audio_features_t *features) {
    // Copy features for analysis
    for (int i = 0; i < GENRE_FEATURES; i++) {
        genre_info.features[i] = features->mfcc[i];
    }
    
    // Simple rule-based classification (placeholder for ML model)
    float centroid_ratio = features->spectral_centroid / 2000.0f;  // Normalize
    float energy_level = features->energy;
    
    if (centroid_ratio > 0.7f && energy_level > 0.5f) {
        genre_info.predicted_genre = 4;  // Electronic
        strcpy(genre_info.genre_name, "Electronic");
    } else if (centroid_ratio < 0.3f && energy_level > 0.3f) {
        genre_info.predicted_genre = 7;  // Blues
        strcpy(genre_info.genre_name, "Blues");
    } else if (beat_tracker.tempo > 120 && beat_tracker.tempo < 140) {
        genre_info.predicted_genre = 1;  // Pop
        strcpy(genre_info.genre_name, "Pop");
    } else {
        genre_info.predicted_genre = 0;  // Rock
        strcpy(genre_info.genre_name, "Rock");
    }
    
    genre_info.confidence = 0.6f;  // Placeholder confidence
}

// Audio processing task (Core 1)
static void audio_processing_task(void *pvParameters) {
    ESP_LOGI(TAG, "Audio processing task started on core %d", xPortGetCoreID());
    
    int32_t *i2s_samples = heap_caps_malloc(N_SAMPLES * sizeof(int32_t), MALLOC_CAP_DMA);
    float *audio_samples = heap_caps_malloc(N_SAMPLES * sizeof(float), MALLOC_CAP_SPIRAM);
    
    if (!i2s_samples || !audio_samples) {
        ESP_LOGE(TAG, "Failed to allocate audio buffers");
        vTaskDelete(NULL);
        return;
    }
    
    size_t bytes_read;
    
    while (1) {
        // Read audio data
        esp_err_t ret = i2s_channel_read(rx_handle, i2s_samples, 
                                         N_SAMPLES * sizeof(int32_t), 
                                         &bytes_read, portMAX_DELAY);
        
        if (ret == ESP_OK && bytes_read > 0) {
            // Convert to float and normalize
            for (int i = 0; i < N_SAMPLES; i++) {
                audio_samples[i] = (float)(i2s_samples[i] >> 14) / 131072.0f;
            }
            
            // Send to analysis queue
            xQueueSend(fft_queue, audio_samples, 0);
        }
        
        // Target ~30fps processing
        vTaskDelay(pdMS_TO_TICKS(33));
    }
    
    heap_caps_free(i2s_samples);
    heap_caps_free(audio_samples);
    vTaskDelete(NULL);
}

// Music analysis task (Core 0)
static void music_analysis_task(void *pvParameters) {
    ESP_LOGI(TAG, "Music analysis task started on core %d", xPortGetCoreID());
    
    float *audio_buffer = heap_caps_malloc(N_SAMPLES * sizeof(float), MALLOC_CAP_SPIRAM);
    if (!audio_buffer) {
        ESP_LOGE(TAG, "Failed to allocate analysis buffer");
        vTaskDelete(NULL);
        return;
    }
    
    audio_features_t features;
    
    while (1) {
        if (xQueueReceive(fft_queue, audio_buffer, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Perform FFT analysis
            perform_fft(audio_buffer, fft_output);
            
            // Extract audio features
            extract_audio_features(fft_output, &features);
            
            // Music analysis
            detect_beats(&features);
            analyze_chords(fft_output);
            classify_genre(&features);
        }
    }
    
    heap_caps_free(audio_buffer);
    vTaskDelete(NULL);
}

// Visualization task (Core 0)
static void visualization_task(void *pvParameters) {
    ESP_LOGI(TAG, "Visualization task started on core %d", xPortGetCoreID());
    
    while (1) {
        // Update LED strip based on frequency spectrum
        // (Implementation would depend on LED driver library)
        
        // Update TFT display with spectrograms and analysis results
        // (Implementation would depend on display driver)
        
        // Log current analysis results
        static int64_t last_log = 0;
        int64_t now = esp_timer_get_time();
        if (now - last_log > 5000000) {  // Every 5 seconds
            ESP_LOGI(TAG, "Analysis: Tempo=%.1f BPM, Genre=%s, Chord=%s", 
                     beat_tracker.tempo, genre_info.genre_name, 
                     current_chord.confidence > 0.3f ? chord_names[current_chord.root_note + 
                     (current_chord.chord_type * 12)] : "Unknown");
            last_log = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000 / VISUALIZATION_FPS));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== Musical DNA Sequencer Starting ===");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize ESP-DSP
    dsps_fft2r_init_fc32(NULL, N_SAMPLES);
    
    // Create queues
    fft_queue = xQueueCreate(4, N_SAMPLES * sizeof(float));
    if (!fft_queue) {
        ESP_LOGE(TAG, "Failed to create FFT queue");
        return;
    }
    
    // Initialize components
    ESP_ERROR_CHECK(i2s_init());
    init_hann_window();
    
    // Initialize beat tracker
    beat_tracker.tempo = 120.0f;  // Default BPM
    beat_tracker.time_signature = 4;
    
    // Start tasks
    xTaskCreatePinnedToCore(audio_processing_task, "audio_proc", 8192, NULL, 
                            configMAX_PRIORITIES - 1, NULL, 1);
    xTaskCreatePinnedToCore(music_analysis_task, "music_analysis", 12288, NULL, 
                            configMAX_PRIORITIES - 2, NULL, 0);
    xTaskCreatePinnedToCore(visualization_task, "visualization", 4096, NULL, 
                            configMAX_PRIORITIES - 3, NULL, 0);
    
    ESP_LOGI(TAG, "System initialized successfully!");
    ESP_LOGI(TAG, "Sample rate: %d Hz, FFT size: %d", I2S_SAMPLE_RATE, N_FFT);
    ESP_LOGI(TAG, "Ready for musical analysis...");
    
    // Main loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "System running... Tempo: %.1f BPM", beat_tracker.tempo);
    }
}