/**
 * @file audio_stream.h
 * @brief Stream API — create/delete logical playback streams and control them.
 * These streams own their decode task and submit PCM to the mixer.
 */
#pragma once

#include "audio_player.h"

#ifdef __cplusplus
extern "C" {
#endif

struct audio_stream;
/**
 * @brief Audio stream handle
 */
typedef struct audio_stream* audio_stream_handle_t;

/**
 * @brief Macro to check if a stream handle is valid
 */
#define CHECK_STREAM(s) \
    ESP_RETURN_ON_FALSE(s != NULL, ESP_ERR_INVALID_ARG, "audio_stream", "stream is NULL")

/**
 * @brief Audio stream types
 */
typedef enum {
    AUDIO_STREAM_TYPE_UNKNOWN = 0, /**< Unknown stream type */
    AUDIO_STREAM_TYPE_DECODER,     /**< Stream that decodes audio (e.g., MP3, WAV) */
    AUDIO_STREAM_TYPE_RAW          /**< Stream that accepts raw PCM data */
} audio_stream_type_t;

/**
 * @brief Configuration structure for an audio stream
 */
typedef struct {
    audio_stream_type_t type;    /**< Type of stream */
    char name[16];               /**< Optional: Name of the stream (e.g. "sfx", "bgm"). Auto-generated if empty. */
    UBaseType_t priority;        /**< FreeRTOS task priority for the stream's decoder task (if applicable) */
    BaseType_t coreID;           /**< ESP32 core ID for the stream's decoder task (if applicable) */
} audio_stream_config_t;

/**
 * @brief Default configuration for an audio decoder stream
 *
 * @param _name Name of the stream
 */
#define DEFAULT_AUDIO_STREAM_CONFIG(_name) {    \
        .type = AUDIO_STREAM_TYPE_DECODER,      \
        .name = _name,                          \
        .priority = tskIDLE_PRIORITY + 1,       \
        .coreID = tskNO_AFFINITY                \
    }

/**
 * @brief Get the current state of a stream
 *
 * @param h Handle of the stream
 * @return Current audio_player_state_t of the stream
 */
audio_player_state_t audio_stream_get_state(audio_stream_handle_t h);

/**
 * @brief Get the type of a stream
 *
 * @param h Handle of the stream
 * @return audio_stream_type_t of the stream
 */
audio_stream_type_t audio_stream_get_type(audio_stream_handle_t h);

/**
 * @brief Play an audio file on a stream
 *
 * Only supported for DECODER type streams.
 *
 * @param h Handle of the stream
 * @param fp File pointer to the audio file
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_NOT_SUPPORTED: Stream is not a decoder stream
 *    - Others: Fail
 */
esp_err_t audio_stream_play(audio_stream_handle_t h, FILE *fp);

/**
 * @brief Queue an audio file to be played on a stream
 *
 * Only supported for DECODER type streams.
 *
 * @param h Handle of the stream
 * @param fp File pointer to the audio file
 * @param play_now If true, start playing immediately (interrupting current playback)
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_NOT_SUPPORTED: Stream is not a decoder stream
 *    - Others: Fail
 */
esp_err_t audio_stream_queue(audio_stream_handle_t h, FILE *fp, bool play_now);

/**
 * @brief Stop playback on a stream
 *
 * @param h Handle of the stream
 * @return
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t audio_stream_stop(audio_stream_handle_t h);

/**
 * @brief Pause playback on a stream
 *
 * Only supported for DECODER type streams.
 *
 * @param h Handle of the stream
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_NOT_SUPPORTED: Stream is not a decoder stream
 *    - Others: Fail
 */
esp_err_t audio_stream_pause(audio_stream_handle_t h);

/**
 * @brief Resume playback on a stream
 *
 * Only supported for DECODER type streams.
 *
 * @param h Handle of the stream
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_NOT_SUPPORTED: Stream is not a decoder stream
 *    - Others: Fail
 */
esp_err_t audio_stream_resume(audio_stream_handle_t h);

/**
 * @brief Direct write raw PCM data to a stream
 *
 * Only supported for RAW type streams.
 * Data format must match the mixer configuration (e.g. 44.1kHz, 16-bit, mono/stereo).
 *
 * @param h Handle of the stream
 * @param data Pointer to the PCM data
 * @param size Size of the data in bytes
 * @param timeout_ms Timeout in milliseconds to wait for space in the stream's buffer
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_NOT_SUPPORTED: Stream is not a raw stream
 *    - Others: Fail
 */
esp_err_t audio_stream_write_pcm(audio_stream_handle_t h, void *data, size_t size, uint32_t timeout_ms);

/**
 * @brief Send an event to a raw stream's callback
 *
 * Allows manual state management for raw streams.
 *
 * @param h Handle of the stream
 * @param event Event to send
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_NOT_SUPPORTED: Stream is not a raw stream
 */
esp_err_t audio_stream_raw_send_event(audio_stream_handle_t h, audio_player_callback_event_t event);

/**
 * @brief Create a new audio stream
 *
 * @param cfg Pointer to the stream configuration structure
 * @return Handle to the new stream, or NULL if failed
 */
audio_stream_handle_t audio_stream_new(audio_stream_config_t *cfg);

/**
 * @brief Delete an audio stream and free its resources
 *
 * @param h Handle of the stream to delete
 * @return
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t audio_stream_delete(audio_stream_handle_t h);

#ifdef __cplusplus
}
#endif
