idf_component_register(SRCS "http_server.c" "wifi_app.c" "hex.c" "main.c"
                    INCLUDE_DIRS "."
    REQUIRES app_update nvs_flash esp_wifi esp_event esp_netif esp_driver_ledc esp_driver_gpio esp_http_server esp_timer led_strip json
    EMBED_FILES webpage/style.css webpage/script.js webpage/favicon.png webpage/index.html

)