#include "audio_processing.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_dsp.h"
#include <math.h>
#include <string.h>

static const char *TAG = "audio_processing";

// Global configuration
static audio_config_t g_audio_config = {0};
static bool g_initialized = false;

// Window function coefficients
static float *g_window_coeffs = NULL;

// Internal function prototypes
static esp_err_t generate_window_coeffs(int window_type, int length);
static float calculate_spectral_centroid(const float *spectrum, int size, int sample_rate);
static float calculate_spectral_rolloff(const float *spectrum, int size, int sample_rate, float threshold);

esp_err_t audio_processing_init(const audio_config_t *config) {
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate configuration
    if (config->fft_size < 64 || config->fft_size > 4096) {
        ESP_LOGE(TAG, "Invalid FFT size: %d", config->fft_size);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (config->sample_rate < 8000 || config->sample_rate > 96000) {
        ESP_LOGE(TAG, "Invalid sample rate: %d", config->sample_rate);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Store configuration
    memcpy(&g_audio_config, config, sizeof(audio_config_t));
    
    // Initialize ESP-DSP
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, config->fft_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ESP-DSP: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Generate window coefficients
    ret = generate_window_coeffs(config->window_type, config->fft_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to generate window coefficients");
        return ret;
    }
    
    g_initialized = true;
    ESP_LOGI(TAG, "Audio processing initialized: %d Hz, FFT size %d", 
             config->sample_rate, config->fft_size);
    
    return ESP_OK;
}

static esp_err_t generate_window_coeffs(int window_type, int length) {
    if (g_window_coeffs) {
        free(g_window_coeffs);
    }
    
    g_window_coeffs = malloc(length * sizeof(float));
    if (!g_window_coeffs) {
        return ESP_ERR_NO_MEM;
    }
    
    switch (window_type) {
        case AUDIO_WINDOW_HANN:
            for (int i = 0; i < length; i++) {
                g_window_coeffs[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (length - 1)));
            }
            break;
            
        case AUDIO_WINDOW_HAMMING:
            for (int i = 0; i < length; i++) {
                g_window_coeffs[i] = 0.54f - 0.46f * cosf(2.0f * M_PI * i / (length - 1));
            }
            break;
            
        case AUDIO_WINDOW_BLACKMAN:
            for (int i = 0; i < length; i++) {
                float a0 = 0.42f;
                float a1 = 0.50f;
                float a2 = 0.08f;
                g_window_coeffs[i] = a0 - a1 * cosf(2.0f * M_PI * i / (length - 1)) + 
                                    a2 * cosf(4.0f * M_PI * i / (length - 1));
            }
            break;
            
        default:
            // Rectangular window (no windowing)
            for (int i = 0; i < length; i++) {
                g_window_coeffs[i] = 1.0f;
            }
            break;
    }
    
    return ESP_OK;
}

esp_err_t audio_apply_window(const float *samples, float *windowed_samples, 
                             int length, int window_type) {
    if (!samples || !windowed_samples) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Use pre-computed coefficients if available and matching
    if (g_window_coeffs && length == g_audio_config.fft_size && 
        window_type == g_audio_config.window_type) {
        for (int i = 0; i < length; i++) {
            windowed_samples[i] = samples[i] * g_window_coeffs[i];
        }
        return ESP_OK;
    }
    
    // Generate window on-the-fly for different parameters
    switch (window_type) {
        case AUDIO_WINDOW_HANN:
            for (int i = 0; i < length; i++) {
                float coeff = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (length - 1)));
                windowed_samples[i] = samples[i] * coeff;
            }
            break;
            
        case AUDIO_WINDOW_HAMMING:
            for (int i = 0; i < length; i++) {
                float coeff = 0.54f - 0.46f * cosf(2.0f * M_PI * i / (length - 1));
                windowed_samples[i] = samples[i] * coeff;
            }
            break;
            
        case AUDIO_WINDOW_BLACKMAN:
            for (int i = 0; i < length; i++) {
                float a0 = 0.42f;
                float a1 = 0.50f;
                float a2 = 0.08f;
                float coeff = a0 - a1 * cosf(2.0f * M_PI * i / (length - 1)) + 
                             a2 * cosf(4.0f * M_PI * i / (length - 1));
                windowed_samples[i] = samples[i] * coeff;
            }
            break;
            
        default:
            // No windowing
            memcpy(windowed_samples, samples, length * sizeof(float));
            break;
    }
    
    return ESP_OK;
}

esp_err_t audio_compute_fft(const float *input_samples, float *fft_output, int fft_size) {
    if (!input_samples || !fft_output) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Allocate complex FFT input buffer
    float *fft_input = malloc(fft_size * 2 * sizeof(float));
    if (!fft_input) {
        return ESP_ERR_NO_MEM;
    }
    
    // Prepare complex input (real + imaginary)
    for (int i = 0; i < fft_size; i++) {
        fft_input[i * 2 + 0] = input_samples[i];  // Real part
        fft_input[i * 2 + 1] = 0.0f;              // Imaginary part
    }
    
    // Perform FFT
    esp_err_t ret = dsps_fft2r_fc32(fft_input, fft_size);
    if (ret != ESP_OK) {
        free(fft_input);
        return ret;
    }
    
    // Bit reversal
    dsps_bit_rev_fc32(fft_input, fft_size);
    
    // Convert to real spectrum
    dsps_cplx2reC_fc32(fft_input, fft_size);
    
    // Calculate magnitude spectrum
    for (int i = 0; i < fft_size / 2; i++) {
        float real = fft_input[i * 2];
        float imag = fft_input[i * 2 + 1];
        fft_output[i] = sqrtf(real * real + imag * imag);
    }
    
    free(fft_input);
    return ESP_OK;
}

static float calculate_spectral_centroid(const float *spectrum, int size, int sample_rate) {
    float weighted_sum = 0.0f;
    float magnitude_sum = 0.0f;
    
    for (int i = 1; i < size; i++) {  // Skip DC component
        float freq = (float)i * sample_rate / (2.0f * size);
        weighted_sum += freq * spectrum[i];
        magnitude_sum += spectrum[i];
    }
    
    return (magnitude_sum > 0) ? weighted_sum / magnitude_sum : 0.0f;
}

static float calculate_spectral_rolloff(const float *spectrum, int size, int sample_rate, float threshold) {
    float total_energy = 0.0f;
    for (int i = 1; i < size; i++) {
        total_energy += spectrum[i];
    }
    
    float energy_threshold = threshold * total_energy;
    float cumulative_energy = 0.0f;
    
    for (int i = 1; i < size; i++) {
        cumulative_energy += spectrum[i];
        if (cumulative_energy >= energy_threshold) {
            return (float)i * sample_rate / (2.0f * size);
        }
    }
    
    return sample_rate / 2.0f;  // Nyquist frequency
}

esp_err_t audio_extract_features(const float *spectrum, int spectrum_size, 
                                 int sample_rate, audio_features_t *features) {
    if (!spectrum || !features) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize features structure
    memset(features, 0, sizeof(audio_features_t));
    
    // Calculate total energy
    features->energy = 0.0f;
    for (int i = 1; i < spectrum_size; i++) {  // Skip DC component
        features->energy += spectrum[i] * spectrum[i];
    }
    features->energy = sqrtf(features->energy);
    
    // Calculate spectral centroid
    features->spectral_centroid = calculate_spectral_centroid(spectrum, spectrum_size, sample_rate);
    
    // Calculate spectral rolloff (85% energy)
    features->spectral_rolloff = calculate_spectral_rolloff(spectrum, spectrum_size, sample_rate, 0.85f);
    
    // Calculate spectral spread
    float centroid = features->spectral_centroid;
    float spread_sum = 0.0f;
    float magnitude_sum = 0.0f;
    
    for (int i = 1; i < spectrum_size; i++) {
        float freq = (float)i * sample_rate / (2.0f * spectrum_size);
        float diff = freq - centroid;
        spread_sum += (diff * diff) * spectrum[i];
        magnitude_sum += spectrum[i];
    }
    features->spectral_spread = (magnitude_sum > 0) ? sqrtf(spread_sum / magnitude_sum) : 0.0f;
    
    // Compute MFCC features
    audio_compute_mfcc(spectrum, spectrum_size, sample_rate, features->mfcc);
    
    // Compute chroma features
    audio_compute_chroma(spectrum, spectrum_size, sample_rate, features->chroma);
    
    features->timestamp = esp_timer_get_time();
    
    return ESP_OK;
}

float audio_freq_to_mel(float freq) {
    return 2595.0f * log10f(1.0f + freq / 700.0f);
}

float audio_mel_to_freq(float mel) {
    return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f);
}

esp_err_t audio_compute_mfcc(const float *spectrum, int spectrum_size, 
                             int sample_rate, float *mfcc) {
    if (!spectrum || !mfcc) {
        return ESP_ERR_INVALID_ARG;
    }
    
    const int num_mel_filters = 26;
    const int num_mfcc = 13;
    
    // Mel filter bank boundaries
    float mel_low = audio_freq_to_mel(80.0f);   // Low frequency
    float mel_high = audio_freq_to_mel(sample_rate / 2.0f);  // High frequency
    
    float mel_filterbank[num_mel_filters];
    memset(mel_filterbank, 0, sizeof(mel_filterbank));
    
    // Apply mel filter bank
    for (int m = 0; m < num_mel_filters; m++) {
        float mel_center = mel_low + (mel_high - mel_low) * m / (num_mel_filters - 1);
        float freq_center = audio_mel_to_freq(mel_center);
        
        // Simple triangular filter approximation
        int bin_center = (int)(freq_center * 2 * spectrum_size / sample_rate);
        int bin_width = spectrum_size / num_mel_filters;
        
        for (int i = bin_center - bin_width; i <= bin_center + bin_width && i < spectrum_size; i++) {
            if (i >= 0) {
                float weight = 1.0f - fabsf(i - bin_center) / (float)bin_width;
                if (weight > 0) {
                    mel_filterbank[m] += spectrum[i] * weight;
                }
            }
        }
        
        // Log compression
        mel_filterbank[m] = log10f(mel_filterbank[m] + 1e-10f);
    }
    
    // Discrete Cosine Transform (simplified)
    for (int i = 0; i < num_mfcc; i++) {
        mfcc[i] = 0.0f;
        for (int j = 0; j < num_mel_filters; j++) {
            mfcc[i] += mel_filterbank[j] * cosf(M_PI * i * (j + 0.5f) / num_mel_filters);
        }
    }
    
    return ESP_OK;
}

esp_err_t audio_compute_chroma(const float *spectrum, int spectrum_size, 
                               int sample_rate, float *chroma) {
    if (!spectrum || !chroma) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize chroma vector
    memset(chroma, 0, 12 * sizeof(float));
    
    // Calculate chroma from spectrum
    for (int i = 1; i < spectrum_size; i++) {
        float freq = (float)i * sample_rate / (2.0f * spectrum_size);
        
        // Focus on musical frequency range
        if (freq > 80.0f && freq < 2000.0f) {
            // Convert frequency to MIDI note number
            float midi_note = 12.0f * log2f(freq / 440.0f) + 69.0f;
            int pitch_class = ((int)roundf(midi_note)) % 12;
            
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
        }
    }
    
    return ESP_OK;
}

esp_err_t audio_beat_detector_init(beat_detector_t *detector) {
    if (!detector) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(detector, 0, sizeof(beat_detector_t));
    detector->tempo = 120.0f;  // Default BPM
    
    return ESP_OK;
}

esp_err_t audio_beat_detector_process(beat_detector_t *detector, 
                                      const audio_features_t *features, 
                                      bool *beat_detected) {
    if (!detector || !features || !beat_detected) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *beat_detected = false;
    
    // Update energy history
    detector->energy_history[detector->history_index] = features->energy;
    detector->history_index = (detector->history_index + 1) % 16;
    
    // Calculate average energy
    float avg_energy = 0.0f;
    for (int i = 0; i < 16; i++) {
        avg_energy += detector->energy_history[i];
    }
    avg_energy /= 16.0f;
    
    // Beat detection threshold
    float beat_threshold = 1.3f * avg_energy;
    int64_t now = features->timestamp;
    
    // Minimum time between beats (avoid double detection)
    if (features->energy > beat_threshold && 
        (now - detector->last_beat_time) > 200000) {  // 200ms
        
        *beat_detected = true;
        
        if (detector->beat_count > 0) {
            float interval = (now - detector->last_beat_time) / 1000000.0f;  // seconds
            int interval_idx = detector->beat_count % 8;
            detector->beat_intervals[interval_idx] = interval;
            
            // Calculate tempo from recent intervals
            float avg_interval = 0.0f;
            int count = (detector->beat_count < 8) ? detector->beat_count : 8;
            for (int i = 0; i < count; i++) {
                avg_interval += detector->beat_intervals[i];
            }
            avg_interval /= count;
            detector->tempo = 60.0f / avg_interval;  // BPM
        }
        
        detector->last_beat_time = now;
        detector->beat_count++;
        detector->confidence = (features->energy - avg_energy) / avg_energy;
    }
    
    return ESP_OK;
}

float audio_calculate_spl(const int32_t *samples, int num_samples, float calibration_offset) {
    if (!samples || num_samples <= 0) {
        return 0.0f;
    }
    
    double sum_squares = 0.0;
    
    // Calculate RMS value
    for (int i = 0; i < num_samples; i++) {
        // Convert I2S data to normalized float (assuming 18-bit useful data in 32-bit)
        double sample = (double)(samples[i] >> 14) / 131072.0;
        sum_squares += sample * sample;
    }
    
    double rms = sqrt(sum_squares / num_samples);
    
    // Prevent log of zero
    if (rms < 1e-10) rms = 1e-10;
    
    // Calculate SPL: 20 * log10(P/Pref) + calibration
    const double ref_pressure = 20e-6;  // 20 µPa reference
    float spl = 20.0f * log10f(rms / ref_pressure) + calibration_offset;
    
    return spl;
}

esp_err_t audio_frequency_filter(const float *spectrum, float *filtered_spectrum,
                                 int spectrum_size, float low_freq, float high_freq,
                                 int sample_rate) {
    if (!spectrum || !filtered_spectrum) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Calculate bin indices for frequency range
    int low_bin = (int)(low_freq * 2 * spectrum_size / sample_rate);
    int high_bin = (int)(high_freq * 2 * spectrum_size / sample_rate);
    
    // Clamp to valid range
    low_bin = (low_bin < 0) ? 0 : low_bin;
    high_bin = (high_bin >= spectrum_size) ? spectrum_size - 1 : high_bin;
    
    // Apply frequency filter
    memset(filtered_spectrum, 0, spectrum_size * sizeof(float));
    for (int i = low_bin; i <= high_bin; i++) {
        filtered_spectrum[i] = spectrum[i];
    }
    
    return ESP_OK;
}

esp_err_t audio_compute_psd(const float *fft_output, float *psd_output, int size) {
    if (!fft_output || !psd_output) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Power spectral density = magnitude^2 / fs
    for (int i = 0; i < size; i++) {
        psd_output[i] = (fft_output[i] * fft_output[i]) / g_audio_config.sample_rate;
    }
    
    return ESP_OK;
}