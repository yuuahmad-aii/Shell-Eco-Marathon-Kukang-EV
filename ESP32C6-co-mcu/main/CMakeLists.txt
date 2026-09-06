idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_driver_uart esp_driver_gpio esp_http_client nvs_flash esp_wifi mbedtls esp_netif)
