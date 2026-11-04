idf_component_register(
    SRCS
        "src/network_wifi.c"
        "src/network_http_server.c"
        "src/network_ota.c"
        "src/network_iot_client.c"
        "src/network_maintenance.c"
        "src/network_self_test.c"

    INCLUDE_DIRS
        "inc"

    REQUIRES
        app
        esp_netif

    PRIV_REQUIRES
        esp_wifi
        esp_event
        esp_http_server
        esp_http_client
        esp_timer
        app_update
        esp_partition
        esp_hw_support
        nvs_flash
        mbedtls
        json
        lwip

    EMBED_FILES
        "webpage/style.css"
        "webpage/script.js"
        "webpage/favicon.png"
        "webpage/index.html"
        "webpage/config.html"
        "webpage/config_script.js"
)