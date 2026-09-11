set(src_dirs
    HTTP_SERVER
    MQTT
    WIFI/WIFI_MODE
    WIFI/WIFI_MANAGER
    WIFI/WIFI_CONFIG
    WIFI/WIFI_SCAN
    OTA
)

set(include_dirs
    HTTP_SERVER
    MQTT
    WIFI/WIFI_MODE
    WIFI/WIFI_MANAGER
    WIFI/WIFI_CONFIG
    WIFI/WIFI_SCAN
    OTA
)

set(embed_files
    "HTTP_SERVER/web/index.html"
    "HTTP_SERVER/web/app.js"
    "HTTP_SERVER/web/style.css"
)

set(requires
    BSP
    Common
    esp_driver_gpio
    esp_driver_uart
    esp_wifi
    esp_event
    esp_netif
    nvs_flash
    esp_http_server
    cjson
    mqtt
    esp_timer
    efuse
    esp_https_ota
    esp_http_client
    esp_app_format
    app_update
)

idf_component_register(
    SRC_DIRS     ${src_dirs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES     ${requires}
    EMBED_FILES  ${embed_files}
)

component_compile_options(
    -Wno-error=format
    -Wno-format
)
