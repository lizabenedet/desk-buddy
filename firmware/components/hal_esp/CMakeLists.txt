idf_component_register(
    SRCS "display.c" "esp_lcd_ili9486.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES esp_lcd
    PRIV_REQUIRES esp_driver_spi esp_driver_gpio
)
