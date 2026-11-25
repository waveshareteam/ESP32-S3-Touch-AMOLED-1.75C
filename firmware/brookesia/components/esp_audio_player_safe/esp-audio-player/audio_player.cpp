/**
 * @file
 * @version 0.1
 *
 * @copyright Copyright 2021 Espressif Systems (Shanghai) Co. Ltd.
 * @copyright Copyright 2022 Chris Morgan <chmorgan@gmail.com>
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "sdkconfig.h"

#include "audio_player.h"
#include "audio_instance.h"

#include "audio_wav.h"
#include "audio_mp3.h"

static const char *TAG = "audio";

typedef enum {
    AUDIO_PLAYER_REQUEST_NONE = 0,
    AUDIO_PLAYER_REQUEST_PAUSE,              /**< pause playback */
    AUDIO_PLAYER_REQUEST_RESUME,             /**< resumed paused playback */
    AUDIO_PLAYER_REQUEST_PLAY,               /**< initiate playing a new file */
    AUDIO_PLAYER_REQUEST_STOP,               /**< stop playback */
    AUDIO_PLAYER_REQUEST_SHUTDOWN_THREAD,    /**< shutdown audio playback thread */
    AUDIO_PLAYER_REQUEST_MAX
} audio_player_event_type_t;

typedef struct {
    audio_player_event_type_t type;

    // valid if type == AUDIO_PLAYER_EVENT_TYPE_PLAY
    FILE* fp;
} audio_player_event_t;

typedef enum {
    FILE_TYPE_UNKNOWN,
#if defined(CONFIG_AUDIO_PLAYER_ENABLE_MP3)
    FILE_TYPE_MP3,
#endif
#if defined(CONFIG_AUDIO_PLAYER_ENABLE_WAV)
    FILE_TYPE_WAV
#endif
} FILE_TYPE;

typedef struct audio_instance {
    /**
     * Set to true before task is created, false immediately before the
     * task is deleted.
     */
    bool running;

    decode_data output;

    QueueHandle_t event_queue;
    TaskHandle_t task_handle;
    SemaphoreHandle_t stopped_sem;
    SemaphoreHandle_t delete_gate;

    /* **************** AUDIO CALLBACK **************** */
    audio_player_cb_t s_audio_cb;
    void *audio_cb_usrt_ctx;
    audio_player_state_t state;

    audio_player_config_t config;

#if defined(CONFIG_AUDIO_PLAYER_ENABLE_WAV)
    wav_instance wav_data;
#endif

#if defined(CONFIG_AUDIO_PLAYER_ENABLE_MP3)
    HMP3Decoder mp3_decoder;
    mp3_instance mp3_data;
#endif

    format i2s_format;  // last configured i2s format
} audio_instance_t;

static audio_instance_t *g_instance = NULL;  // when non-null, in legacy non-mixer mode
static portMUX_TYPE g_legacy_lifecycle_mux = portMUX_INITIALIZER_UNLOCKED;
static bool g_legacy_lifecycle_busy = false;

audio_player_state_t audio_instance_get_state(audio_instance_handle_t h) {
    audio_instance_t *i = static_cast<audio_instance_t *>(h);
    return i ? i->state : AUDIO_PLAYER_STATE_IDLE;
}

esp_err_t audio_instance_callback_register(audio_instance_handle_t h, audio_player_cb_t call_back, void *user_ctx) {
#if CONFIG_IDF_TARGET_ARCH_XTENSA
    ESP_RETURN_ON_FALSE(esp_ptr_executable(reinterpret_cast<void*>(call_back)), ESP_ERR_INVALID_ARG,
        TAG, "Not a valid call back");
#else
    ESP_RETURN_ON_FALSE(reinterpret_cast<void*>(call_back), ESP_ERR_INVALID_ARG,
        TAG, "Not a valid call back");
#endif
    audio_instance_t *i = static_cast<audio_instance_t *>(h);
    CHECK_INSTANCE(i);
    i->s_audio_cb = call_back;
    i->audio_cb_usrt_ctx = user_ctx;

    return ESP_OK;
}

const char* event_to_string(audio_player_callback_event_t event) {
    switch(event) {
    case AUDIO_PLAYER_CALLBACK_EVENT_IDLE:
        return "AUDIO_PLAYER_CALLBACK_EVENT_IDLE";
    case AUDIO_PLAYER_CALLBACK_EVENT_COMPLETED_PLAYING_NEXT:
        return "AUDIO_PLAYER_CALLBACK_EVENT_COMPLETED_PLAYING_NEXT";
    case AUDIO_PLAYER_CALLBACK_EVENT_PLAYING:
        return "AUDIO_PLAYER_CALLBACK_EVENT_PLAYING";
    case AUDIO_PLAYER_CALLBACK_EVENT_PAUSE:
        return "AUDIO_PLAYER_CALLBACK_EVENT_PAUSE";
    case AUDIO_PLAYER_CALLBACK_EVENT_SHUTDOWN:
        return "AUDIO_PLAYER_CALLBACK_EVENT_SHUTDOWN";
    case AUDIO_PLAYER_CALLBACK_EVENT_UNKNOWN_FILE_TYPE:
        return "AUDIO_PLAYER_CALLBACK_EVENT_UNKNOWN_FILE_TYPE";
    case AUDIO_PLAYER_CALLBACK_EVENT_UNKNOWN:
        return "AUDIO_PLAYER_CALLBACK_EVENT_UNKNOWN";
    }

    return "unknown event";
}

audio_player_callback_event_t state_to_event(audio_player_state_t state) {
    audio_player_callback_event_t event = AUDIO_PLAYER_CALLBACK_EVENT_UNKNOWN;

    switch(state) {
        case AUDIO_PLAYER_STATE_IDLE:
            event = AUDIO_PLAYER_CALLBACK_EVENT_IDLE;
            break;
        case AUDIO_PLAYER_STATE_PAUSE:
            event = AUDIO_PLAYER_CALLBACK_EVENT_PAUSE;
            break;
        case AUDIO_PLAYER_STATE_PLAYING:
            event = AUDIO_PLAYER_CALLBACK_EVENT_PLAYING;
            break;
        case AUDIO_PLAYER_STATE_SHUTDOWN:
            event = AUDIO_PLAYER_CALLBACK_EVENT_SHUTDOWN;
            break;
    };

    return event;
}

static void dispatch_callback(audio_instance_t *i, audio_player_callback_event_t event) {
    LOGI_1("event '%s'", event_to_string(event));

#if CONFIG_IDF_TARGET_ARCH_XTENSA
    if (esp_ptr_executable(reinterpret_cast<void*>(i->s_audio_cb))) {
#else
    if (reinterpret_cast<void*>(i->s_audio_cb)) {
#endif
        audio_player_cb_ctx_t ctx = {
            .audio_event = event,
            .user_ctx = i->audio_cb_usrt_ctx,
        };
        i->s_audio_cb(&ctx);
    }
}

static void set_state(audio_instance_t *i, audio_player_state_t new_state) {
    if(i->state != new_state) {
        i->state = new_state;
        audio_player_callback_event_t event = state_to_event(new_state);
        dispatch_callback(i, event);
    }
}

static void audio_instance_init(audio_instance_t *i) {
    i->event_queue = NULL;
    i->s_audio_cb = NULL;
    i->audio_cb_usrt_ctx = NULL;
    i->state = AUDIO_PLAYER_STATE_IDLE;
    memset(&i->i2s_format, 0, sizeof(i->i2s_format));
}

static esp_err_t mono_to_stereo(uint32_t output_bits_per_sample, decode_data &adata) {
    size_t data = adata.frame_count * (output_bits_per_sample / BITS_PER_BYTE);
    data *= 2;

    // do we have enough space in the output buffer to convert mono to stereo?
    if(data > adata.samples_capacity_max) {
        ESP_LOGE(TAG, "insufficient space in output.samples to convert mono to stereo, need %d, have %d", data, adata.samples_capacity_max);
        return ESP_ERR_NO_MEM;
    }

    size_t new_sample_count = adata.frame_count * 2;

    // convert from back to front to allow conversion in-place
    //
    // NOTE: -1 is because we want to shift to the sample at position X
    //       but if we do (ptr + X) we end up at the sample at index X instead
    //       which is one further
    int16_t *out = reinterpret_cast<int16_t*>(adata.samples) + (new_sample_count - 1);
    int16_t *in = reinterpret_cast<int16_t*>(adata.samples) + (adata.frame_count - 1);
    size_t samples = adata.frame_count;
    while(samples) {
        // write right channel
        *out = *in;
        out--;

        // write left channel
        *out = *in;
        out--;

        // move input buffer back and decrement samples
        in--;
        samples--;
    }

    // adjust channels to 2
    adata.fmt.channels = 2;

    return ESP_OK;
}

static esp_err_t aplay_file(audio_instance_t *i, FILE *fp) {
    LOGI_1("start to decode");

    esp_err_t ret = ESP_OK;
    audio_player_event_t audio_event = { .type = AUDIO_PLAYER_REQUEST_NONE, .fp = NULL };

    FILE_TYPE file_type = FILE_TYPE_UNKNOWN;

#if defined(CONFIG_AUDIO_PLAYER_ENABLE_MP3)
    if(is_mp3(fp)) {
        file_type = FILE_TYPE_MP3;
        LOGI_1("file is mp3");

        // initialize mp3_instance
        i->mp3_data.bytes_in_data_buf = 0;
        i->mp3_data.read_ptr = i->mp3_data.data_buf;
        i->mp3_data.eof_reached = false;
    }
#endif

#if defined(CONFIG_AUDIO_PLAYER_ENABLE_WAV)
    // This can be a pointless condition depending on the build options, no reason to warn about it
    // cppcheck-suppress knownConditionTrueFalse
    if(file_type == FILE_TYPE_UNKNOWN)
    {
        if(is_wav(fp, &i->wav_data)) {
            file_type = FILE_TYPE_WAV;
            LOGI_1("file is wav");
        }
    }
#endif

    // cppcheck-suppress knownConditionTrueFalse
    if(file_type == FILE_TYPE_UNKNOWN) {
        ESP_LOGE(TAG, "unknown file type, cleaning up");
        dispatch_callback(i, AUDIO_PLAYER_CALLBACK_EVENT_UNKNOWN_FILE_TYPE);
        goto clean_up;
    }

    do {
        /* Process audio event sent from other task */
        if (pdPASS == xQueuePeek(i->event_queue, &audio_event, 0)) {
            LOGI_2("event in queue");
            if (AUDIO_PLAYER_REQUEST_PAUSE == audio_event.type) {
                // receive the pause event to take it off of the queue
                xQueueReceive(i->event_queue, &audio_event, 0);

                set_state(i, AUDIO_PLAYER_STATE_PAUSE);

                // wait until an event is received that will cause playback to resume,
                // stop, or change file
                while(1) {
                    xQueuePeek(i->event_queue, &audio_event, portMAX_DELAY);

                    if((AUDIO_PLAYER_REQUEST_PLAY != audio_event.type) &&
                       (AUDIO_PLAYER_REQUEST_STOP != audio_event.type) &&
                       (AUDIO_PLAYER_REQUEST_RESUME != audio_event.type) &&
                       (AUDIO_PLAYER_REQUEST_SHUTDOWN_THREAD != audio_event.type))
                    {
                        // receive to discard the event
                        xQueueReceive(i->event_queue, &audio_event, 0);
                    } else {
                        break;
                    }
                }

                if(AUDIO_PLAYER_REQUEST_RESUME == audio_event.type) {
                    // receive to discard the event
                    xQueueReceive(i->event_queue, &audio_event, 0);
                    continue;
                }

                // else fall out of this condition and let the below logic
                // handle the other event types
            }

            if ((AUDIO_PLAYER_REQUEST_STOP == audio_event.type) ||
                (AUDIO_PLAYER_REQUEST_PLAY == audio_event.type) ||
                (AUDIO_PLAYER_REQUEST_SHUTDOWN_THREAD == audio_event.type)) {
                ret = ESP_OK;
                goto clean_up;
            } else {
                // receive to discard the event, this event has no
                // impact on the state of playback
                xQueueReceive(i->event_queue, &audio_event, 0);
                continue;
            }
        }

        set_state(i, AUDIO_PLAYER_STATE_PLAYING);

        DECODE_STATUS decode_status = DECODE_STATUS_ERROR;

        switch(file_type) {
#if defined(CONFIG_AUDIO_PLAYER_ENABLE_MP3)
            case FILE_TYPE_MP3:
                decode_status = decode_mp3(i->mp3_decoder, fp, &i->output, &i->mp3_data);
                break;
#endif
#if defined(CONFIG_AUDIO_PLAYER_ENABLE_WAV)
            case FILE_TYPE_WAV:
                decode_status = decode_wav(fp, &i->output, &i->wav_data);
                break;
#endif
            case FILE_TYPE_UNKNOWN:
                ESP_LOGE(TAG, "unexpected unknown file type when decoding");
                break;
        }

        // break out and exit if we aren't supposed to continue decoding
        if(decode_status == DECODE_STATUS_CONTINUE)
        {
            // if mono and force_stereo set, convert to stereo as es8311 requires stereo input
            // even though it is mono output
            if(i->output.fmt.channels == 1 && i->config.force_stereo) {
                LOGI_3("c == 1, mono -> stereo");
                ret = mono_to_stereo(i->output.fmt.bits_per_sample, i->output);
                if(ret != ESP_OK) {
                    goto clean_up;
                }
            }

            /* Configure I2S clock if the output format changed */
            if ((i->i2s_format.sample_rate != i->output.fmt.sample_rate) ||
                    (i->i2s_format.channels != i->output.fmt.channels) ||
                    (i->i2s_format.bits_per_sample != i->output.fmt.bits_per_sample)) {
                i->i2s_format = i->output.fmt;
                LOGI_1("format change: sr=%d, bit=%lu, ch=%lu",
                        i->i2s_format.sample_rate,
                        i->i2s_format.bits_per_sample,
                        i->i2s_format.channels);
                i2s_slot_mode_t channel_setting = (i->i2s_format.channels == 1) ? I2S_SLOT_MODE_MONO : I2S_SLOT_MODE_STEREO;
                ret = i->config.clk_set_fn(i->i2s_format.sample_rate,
                            i->i2s_format.bits_per_sample,
                            channel_setting);
                ESP_GOTO_ON_ERROR(ret, clean_up, TAG, "i2s_set_clk");
            }

            /**
             * Block until all data has been accepted into the i2s driver, however
             * the i2s driver has been configured with a buffer to allow for the next round of
             * audio decoding to occur while the previous set of samples is finishing playback, in order
             * to ensure playback without interruption.
             */
            size_t bytes_written = 0;
            size_t bytes_to_write = i->output.frame_count * i->output.fmt.channels * (i->i2s_format.bits_per_sample / 8);
            LOGI_2("c %d, bps %d, bytes %d, frame_count %d",
                i->output.fmt.channels,
                i2s_format.bits_per_sample,
                bytes_to_write,
                i->output.frame_count);

            // NOTE: to aid transition in api, using write_fn2 based on write_ctx assignment
            if (i->config.write_ctx)
                i->config.write_fn2(i->output.samples, bytes_to_write, &bytes_written, portMAX_DELAY, i->config.write_ctx);
            else
                i->config.write_fn(i->output.samples, bytes_to_write, &bytes_written, portMAX_DELAY);

            if(bytes_to_write != bytes_written) {
                ESP_LOGE(TAG, "to write %d != written %d", bytes_to_write, bytes_written);
            }
        } else if(decode_status == DECODE_STATUS_NO_DATA_CONTINUE)
        {
            LOGI_2("no data");
        } else { // DECODE_STATUS_DONE || DECODE_STATUS_ERROR
            LOGI_1("breaking out of playback");
            break;
        }
    } while (true);

clean_up:
    return ret;
}

static void audio_task(void *pvParam) {
    audio_instance_t *i = static_cast<audio_instance_t*>(pvParam);
    audio_player_event_t audio_event;

    while (true) {
        // pull items off of the queue until we run into a PLAY request
        while(true) {
            // zero delay in the case where we are playing as we want to
            // send an event indicating either
            // PLAYING -> IDLE (IDLE) or PLAYING -> PLAYING (COMPLETED PLAYING NEXT)
            // and thus don't want to block until the next request comes in
            // in the case when there are no further requests pending
            int delay = (i->state == AUDIO_PLAYER_STATE_PLAYING) ? 0 : portMAX_DELAY;

            int retval = xQueuePeek(i->event_queue, &audio_event, delay);
            if (pdPASS == retval) { // item on the queue, process it
                xQueueReceive(i->event_queue, &audio_event, 0);

                // if the item is a play request, process it
                if(AUDIO_PLAYER_REQUEST_PLAY == audio_event.type) {
                    if(i->state == AUDIO_PLAYER_STATE_PLAYING) {
                        dispatch_callback(i, AUDIO_PLAYER_CALLBACK_EVENT_COMPLETED_PLAYING_NEXT);
                    } else {
                        set_state(i, AUDIO_PLAYER_STATE_PLAYING);
                    }

                    break;
                } else if(AUDIO_PLAYER_REQUEST_SHUTDOWN_THREAD == audio_event.type) {
                    // Reject any callback-initiated or cross-core control event
                    // as soon as shutdown is committed.
                    i->running = false;
                    set_state(i, AUDIO_PLAYER_STATE_SHUTDOWN);
                    SemaphoreHandle_t stopped_sem = i->stopped_sem;

                    // No access to the instance is allowed after this point. The
                    // deleter owns all storage once it receives this semaphore.
                    if (stopped_sem) {
                        xSemaphoreGive(stopped_sem);
                    }
                    vTaskDelete(NULL);
                    break;
                } else {
                    // ignore other events when not playing
                }
            } else { // no items on the queue
                // if we are playing transition to idle and indicate the transition via callback
                if(i->state == AUDIO_PLAYER_STATE_PLAYING) {
                    set_state(i, AUDIO_PLAYER_STATE_IDLE);
                }
            }
        }

        if (i->config.mute_fn) i->config.mute_fn(AUDIO_PLAYER_UNMUTE);
        esp_err_t ret_val = aplay_file(i, audio_event.fp);
        if(ret_val != ESP_OK)
        {
            ESP_LOGE(TAG, "aplay_file() %d", ret_val);
        }
        if (i->config.mute_fn) i->config.mute_fn(AUDIO_PLAYER_MUTE);

        if(audio_event.fp) fclose(audio_event.fp);
    }
}

/* **************** AUDIO PLAY CONTROL **************** */
static esp_err_t audio_send_event(audio_instance_t *i, audio_player_event_t event) {
    ESP_RETURN_ON_FALSE(i->running, ESP_ERR_INVALID_STATE,
        TAG, "Audio task is shutting down");
    ESP_RETURN_ON_FALSE(NULL != i->event_queue, ESP_ERR_INVALID_STATE,
        TAG, "Audio task not started yet");

    BaseType_t ret_val = xQueueSend(i->event_queue, &event, 0);

    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_ERR_INVALID_STATE,
        TAG, "The last event has not been processed yet");

    return ESP_OK;
}

/* ================= New multi-instance API ================= */

esp_err_t audio_instance_play(audio_instance_handle_t h, FILE *fp) {
    audio_instance_t *i = static_cast<audio_instance_t *>(h);
    CHECK_INSTANCE(i);

    LOGI_1("%s", __FUNCTION__);
    audio_player_event_t event = { .type = AUDIO_PLAYER_REQUEST_PLAY, .fp = fp };
    return audio_send_event(i, event);
}

esp_err_t audio_instance_pause(audio_instance_handle_t h) {
    audio_instance_t *i = static_cast<audio_instance_t *>(h);
    CHECK_INSTANCE(i);

    LOGI_1("%s", __FUNCTION__);
    audio_player_event_t event = { .type = AUDIO_PLAYER_REQUEST_PAUSE, .fp = NULL };
    return audio_send_event(i, event);
}

esp_err_t audio_instance_resume(audio_instance_handle_t h) {
    audio_instance_t *i = static_cast<audio_instance_t *>(h);
    CHECK_INSTANCE(i);

    LOGI_1("%s", __FUNCTION__);
    audio_player_event_t event = { .type = AUDIO_PLAYER_REQUEST_RESUME, .fp = NULL };
    return audio_send_event(i, event);
}

esp_err_t audio_instance_stop(audio_instance_handle_t h) {
    audio_instance_t *i = static_cast<audio_instance_t *>(h);
    CHECK_INSTANCE(i);

    LOGI_1("%s", __FUNCTION__);
    audio_player_event_t event = { .type = AUDIO_PLAYER_REQUEST_STOP, .fp = NULL };
    return audio_send_event(i, event);
}

/**
 * Can only shut down the playback thread if the thread is not presently playing audio.
 * Call audio_player_stop()
 */
static esp_err_t _internal_audio_player_shutdown_thread(audio_instance_t *i) {
    CHECK_INSTANCE(i);

    LOGI_1("%s", __FUNCTION__);
    audio_player_event_t event = { .type = AUDIO_PLAYER_REQUEST_SHUTDOWN_THREAD, .fp = NULL };
    ESP_RETURN_ON_FALSE(NULL != i->event_queue, ESP_ERR_INVALID_STATE,
        TAG, "Audio task not started yet");
    BaseType_t sent = xQueueSendToFront(i->event_queue, &event, pdMS_TO_TICKS(100));
    ESP_RETURN_ON_FALSE(pdPASS == sent, ESP_ERR_TIMEOUT,
        TAG, "Audio shutdown request queue is full");
    return ESP_OK;
}

static void cleanup_memory(audio_instance_t *i) {
#if defined(CONFIG_AUDIO_PLAYER_ENABLE_MP3)
    if(i->mp3_decoder) MP3FreeDecoder(i->mp3_decoder);
    if(i->mp3_data.data_buf) free(i->mp3_data.data_buf);
#endif
    if(i->output.samples) free(i->output.samples);

    if (i->event_queue) {
        // PLAY requests transfer FILE ownership to the player. Close requests
        // that were still queued when the worker accepted SHUTDOWN.
        audio_player_event_t pending = {};
        while (xQueueReceive(i->event_queue, &pending, 0) == pdPASS) {
            if (pending.type == AUDIO_PLAYER_REQUEST_PLAY && pending.fp) {
                fclose(pending.fp);
            }
        }
        vQueueDelete(i->event_queue);
        i->event_queue = NULL;
    }
    if (i->stopped_sem) {
        vSemaphoreDelete(i->stopped_sem);
        i->stopped_sem = NULL;
    }
    if (i->delete_gate) {
        vSemaphoreDelete(i->delete_gate);
        i->delete_gate = NULL;
    }
}

esp_err_t audio_instance_new(audio_instance_handle_t *h, audio_player_config_t *config) {
    BaseType_t task_val;
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(h != NULL, ESP_ERR_INVALID_ARG, TAG, "handle pointer is NULL");
    ESP_RETURN_ON_FALSE(*h == NULL, ESP_ERR_INVALID_ARG, TAG, "instance is not NULL");
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "null config");

    audio_instance_t *i = static_cast<audio_instance_t *>(calloc(1, sizeof(audio_instance_t)));
    if (i == NULL) return ESP_ERR_NO_MEM;

    audio_instance_init(i);

    i->config = *config;

    /* Audio control event queue */
    i->event_queue = xQueueCreate(4, sizeof(audio_player_event_t));
    ESP_GOTO_ON_FALSE(NULL != i->event_queue, ESP_ERR_NO_MEM, cleanup,
        TAG, "xQueueCreate");
    i->stopped_sem = xSemaphoreCreateBinary();
    ESP_GOTO_ON_FALSE(NULL != i->stopped_sem, ESP_ERR_NO_MEM, cleanup,
        TAG, "xSemaphoreCreateBinary");
    i->delete_gate = xSemaphoreCreateBinary();
    ESP_GOTO_ON_FALSE(NULL != i->delete_gate, ESP_ERR_NO_MEM, cleanup,
        TAG, "xSemaphoreCreateBinary delete gate");
    xSemaphoreGive(i->delete_gate);

    /** See https://github.com/ultraembedded/libhelix-mp3/blob/0a0e0673f82bc6804e5a3ddb15fb6efdcde747cd/testwrap/main.c#L74 */
    i->output.samples_capacity = MAX_NCHAN * MAX_NGRAN * MAX_NSAMP;
    i->output.samples_capacity_max = i->output.samples_capacity * 2;
    i->output.samples = static_cast<uint8_t*>(malloc(i->output.samples_capacity_max));
    LOGI_1("samples_capacity %d bytes", i->output.samples_capacity_max);
    ESP_GOTO_ON_FALSE(NULL != i->output.samples, ESP_ERR_NO_MEM, cleanup,
        TAG, "Failed allocate output buffer");

#if defined(CONFIG_AUDIO_PLAYER_ENABLE_MP3)
    i->mp3_data.data_buf_size = MAINBUF_SIZE * 3;
    i->mp3_data.data_buf = static_cast<uint8_t*>(malloc(i->mp3_data.data_buf_size));
    ESP_GOTO_ON_FALSE(NULL != i->mp3_data.data_buf, ESP_ERR_NO_MEM, cleanup,
        TAG, "Failed allocate mp3 data buffer");

    i->mp3_decoder = MP3InitDecoder();
    ESP_GOTO_ON_FALSE(NULL != i->mp3_decoder, ESP_ERR_NO_MEM, cleanup,
        TAG, "Failed create MP3 decoder");
#endif

    memset(&i->i2s_format, 0, sizeof(i->i2s_format));

    i->running = true;
    task_val = xTaskCreatePinnedToCore(
        (TaskFunction_t)        audio_task,
                                "Audio Task",
                                4 * 1024,
                                i,
        (UBaseType_t)           i->config.priority,
        &i->task_handle,
        (BaseType_t)            i->config.coreID);

    ESP_GOTO_ON_FALSE(pdPASS == task_val, ESP_ERR_NO_MEM, cleanup,
        TAG, "Failed create audio task");

    // start muted
    if (i->config.mute_fn)
        i->config.mute_fn(AUDIO_PLAYER_MUTE);

    *h = i;
    return ret;

// At the moment when we run cppcheck there is a lack of esp-idf header files this
// means cppcheck doesn't know that ESP_GOTO_ON_FALSE() etc are making use of this label
// cppcheck-suppress unusedLabelConfiguration
cleanup:
    cleanup_memory(i);
    free(i);
    i = NULL;

    return ret;
}

esp_err_t audio_instance_delete(audio_instance_handle_t h) {
    audio_instance_t *i = static_cast<audio_instance_t *>(h);
    CHECK_INSTANCE(i);

    ESP_RETURN_ON_FALSE(xTaskGetCurrentTaskHandle() != i->task_handle,
        ESP_ERR_INVALID_STATE, TAG, "Audio worker cannot delete itself");
    ESP_RETURN_ON_FALSE(i->stopped_sem != NULL, ESP_ERR_INVALID_STATE,
        TAG, "Audio worker completion semaphore is unavailable");
    ESP_RETURN_ON_FALSE(i->delete_gate != NULL, ESP_ERR_INVALID_STATE,
        TAG, "Audio delete gate is unavailable");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(i->delete_gate, 0) == pdTRUE,
        ESP_ERR_INVALID_STATE, TAG, "Audio instance deletion is already in progress");

    const TickType_t timeout = pdMS_TO_TICKS(5000);
    const TickType_t started = xTaskGetTickCount();
    bool worker_stopped = false;
    while ((xTaskGetTickCount() - started) < timeout) {
        if (xSemaphoreTake(i->stopped_sem, 0) == pdTRUE) {
            worker_stopped = true;
            break;
        }

        const esp_err_t request_result =
            _internal_audio_player_shutdown_thread(i);
        if (request_result == ESP_OK) {
            const TickType_t elapsed = xTaskGetTickCount() - started;
            const TickType_t remaining = elapsed < timeout ? timeout - elapsed : 0;
            worker_stopped = xSemaphoreTake(i->stopped_sem, remaining) == pdTRUE;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // A timed-out worker still owns and may access the queue, codec callback,
    // decoder buffers, and FILE. Keep the complete instance alive for retry.
    if (!worker_stopped) {
        // The instance is intentionally retained, so permit a later retry.
        xSemaphoreGive(i->delete_gate);
        ESP_LOGE(TAG, "Timed out waiting for audio worker shutdown");
        return ESP_ERR_TIMEOUT;
    }

    i->task_handle = NULL;
    cleanup_memory(i);
    free(i);
    i = NULL;

    return ESP_OK;
}

/* ================= Legacy API implemented via default instance ================= */

audio_player_state_t audio_player_get_state() {
    return audio_instance_get_state(g_instance);
}

esp_err_t audio_player_callback_register(audio_player_cb_t call_back, void *user_ctx) {
    return audio_instance_callback_register(g_instance, call_back, user_ctx);
}

esp_err_t audio_player_play(FILE *fp) {
    return audio_instance_play(g_instance, fp);
}

esp_err_t audio_player_pause() {
    return audio_instance_pause(g_instance);
}

esp_err_t audio_player_resume() {
    return audio_instance_resume(g_instance);
}

esp_err_t audio_player_stop() {
    return audio_instance_stop(g_instance);
}

esp_err_t audio_player_new(audio_player_config_t config) {
    portENTER_CRITICAL(&g_legacy_lifecycle_mux);
    if (g_legacy_lifecycle_busy) {
        portEXIT_CRITICAL(&g_legacy_lifecycle_mux);
        return ESP_ERR_INVALID_STATE;
    }
    if (g_instance) {
        portEXIT_CRITICAL(&g_legacy_lifecycle_mux);
        return ESP_OK;
    }
    g_legacy_lifecycle_busy = true;
    portEXIT_CRITICAL(&g_legacy_lifecycle_mux);

    config.force_stereo = true;    // preserve legacy behavior
    audio_instance_handle_t h = NULL;
    const esp_err_t ret = audio_instance_new(&h, &config);

    portENTER_CRITICAL(&g_legacy_lifecycle_mux);
    if (ret == ESP_OK) {
        g_instance = static_cast<audio_instance_t *>(h);
    }
    g_legacy_lifecycle_busy = false;
    portEXIT_CRITICAL(&g_legacy_lifecycle_mux);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to create new audio instance: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t audio_player_delete() {
    portENTER_CRITICAL(&g_legacy_lifecycle_mux);
    if (g_legacy_lifecycle_busy) {
        portEXIT_CRITICAL(&g_legacy_lifecycle_mux);
        return ESP_ERR_INVALID_STATE;
    }
    audio_instance_t *instance = g_instance;
    if (!instance) {
        portEXIT_CRITICAL(&g_legacy_lifecycle_mux);
        return ESP_OK;
    }
    g_legacy_lifecycle_busy = true;
    portEXIT_CRITICAL(&g_legacy_lifecycle_mux);

    const esp_err_t ret = audio_instance_delete(instance);

    portENTER_CRITICAL(&g_legacy_lifecycle_mux);
    if (ret == ESP_OK && g_instance == instance) {
        g_instance = NULL;
    }
    g_legacy_lifecycle_busy = false;
    portEXIT_CRITICAL(&g_legacy_lifecycle_mux);

    return ret;
}
