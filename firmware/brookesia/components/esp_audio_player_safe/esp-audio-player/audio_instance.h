#pragma once

#include "esp_err.h"
#include "include/audio_player.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque handle for a player instance.
 * Used for multi-instance control in mixer
 */
typedef void* audio_instance_handle_t;

#define CHECK_INSTANCE(i) \
    ESP_RETURN_ON_FALSE(i != NULL, ESP_ERR_INVALID_ARG, "audio_instance", "instance is NULL")

const char* event_to_string(audio_player_callback_event_t event);
audio_player_callback_event_t state_to_event(audio_player_state_t state);

audio_player_state_t audio_instance_get_state(audio_instance_handle_t h);
esp_err_t audio_instance_callback_register(audio_instance_handle_t h, audio_player_cb_t call_back, void *user_ctx);

esp_err_t audio_instance_play(audio_instance_handle_t h, FILE *fp);
esp_err_t audio_instance_pause(audio_instance_handle_t h);
esp_err_t audio_instance_resume(audio_instance_handle_t h);
esp_err_t audio_instance_stop(audio_instance_handle_t h);

esp_err_t audio_instance_new(audio_instance_handle_t *h, audio_player_config_t *config);
esp_err_t audio_instance_delete(audio_instance_handle_t h);

#ifdef __cplusplus
}
#endif
