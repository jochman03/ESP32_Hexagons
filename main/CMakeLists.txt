idf_component_register(SRCS "state_save.c" "http_server.c" "wifi.c" "hex.c" "main.c" "config.c"
                    INCLUDE_DIRS "."
    REQUIRES app_update nvs_flash esp_wifi esp_event esp_netif esp_driver_ledc esp_driver_gpio esp_http_server esp_timer led_strip json
    EMBED_FILES webpage/style.css webpage/script.js webpage/favicon.png webpage/index.html webpage/config.html webpage/config_script.js

)