idf_component_register(
    SRCS "storage_service.c"
    INCLUDE_DIRS "include"
    REQUIRES esp32_s3_touch_amoled_1_75 fatfs esp_hw_support esp_timer
)
