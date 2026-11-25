/**
 * @file audio_mixer.h
 * @brief Mixer interface for esp-audio-player. Provides a global mixer that accepts
 * PCM from multiple sources via FreeRTOS ring buffers and writes mixed PCM to I2S.
 */
#pragma once

#include <stdint.h>

#include "esp_err.h"

#include "audio_player.h"
#include "../audio_decode_types.h"  // FIXME: leaks out
#include "audio_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration structure for the audio mixer
 */
typedef struct {
    audio_player_mute_fn mute_fn;        /**< Function to mute/unmute audio */
    audio_reconfig_std_clock clk_set_fn; /**< Function to reconfigure I2S clock */
    audio_player_write_fn write_fn;      /**< Function to write PCM data to I2S */
    UBaseType_t priority;                /**< FreeRTOS task priority for the mixer task */
    BaseType_t coreID;                   /**< ESP32 core ID for the mixer task */

    format i2s_format;                   /**< Fixed output format for the mixer */
} audio_mixer_config_t;

/**
 * @brief Mixer callback function type
 */
typedef audio_player_cb_t audio_mixer_cb_t;

/**
 * @brief Get the number of active streams in the mixer
 *
 * @return Number of active streams
 */
uint8_t audio_mixer_stream_count();

/**
 * @brief Lock the mixer's main mutex
 *
 * Call this before modifying stream state (busy flags, queues).
 */
void audio_mixer_lock();

/**
 * @brief Unlock the mixer's main mutex
 */
void audio_mixer_unlock();

/**
 * @brief Add a stream to the mixer's processing list
 *
 * This function is thread-safe.
 *
 * @param h Handle of the stream to add
 */
void audio_mixer_add_stream(audio_stream_handle_t h);

/**
 * @brief Remove a stream from the mixer's processing list
 *
 * This function is thread-safe.
 *
 * @param h Handle of the stream to remove
 */
void audio_mixer_remove_stream(audio_stream_handle_t h);

/**
 * @brief Query the current mixer output format
 *
 * Returns zeros if the mixer is not initialized.
 *
 * @param[out] sample_rate Pointer to store the sample rate
 * @param[out] bits_per_sample Pointer to store the bits per sample
 * @param[out] channels Pointer to store the number of channels
 */
void audio_mixer_get_output_format(uint32_t *sample_rate, uint32_t *bits_per_sample, uint32_t *channels);

/**
 * @brief Register a global callback for mixer events
 *
 * @param cb Callback function to register
 */
void audio_mixer_callback_register(audio_mixer_cb_t cb);

/**
 * @brief Check if the mixer is initialized
 *
 * @return true if initialized, false otherwise
 */
bool audio_mixer_is_initialized();

/**
 * @brief Initialize the mixer and start the mixer task
 *
 * @param cfg Pointer to the mixer configuration structure
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_INVALID_ARG: Invalid configuration
 *    - Others: Fail
 */
esp_err_t audio_mixer_init(audio_mixer_config_t *cfg);

/**
 * @brief Deinitialize the mixer and stop the mixer task
 */
void audio_mixer_deinit();

#ifdef __cplusplus
}
#endif
