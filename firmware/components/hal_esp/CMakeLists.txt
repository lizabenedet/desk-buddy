idf_component_register(
    SRCS "display.c" "esp_lcd_ili9486.c" "net.c" "weather_fetch.c" "pc_server.c" "web_routes.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES esp_lcd freertos
    PRIV_REQUIRES core esp_driver_spi esp_driver_gpio esp_wifi esp_netif esp_event esp_timer
                  esp_http_client esp_http_server mbedtls nvs_flash lwip app_update esp_app_format
    EMBED_TXTFILES "../../web/index.html"
)
