idf_component_register(SRCS "hex.c" "main.c"
                    INCLUDE_DIRS "."
REQUIRES app_update nvs_flash esp_wifi esp_event esp_netif esp_driver_ledc esp_http_server esp_timer led_strip json                    
)