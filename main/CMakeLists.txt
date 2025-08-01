idf_component_register(SRCS "main.cpp"
                    PRIV_REQUIRES spi_flash
                    REQUIRES giac console driver Adafruit_SSD1306
                    INCLUDE_DIRS "")

project(esp_giac)
