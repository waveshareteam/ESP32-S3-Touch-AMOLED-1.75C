idf_component_register(
    SRCS "display_power.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        esp32_s3_touch_amoled_1_75
        esp_driver_gpio
        lvgl__lvgl
        nvs_flash
)
