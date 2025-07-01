#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Audio Processing Configuration
#define AUDIO_SAMPLE_RATE_8K        8000
#define AUDIO_SAMPLE_RATE_16K       16000
#define AUDIO_SAMPLE_RATE_22K       22050
#define AUDIO_SAMPLE_RATE_44K       44100
#define AUDIO_SAMPLE_RATE_48K       48000

#define AUDIO_FFT_SIZE_256          256
#define AUDIO_FFT_SIZE_512          512
#define AUDIO_FFT_SIZE_1024         1024
#define AUDIO_FFT_SIZE_2048         2048

#define AUDIO_WINDOW_HANN           0
#define AUDIO_WINDOW_HAMMING        1
#define AUDIO_WINDOW_BLACKMAN       2

// Audio Feature Extraction
typedef struct {
    float energy;                   // Total energy
    float spectral_centroid;        // Center of mass of spectrum
    float spectral_rolloff;         // 85% energy rolloff frequency
    float spectral_spread;          // Spread around centroid
    float spectral_skewness;        // Asymmetry of spectrum
    float spectral_kurtosis;        // Flatness of spectrum
    float zero_crossing_rate;       // Rate of sign changes
    float mfcc[13];                 // Mel-frequency cepstral coefficients
    float chroma[12];               // Pitch class profile
    float tempo;                    // Estimated BPM
    int64_t timestamp;              // Capture timestamp
} audio_features_t;

// Beat Detection State
typedef struct {
    float energy_history[16];       // Energy history buffer
    float beat_intervals[8];        // Recent beat intervals
    int history_index;              // Current position in history
    int beat_count;                 // Total beats detected
    float tempo;                    // Current BPM estimate
    float confidence;               // Beat detection confidence
    int64_t last_beat_time;         // Timestamp of last beat
} beat_detector_t;

// Frequency Analysis Configuration
typedef struct {
    int sample_rate;                // Sample rate in Hz
    int fft_size;                   // FFT size (power of 2)
    int window_type;                // Window function type
    int hop_size;                   // Hop size for overlapping frames
    bool normalize;                 // Normalize output
} audio_config_t;

// Audio Processing Functions

/**
 * @brief Initialize audio processing with configuration
 * @param config Audio processing configuration
 * @return ESP_OK on success, error code on failure
 */
esp_err_t audio_processing_init(const audio_config_t *config);

/**
 * @brief Apply window function to audio samples
 * @param samples Input samples
 * @param windowed_samples Output windowed samples
 * @param length Number of samples
 * @param window_type Window function type
 * @return ESP_OK on success
 */
esp_err_t audio_apply_window(const float *samples, float *windowed_samples, 
                             int length, int window_type);

/**
 * @brief Perform FFT on audio samples
 * @param input_samples Time domain samples
 * @param fft_output Frequency domain output (magnitude spectrum)
 * @param fft_size FFT size
 * @return ESP_OK on success
 */
esp_err_t audio_compute_fft(const float *input_samples, float *fft_output, int fft_size);

/**
 * @brief Extract audio features from spectrum
 * @param spectrum Magnitude spectrum
 * @param spectrum_size Size of spectrum
 * @param sample_rate Sample rate in Hz
 * @param features Output features structure
 * @return ESP_OK on success
 */
esp_err_t audio_extract_features(const float *spectrum, int spectrum_size, 
                                 int sample_rate, audio_features_t *features);

/**
 * @brief Calculate MFCC features
 * @param spectrum Magnitude spectrum
 * @param spectrum_size Size of spectrum
 * @param sample_rate Sample rate in Hz
 * @param mfcc Output MFCC coefficients (13 values)
 * @return ESP_OK on success
 */
esp_err_t audio_compute_mfcc(const float *spectrum, int spectrum_size, 
                             int sample_rate, float *mfcc);

/**
 * @brief Calculate chroma features
 * @param spectrum Magnitude spectrum
 * @param spectrum_size Size of spectrum
 * @param sample_rate Sample rate in Hz
 * @param chroma Output chroma vector (12 values)
 * @return ESP_OK on success
 */
esp_err_t audio_compute_chroma(const float *spectrum, int spectrum_size, 
                               int sample_rate, float *chroma);

/**
 * @brief Initialize beat detector
 * @param detector Beat detector state
 * @return ESP_OK on success
 */
esp_err_t audio_beat_detector_init(beat_detector_t *detector);

/**
 * @brief Process audio frame for beat detection
 * @param detector Beat detector state
 * @param features Audio features from current frame
 * @param beat_detected Output: true if beat detected
 * @return ESP_OK on success
 */
esp_err_t audio_beat_detector_process(beat_detector_t *detector, 
                                      const audio_features_t *features, 
                                      bool *beat_detected);

/**
 * @brief Calculate SPL from audio samples
 * @param samples Audio samples
 * @param num_samples Number of samples
 * @param calibration_offset Microphone calibration offset
 * @return SPL in dB
 */
float audio_calculate_spl(const int32_t *samples, int num_samples, float calibration_offset);

/**
 * @brief Apply frequency filtering to spectrum
 * @param spectrum Input spectrum
 * @param filtered_spectrum Output filtered spectrum
 * @param spectrum_size Size of spectrum
 * @param low_freq Low cutoff frequency (Hz)
 * @param high_freq High cutoff frequency (Hz)
 * @param sample_rate Sample rate (Hz)
 * @return ESP_OK on success
 */
esp_err_t audio_frequency_filter(const float *spectrum, float *filtered_spectrum,
                                 int spectrum_size, float low_freq, float high_freq,
                                 int sample_rate);

/**
 * @brief Convert frequency to mel scale
 * @param freq Frequency in Hz
 * @return Mel value
 */
float audio_freq_to_mel(float freq);

/**
 * @brief Convert mel scale to frequency
 * @param mel Mel value
 * @return Frequency in Hz
 */
float audio_mel_to_freq(float mel);

/**
 * @brief Calculate power spectral density
 * @param fft_output FFT magnitude spectrum
 * @param psd_output Power spectral density output
 * @param size Spectrum size
 * @return ESP_OK on success
 */
esp_err_t audio_compute_psd(const float *fft_output, float *psd_output, int size);

#ifdef __cplusplus
}
#endif