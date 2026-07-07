idf_component_register(SRCS "main.cpp"
                    "port_axp2101.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES esp_driver_gpio esp_driver_i2c)
