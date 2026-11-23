idf_component_register(
    SRCS "system_status.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        brookesia_core
        esp32_s3_touch_amoled_1_75
        esp_driver_i2c
        esp_event
        esp_netif
        esp_wifi
        nvs_flash
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
