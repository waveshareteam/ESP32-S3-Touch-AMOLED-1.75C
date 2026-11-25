#pragma once

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/sdmmc_host.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "bsp/config.h"
#include "bsp/display.h"
#include "esp_codec_dev.h"


#include "lvgl.h"
#include "esp_lv_adapter.h"


/**************************************************************************************************
 *  BSP Capabilities
 **************************************************************************************************/

#define BSP_CAPS_DISPLAY        1
#define BSP_CAPS_TOUCH          1
#define BSP_CAPS_BUTTONS        0
#define BSP_CAPS_AUDIO          1
#define BSP_CAPS_AUDIO_SPEAKER  1
#define BSP_CAPS_AUDIO_MIC      1
#define BSP_CAPS_SDCARD         0
#define BSP_CAPS_IMU            0

/**************************************************************************************************
 * ESP-SparkBot-BSP pinout
 **************************************************************************************************/

/* I2C */
#define BSP_I2C_SCL           (GPIO_NUM_14)
#define BSP_I2C_SDA           (GPIO_NUM_15)

#define BSP_I2S_SCLK          (GPIO_NUM_9)
#define BSP_I2S_MCLK          (GPIO_NUM_16)
#define BSP_I2S_LCLK          (GPIO_NUM_45)
#define BSP_I2S_DOUT          (GPIO_NUM_8)
#define BSP_I2S_DSIN          (GPIO_NUM_10)
#define BSP_POWER_AMP_IO      (GPIO_NUM_46)

/* Display */
#define BSP_LCD_CS        (GPIO_NUM_12)
#define BSP_LCD_PCLK      (GPIO_NUM_38)
#define BSP_LCD_DATA0     (GPIO_NUM_4)
#define BSP_LCD_DATA1     (GPIO_NUM_5)
#define BSP_LCD_DATA2     (GPIO_NUM_6)
#define BSP_LCD_DATA3     (GPIO_NUM_7)

#define BSP_LCD_BACKLIGHT     (GPIO_NUM_NC)
#define BSP_LCD_RST           (GPIO_NUM_1)
#define BSP_LCD_TOUCH_RST     (GPIO_NUM_2)
#define BSP_LCD_TOUCH_INT     (GPIO_NUM_11)

/* KEY1 is the ESP32-S3 BOOT strap. KEY2 mirrors the AXP2101 PWRON key through a BSS138 stage. */
#define BSP_BOOT_KEY_IO       (GPIO_NUM_0)
#define BSP_PWR_KEY_IO        (GPIO_NUM_3)

#define LVGL_BUFFER_HEIGHT          (CONFIG_BSP_DISPLAY_LVGL_BUF_HEIGHT)

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 *
 * I2C interface
 *
 * There are two devices connected to I2C peripheral:
 *  - QMA7981 Inertial measurement unit
 *  - OV2640 Camera module
 **************************************************************************************************/
#define BSP_I2C_NUM     CONFIG_BSP_I2C_NUM

/**
 * @brief Init I2C driver
 *
 * @return
 *      - ESP_OK                On success
 *      - ESP_ERR_INVALID_ARG   I2C parameter error
 *      - ESP_FAIL              I2C driver installation error
 *
 */
esp_err_t bsp_i2c_init(void);

/**
 * @brief Deinit I2C driver and free its resources
 *
 * @return
 *      - ESP_OK                On success
 *      - ESP_ERR_INVALID_ARG   I2C parameter error
 *
 */
esp_err_t bsp_i2c_deinit(void);

/**
 * @brief Get I2C driver handle
 *
 * @return
 *      - I2C handle
 *
 */
i2c_master_bus_handle_t bsp_i2c_get_handle(void);


/*
 * ES7210 physical-input masks. The board connects MIC1 and MIC2 to the two
 * front microphones, and routes the ES8311 analog playback signal to MIC3 as
 * the acoustic-echo reference. MIC4 is not connected.
 *
 * These values are for es7210_codec_cfg_t.mic_selected and
 * esp_codec_dev_set_in_channel_gain(); they are not serialized TDM masks.
 */
#define BSP_AUDIO_ES7210_MIC1_FL_MASK          (1U << 0)
#define BSP_AUDIO_ES7210_MIC2_FR_MASK          (1U << 1)
#define BSP_AUDIO_ES7210_MIC3_RE_MASK          (1U << 2)
#define BSP_AUDIO_ES7210_MIC4_NA_MASK          (1U << 3)
#define BSP_AUDIO_ES7210_MIC_MASK_FL_FR        \
    (BSP_AUDIO_ES7210_MIC1_FL_MASK | BSP_AUDIO_ES7210_MIC2_FR_MASK)
#define BSP_AUDIO_ES7210_CONNECTED_MIC_MASK    \
    (BSP_AUDIO_ES7210_MIC_MASK_FL_FR | BSP_AUDIO_ES7210_MIC3_RE_MASK)

/*
 * ES7210 serialized TDM order: MIC1, MIC3(reference), MIC2, MIC4. These masks
 * belong only to esp_codec_dev_sample_info_t.channel_mask.
 */
#define BSP_AUDIO_TDM_SLOT_COUNT               (4U)
#define BSP_AUDIO_TDM_SLOT_FL                  (0U)
#define BSP_AUDIO_TDM_SLOT_RE                  (1U)
#define BSP_AUDIO_TDM_SLOT_FR                  (2U)
#define BSP_AUDIO_TDM_SLOT_NA                  (3U)
#define BSP_AUDIO_TDM_SLOT_MASK_FL             ESP_CODEC_DEV_MAKE_CHANNEL_MASK(BSP_AUDIO_TDM_SLOT_FL)
#define BSP_AUDIO_TDM_SLOT_MASK_RE             ESP_CODEC_DEV_MAKE_CHANNEL_MASK(BSP_AUDIO_TDM_SLOT_RE)
#define BSP_AUDIO_TDM_SLOT_MASK_FR             ESP_CODEC_DEV_MAKE_CHANNEL_MASK(BSP_AUDIO_TDM_SLOT_FR)
#define BSP_AUDIO_TDM_SLOT_MASK_NA             ESP_CODEC_DEV_MAKE_CHANNEL_MASK(BSP_AUDIO_TDM_SLOT_NA)
#define BSP_AUDIO_TDM_SLOT_MASK_FL_FR           \
    (BSP_AUDIO_TDM_SLOT_MASK_FL | BSP_AUDIO_TDM_SLOT_MASK_FR)

/**************************************************************************************************
 *
 * I2S audio interface
 *
 * ES8311 drives playback over standard stereo I2S. ES7210 captures four TDM
 * slots on the same physical clocks. Codec handles returned here are BSP-owned.
 **************************************************************************************************/

/**
 * @brief Initialize legacy standard-I2S TX and RX audio.
 *
 * @param[in] i2s_config Pass NULL for mono, duplex, 16-bit, 22050 Hz.
 * @return
 *      - ESP_OK                On success
 *      - ESP_ERR_NOT_SUPPORTED The communication mode is not supported on the current chip
 *      - ESP_ERR_INVALID_ARG   NULL pointer or invalid configuration
 *      - ESP_ERR_NOT_FOUND     No available I2S channel found
 *      - ESP_ERR_NO_MEM        No memory for storing the channel information
 *      - ESP_ERR_INVALID_STATE This channel has not initialized or already started
 */
esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config);

/**
 * @brief Initialize standard-I2S TX and four-slot TDM RX.
 *
 * TX and RX share MCLK/BCLK/LRCK, so both configurations must use the same
 * sample rate. Call this before creating codec devices.
 */
esp_err_t bsp_audio_init_tx_std_rx_tdm(const i2s_std_config_t *tx_config,
                                       const i2s_tdm_config_t *rx_config);

/**
 * @brief Initialize the board voice profile.
 *
 * The physical profile is 24 kHz, 16-bit stereo STD TX plus four-slot TDM RX,
 * with MCLK x256 and the ES7210 order MIC1/MIC3(reference)/MIC2/MIC4.
 */
esp_err_t bsp_audio_init_voice_24k(void);

/**
 * @brief Delete BSP-owned codec, data-interface, and I2S resources.
 *
 * Stop all codec users before calling. Returned codec handles become invalid.
 */
esp_err_t bsp_audio_deinit(void);

/**
 * @brief Initialize or return the BSP-owned ES8311 speaker codec device.
 *
 * @return Pointer to codec device handle or NULL when error occurred
 */
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void);

/**
 * @brief Initialize or return the BSP-owned ES7210 microphone codec device.
 *
 * @return Pointer to codec device handle or NULL when error occurred
 */
esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void);

/**************************************************************************************************
 *
 * SPIFFS
 *
 * After mounting the SPIFFS, it can be accessed with stdio functions ie.:
 * \code{.c}
 * FILE* f = fopen(BSP_SPIFFS_MOUNT_POINT"/hello.txt", "w");
 * fprintf(f, "Hello World!\n");
 * fclose(f);
 * \endcode
 **************************************************************************************************/
#define BSP_SPIFFS_MOUNT_POINT      CONFIG_BSP_SPIFFS_MOUNT_POINT

/**
 * @brief Mount SPIFFS to virtual file system
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if esp_vfs_spiffs_register was already called
 *      - ESP_ERR_NO_MEM if memory can not be allocated
 *      - ESP_FAIL if partition can not be mounted
 *      - other error codes
 */
esp_err_t bsp_spiffs_mount(void);

/**
 * @brief Unmount SPIFFS from virtual file system
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if already unmounted
 */
esp_err_t bsp_spiffs_unmount(void);

/**************************************************************************************************
 *
 * uSD card
 *
 * The ESP32-S3-Touch-AMOLED-1.75C has no microSD slot. GPIO1 and GPIO2 drive the LCD and touch
 * resets, so the mount entry points never configure SDMMC and callers see a missing card.
 **************************************************************************************************/
#define BSP_SD_MOUNT_POINT      CONFIG_BSP_SD_MOUNT_POINT
extern sdmmc_card_t *bsp_sdcard;

/**
 * @brief Report that this board has no microSD card
 *
 * @return ESP_ERR_NOT_SUPPORTED always
 */
esp_err_t bsp_sdcard_mount(void);

/**
 * @brief Report that no microSD card is mounted
 *
 * @return ESP_ERR_INVALID_STATE always
 */
esp_err_t bsp_sdcard_unmount(void);


/**************************************************************************************************
 *
 * LCD interface
 *
 * LVGL is used as graphics library. LVGL is NOT thread safe, therefore the user must take LVGL mutex
 * by calling bsp_display_lock() before calling any LVGL API (lv_...) and then give the mutex with
 * bsp_display_unlock().
 *
 * If you want to use the display without LVGL, see bsp/display.h API and use BSP version with 'noglib' suffix.
 **************************************************************************************************/
#define BSP_LCD_SPI_NUM            (SPI2_HOST)

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)

/**
 * @brief BSP display configuration structure
 */
typedef struct {
    esp_lv_adapter_config_t          lv_adapter_cfg;
    esp_lv_adapter_rotation_t        rotation;
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode;
    struct {
        unsigned int swap_xy;  /*!< Swap X and Y after read coordinates */
        unsigned int mirror_x; /*!< Mirror X after read coordinates */
        unsigned int mirror_y; /*!< Mirror Y after read coordinates */
    } touch_flags;
} bsp_display_cfg_t;

/**
 * @brief Initialize display
 *
 * This function initializes SPI, display controller and starts LVGL handling task.
 *
 * @return Pointer to LVGL display or NULL when error occurred
 */
lv_display_t *bsp_display_start(void);

/**
 * @brief Initialize display
 *
 * This function initializes SPI, display controller and starts LVGL handling task.
 * LCD backlight must be enabled separately by calling bsp_display_brightness_set()
 *
 * @param cfg display configuration
 *
 * @return Pointer to LVGL display or NULL when error occurred
 */
lv_display_t *bsp_display_start_with_config(bsp_display_cfg_t *cfg);

/**
 * @brief Get pointer to input device (touch, buttons, ...)
 *
 * @note The LVGL input device is initialized in bsp_display_start() function.
 *
 * @return Pointer to LVGL input device or NULL when not initialized
 */
lv_indev_t *bsp_display_get_input_dev(void);

/**
 * @brief Take LVGL mutex
 *
 * @param timeout_ms Timeout in [ms]. -1 will block indefinitely.
 * @return true  Mutex was taken
 * @return false Mutex was NOT taken
 */
esp_err_t bsp_display_lock(uint32_t timeout_ms);

/**
 * @brief Give LVGL mutex
 *
 */
void bsp_display_unlock(void);
#endif // BSP_CONFIG_NO_GRAPHIC_LIB == 0

#ifdef __cplusplus
}
#endif
