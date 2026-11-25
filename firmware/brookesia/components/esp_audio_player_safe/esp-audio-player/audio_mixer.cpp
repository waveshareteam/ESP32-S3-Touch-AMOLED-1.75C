/**
 * @file audio_mixer.cpp
 */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/queue.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"
#include "esp_check.h"
#include "esp_log.h"

#include "audio_mixer.h"
#include "audio_player.h"
#include "audio_instance.h"
#include "audio_stream.h"

static const char *TAG = "audio_mixer";

static TaskHandle_t s_mixer_task = NULL;
static audio_mixer_config_t s_cfg = {};
static volatile bool s_running = false;
static audio_mixer_cb_t s_mixer_user_cb = NULL;

typedef struct audio_stream {
    audio_stream_type_t type;
    char name[16];
    audio_instance_handle_t instance;
    QueueHandle_t file_queue;
    RingbufHandle_t pcm_rb;
    audio_player_state_t state;  // used only for RAW stream types.

    SLIST_ENTRY(audio_stream) next;
} audio_stream_t;

SLIST_HEAD(audio_stream_list, audio_stream);
static audio_stream_list s_stream_list = SLIST_HEAD_INITIALIZER(s_stream_list);
static uint32_t s_stream_name_counter = 0;  // counter for unique naming (monotonic)
static uint32_t s_active_streams = 0;       // counter for stream counting
static SemaphoreHandle_t s_stream_mutex = NULL;


static int16_t sat_add16(int32_t a, int32_t b) {
    int32_t s = a + b;
    if (s > INT16_MAX) return INT16_MAX;
    if (s < INT16_MIN) return INT16_MIN;
    return (int16_t)s;
}

static void mixer_task(void *arg) {
    const size_t frames = 512; // tune as needed
    const size_t bytes = frames * s_cfg.i2s_format.channels * sizeof(int16_t);

    int16_t *mix = static_cast<int16_t *>(heap_caps_malloc(bytes, MALLOC_CAP_8BIT));
    ESP_ERROR_CHECK(mix == NULL);

    while (s_running) {
        memset(mix, 0, bytes);

        audio_mixer_lock();

        audio_stream_t *stream;
        SLIST_FOREACH(stream, &s_stream_list, next) {
            if (!stream->pcm_rb) continue;

            size_t received_bytes = 0;
            void *item = xRingbufferReceiveUpTo(stream->pcm_rb, &received_bytes, pdMS_TO_TICKS(5), bytes);

            if (item && received_bytes > 0) {
                int16_t *samples = static_cast<int16_t *>(item);
                size_t count = received_bytes / sizeof(int16_t);

                for (size_t k = 0; k < count; ++k) {
                    mix[k] = sat_add16(mix[k], samples[k]);
                }
                vRingbufferReturnItem(stream->pcm_rb, item);
            } else if (item) {
                vRingbufferReturnItem(stream->pcm_rb, item);
            }
        }

        audio_mixer_unlock();

        size_t written = 0;
        if (s_cfg.write_fn) {
            s_cfg.write_fn(mix, bytes, &written, portMAX_DELAY);
            if (written != bytes) {
                ESP_LOGW(TAG, "mixer short write %u/%u", (unsigned)written, (unsigned)bytes);
            }
        }
    }

    free(mix);
    vTaskDelete(NULL);
}

IRAM_ATTR static esp_err_t mixer_stream_write(void *data, size_t size, size_t *bytes_written, uint32_t timeout, void *stream) {
    audio_stream_t *s = static_cast<audio_stream_t *>(stream);
    if (!s || !s->pcm_rb) {
        if (bytes_written) *bytes_written = 0;
        return ESP_ERR_INVALID_ARG;
    }

    /* send data to the stream's ring buffer */
    BaseType_t res = xRingbufferSend(s->pcm_rb, data, size, timeout);
    if (res == pdTRUE) {
        if (bytes_written) *bytes_written = size;
    } else {
        if (bytes_written) *bytes_written = 0;
        ESP_LOGW(TAG, "stream ringbuf full");
    }
    return ESP_OK;
}

static esp_err_t mixer_stream_clk_set_fn(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch) {
    if (rate != s_cfg.i2s_format.sample_rate) {
        ESP_LOGE(TAG, "stream sample rate mismatch: %lu Hz (mixer expects %u Hz)", rate, s_cfg.i2s_format.sample_rate);
        return ESP_ERR_INVALID_ARG;
    }

    if (bits_cfg != s_cfg.i2s_format.bits_per_sample) {
        ESP_LOGE(TAG, "stream bit depth mismatch: %lu bits (mixer expects %lu bits)", bits_cfg, s_cfg.i2s_format.bits_per_sample);
        return ESP_ERR_INVALID_ARG;
    }

    if (ch != s_cfg.i2s_format.channels) {
        ESP_LOGE(TAG, "stream channels mismatch: %u (mixer expects %lu)", ch, s_cfg.i2s_format.channels);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static void mixer_stream_event_handler(audio_player_cb_ctx_t *ctx) {
    if (!ctx || !ctx->user_ctx) return;

    audio_stream_t *s = static_cast<audio_stream_t *>(ctx->user_ctx);

    // handle auto-queueing
    if (ctx->audio_event == AUDIO_PLAYER_CALLBACK_EVENT_IDLE) {
        if (s_stream_mutex) xSemaphoreTake(s_stream_mutex, portMAX_DELAY);

        // Check if there is anything in the queue to play next
        FILE *next_fp = NULL;
        if (xQueueReceive(s->file_queue, &next_fp, 0) == pdTRUE) {
            ESP_LOGD(TAG, "stream '%s' auto-advancing queue", s->name);
            audio_instance_play(s->instance, next_fp);
        }
        audio_mixer_unlock();
    }

    // service callback
    if (s_mixer_user_cb) {
        s_mixer_user_cb(ctx);
    }
}

static void mixer_free_stream_resources(audio_stream_t *s) {
    if (s->instance) audio_instance_delete(s->instance);
    if (s->pcm_rb) vRingbufferDelete(s->pcm_rb);
    if (s->file_queue) {
        FILE *fp = NULL;
        while(xQueueReceive(s->file_queue, &fp, 0) == pdTRUE) {
            if (fp) fclose(fp);
        }
        vQueueDelete(s->file_queue);
    }
    free(s);
}

/////////////////////////////

inline uint8_t audio_mixer_stream_count() {
    return s_active_streams;
}

inline void audio_mixer_lock() {
    if (s_stream_mutex) xSemaphoreTake(s_stream_mutex, portMAX_DELAY);
}

inline void audio_mixer_unlock() {
    if (s_stream_mutex) xSemaphoreGive(s_stream_mutex);
}

void audio_mixer_add_stream(audio_stream_handle_t h) {
    audio_mixer_lock();
    SLIST_INSERT_HEAD(&s_stream_list, static_cast<audio_stream_t*>(h), next);
    s_active_streams++;
    audio_mixer_unlock();
}

void audio_mixer_remove_stream(audio_stream_handle_t h) {
    audio_mixer_lock();
    SLIST_REMOVE(&s_stream_list, static_cast<audio_stream_t*>(h), audio_stream, next);
    if (s_active_streams > 0) s_active_streams--;
    audio_mixer_unlock();
}

void audio_mixer_get_output_format(uint32_t *sample_rate, uint32_t *bits_per_sample, uint32_t *channels) {
    if (sample_rate) *sample_rate = s_cfg.i2s_format.sample_rate;
    if (bits_per_sample) *bits_per_sample = s_cfg.i2s_format.bits_per_sample;
    if (channels) *channels = s_cfg.i2s_format.channels;
}

void audio_mixer_callback_register(audio_mixer_cb_t cb) {
    s_mixer_user_cb = cb;
}

esp_err_t audio_mixer_init(audio_mixer_config_t *cfg) {
    if (s_running) return ESP_OK;
    ESP_RETURN_ON_FALSE(cfg && cfg->write_fn && cfg->clk_set_fn, ESP_ERR_INVALID_ARG, TAG, "invalid mixer config");
    s_cfg = *cfg;

    i2s_slot_mode_t channel_setting = (s_cfg.i2s_format.channels == 1) ? I2S_SLOT_MODE_MONO : I2S_SLOT_MODE_STEREO;
    ESP_RETURN_ON_ERROR(s_cfg.clk_set_fn(s_cfg.i2s_format.sample_rate, s_cfg.i2s_format.bits_per_sample, channel_setting), TAG, "clk set failed");

    s_running = true;
    if (!s_stream_mutex) s_stream_mutex = xSemaphoreCreateMutex();

    SLIST_INIT(&s_stream_list);

    BaseType_t ok = xTaskCreatePinnedToCore(mixer_task, "audio_mixer", 4096, NULL, s_cfg.priority, &s_mixer_task, s_cfg.coreID);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_FAIL, TAG, "failed to start mixer");

    ESP_LOGD(TAG, "mixer started");
    return ESP_OK;
}

bool audio_mixer_is_initialized() {
    return s_mixer_task != NULL;
}

void audio_mixer_deinit() {
    if (!s_running) return;

    // Task will exit on next loop; no join primitive in FreeRTOS here.
    s_running = false;
    s_mixer_task = NULL;

    // Clean up any remaining channels (safe teardown)
    audio_mixer_lock();

    while (!SLIST_EMPTY(&s_stream_list)) {
        audio_stream_t *it = SLIST_FIRST(&s_stream_list);
        SLIST_REMOVE_HEAD(&s_stream_list, next);
        mixer_free_stream_resources(it);
    }
    s_active_streams = 0;

    audio_mixer_unlock();
}

/* ================= Stream (mixer channel) API ================= */

static void dispatch_callback(audio_stream_t *s, audio_player_callback_event_t event) {
    ESP_LOGD(TAG, "event '%s'", event_to_string(event));

#if CONFIG_IDF_TARGET_ARCH_XTENSA
    if (esp_ptr_executable(reinterpret_cast<void*>(s_mixer_user_cb))) {
#else
    if (reinterpret_cast<void*>(s_mixer_user_cb)) {
#endif
        audio_player_cb_ctx_t ctx = {
            .audio_event = event,
            .user_ctx = s,
        };
        s_mixer_user_cb(&ctx);
    }
}

static void stream_purge_ringbuf(audio_stream_t *s) {
    if (!s || !s->pcm_rb) return;

    size_t item_size;
    void *item;
    while ((item = xRingbufferReceive(s->pcm_rb, &item_size, 0)) != NULL) {
        vRingbufferReturnItem(s->pcm_rb, item);
    }
}

esp_err_t audio_stream_raw_send_event(audio_stream_handle_t h, audio_player_callback_event_t event) {
    audio_stream_t *s = h;
    CHECK_STREAM(s);

    if (s->type != AUDIO_STREAM_TYPE_RAW) return ESP_ERR_NOT_SUPPORTED;

    // NOTE: essentially made event_to_state()
    audio_player_state_t new_state = AUDIO_PLAYER_STATE_IDLE;
    switch (event) {
        case AUDIO_PLAYER_CALLBACK_EVENT_IDLE:
            new_state = AUDIO_PLAYER_STATE_IDLE;
            break;
        case AUDIO_PLAYER_CALLBACK_EVENT_PLAYING:
        case AUDIO_PLAYER_CALLBACK_EVENT_COMPLETED_PLAYING_NEXT:
            new_state = AUDIO_PLAYER_STATE_PLAYING;
            break;
        case AUDIO_PLAYER_CALLBACK_EVENT_SHUTDOWN:
            new_state = AUDIO_PLAYER_STATE_SHUTDOWN;
            break;
        default:
            new_state = AUDIO_PLAYER_STATE_IDLE;
            break;
    }

    if(s->state != new_state) {
        s->state = new_state;
        dispatch_callback(s, event);
    }
    return ESP_OK;
}

audio_player_state_t audio_stream_get_state(audio_stream_handle_t h) {
    audio_stream_t *s = h;
    if (!s) return AUDIO_PLAYER_STATE_IDLE;

    /* DECODER stream? defer to the instance state */
    if (s->type == AUDIO_STREAM_TYPE_DECODER) {
        return audio_instance_get_state(s->instance);
    }

    /* RAW stream? check if ringbuf has data */
    if (s->type == AUDIO_STREAM_TYPE_RAW) {
        // TODO: determine if checking ringbuf is valuable vs. having a stream emit its own state
        //       using the method audio_stream_raw_send_event().
        // if (!s->pcm_rb) return AUDIO_PLAYER_STATE_IDLE;
        //
        // // peek for any bytes
        // UBaseType_t items_waiting = 0;
        // vRingbufferGetInfo(s->pcm_rb, NULL, NULL, NULL, NULL, &items_waiting);
        //
        // if (items_waiting > 0)
        //     return AUDIO_PLAYER_STATE_PLAYING;
        return s->state;
    }

    return AUDIO_PLAYER_STATE_IDLE;
}

audio_stream_type_t audio_stream_get_type(audio_stream_handle_t h) {
    if (!h) return AUDIO_STREAM_TYPE_UNKNOWN;
    return h->type;
}

esp_err_t audio_stream_play(audio_stream_handle_t h, FILE *fp) {
    audio_stream_t *s = h;
    CHECK_STREAM(s);

    if (s->type != AUDIO_STREAM_TYPE_DECODER) {
        ESP_LOGE(TAG, "stream '%s' is not a decoder stream", s->name);
        return ESP_ERR_NOT_SUPPORTED;
    }

    // stop current playback?
    if (audio_instance_get_state(s->instance) == AUDIO_PLAYER_STATE_PLAYING)
        audio_stream_stop(s);

    return audio_instance_play(s->instance, fp);
}

esp_err_t audio_stream_queue(audio_stream_handle_t h, FILE *fp, bool play_now) {
    if (play_now) {
        return audio_stream_play(h, fp);
    }

    audio_stream_t *s = h;
    CHECK_STREAM(s);

    if (s->type != AUDIO_STREAM_TYPE_DECODER) {
        ESP_LOGE(TAG, "stream '%s' is not a decoder stream", s->name);
        return ESP_ERR_NOT_SUPPORTED;
    }

    audio_mixer_lock();

    // add to queue
    if (xQueueSend(s->file_queue, &fp, 0) != pdTRUE) {
        ESP_LOGE(TAG, "stream '%s' queue full", s->name);
        fclose(fp); // Take ownership and close if we can't queue
        audio_mixer_unlock();
        return ESP_FAIL;
    }

    // if stream is IDLE, we need to kickstart it
    if (audio_instance_get_state(s->instance) == AUDIO_PLAYER_STATE_IDLE) {
        FILE *next_fp = NULL;
        // pop the one we just pushed (or the one at head)
        if (xQueueReceive(s->file_queue, &next_fp, 0) == pdTRUE) {
            audio_instance_play(s->instance, next_fp);
        }
    }

    audio_mixer_unlock();
    return ESP_OK;
}

esp_err_t audio_stream_stop(audio_stream_handle_t h) {
    audio_stream_t *s = h;
    CHECK_STREAM(s);
    esp_err_t err = ESP_OK;

    if (s->type == AUDIO_STREAM_TYPE_DECODER) {
        // clear any pending queue items
        FILE *pending = NULL;
        while (xQueueReceive(s->file_queue, &pending, 0) == pdTRUE) {
            if (pending) fclose(pending);
        }

        err = audio_instance_stop(s->instance);
    }

    stream_purge_ringbuf(s);
    return err;
}

esp_err_t audio_stream_pause(audio_stream_handle_t h) {
    audio_stream_t *s = h;
    CHECK_STREAM(s);
    if (s->type != AUDIO_STREAM_TYPE_DECODER) return ESP_ERR_NOT_SUPPORTED;
    return audio_instance_pause(s->instance);
}

esp_err_t audio_stream_resume(audio_stream_handle_t h) {
    audio_stream_t *s = h;
    CHECK_STREAM(s);
    if (s->type != AUDIO_STREAM_TYPE_DECODER) return ESP_ERR_NOT_SUPPORTED;
    return audio_instance_resume(s->instance);
}

esp_err_t audio_stream_write_pcm(audio_stream_handle_t h, void *data, size_t size, uint32_t timeout_ms) {
    audio_stream_t *s = h;
    CHECK_STREAM(s);

    if (s->type != AUDIO_STREAM_TYPE_RAW) {
        ESP_LOGE(TAG, "stream '%s' is not a raw stream", s->name);
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (!s->pcm_rb) return ESP_ERR_INVALID_STATE;

    // Send data to the ring buffer (BYTEBUF type)
    BaseType_t res = xRingbufferSend(s->pcm_rb, data, size, pdMS_TO_TICKS(timeout_ms));
    if (res != pdTRUE) {
        ESP_LOGW(TAG, "stream '%s' overflow", s->name);
        return ESP_FAIL;
    }
    return ESP_OK;
}

audio_stream_handle_t audio_stream_new(audio_stream_config_t *cfg) {
    ESP_RETURN_ON_FALSE(cfg, NULL, TAG, "null config");

    audio_stream_t *stream = static_cast<audio_stream_t *>(calloc(1, sizeof(audio_stream_t)));
    stream->type = cfg->type;

    /* use provided name? */
    if (cfg->name[0] != '\0') {
        strncpy(stream->name, cfg->name, sizeof(stream->name) - 1);
        stream->name[sizeof(stream->name) - 1] = 0;
    }
    /* otherwise, generate a unique monotonic name */
    else {
        snprintf(stream->name, sizeof(stream->name), "stream_%lu", static_cast<unsigned long>(s_stream_name_counter++));
    }

    /* DECODER type stream? create a player instance and queue */
    if (cfg->type == AUDIO_STREAM_TYPE_DECODER) {
        // new player instance
        audio_player_config_t instance_cfg;
        instance_cfg.mute_fn = NULL;
        instance_cfg.clk_set_fn = mixer_stream_clk_set_fn;
        instance_cfg.coreID = cfg->coreID;
        instance_cfg.priority = cfg->priority;
        instance_cfg.force_stereo = false;
        instance_cfg.write_fn2 = mixer_stream_write;
        instance_cfg.write_ctx = stream;

        audio_instance_handle_t h = NULL;
        esp_err_t err = audio_instance_new(&h, &instance_cfg);

        if (err != ESP_OK) {
            free(stream);
            return NULL;
        }
        stream->instance = h;

        // create file queue & attach event handler
        stream->file_queue = xQueueCreate(4, sizeof(FILE*));
        audio_instance_callback_register(stream->instance, mixer_stream_event_handler, stream);
    }

    /* always create a ringbuffer */
    stream->pcm_rb = xRingbufferCreate(16 * 1024, RINGBUF_TYPE_BYTEBUF);

    if (!stream->pcm_rb || (cfg->type == AUDIO_STREAM_TYPE_DECODER && !stream->file_queue)) {
        if (stream->file_queue) vQueueDelete(stream->file_queue);
        if (stream->pcm_rb) vRingbufferDelete(stream->pcm_rb);
        if (stream->instance) audio_instance_delete(stream->instance);
        free(stream);
        return NULL;
    }

    /* add to stream tracking */
    audio_mixer_add_stream(stream);

    ESP_LOGI(TAG, "Created stream '%s' (active: %u)", stream->name, audio_mixer_stream_count());

    return stream;
}

esp_err_t audio_stream_delete(audio_stream_handle_t h) {
    audio_stream_t *s = h;
    CHECK_STREAM(s);

    /* remove from stream tracking */
    audio_mixer_remove_stream(s);

    /* cleanup stream */
    mixer_free_stream_resources(s);

    ESP_LOGI(TAG, "Deleted stream '%s' (active: %u)", s->name, audio_mixer_stream_count());

    return ESP_OK;
}
