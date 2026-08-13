set(src_dirs
"LED"
"KEY"
"USAR"
"MOS"
"MQTT"
"Protocol"
"WIFI/WIFI_MODE"
"WIFI/WIFI_MANAGER"
"WIFI/WIFI_CONFIG"
"WIFI/WIFI_SCAN"
"HTTP_SERVER"
)

set(include_dirs
"LED"
"KEY"
"USAR"
"MOS"
"MQTT"
"Protocol"
"WIFI/WIFI_MODE"
"WIFI/WIFI_MANAGER"
"WIFI/WIFI_CONFIG"
"WIFI/WIFI_SCAN"
"HTTP_SERVER"
)

set(embed_files
"HTTP_SERVER/web/index.html"
"HTTP_SERVER/web/app.js"
"HTTP_SERVER/web/style.css"
)


set(requires
esp_driver_gpio
esp_wifi
esp_event
esp_netif
nvs_flash
esp_http_server
espressif__cjson
esp_driver_uart
mqtt
esp_timer
)

idf_component_register(
SRC_DIRS ${src_dirs}
INCLUDE_DIRS ${include_dirs}
REQUIRES ${requires}
EMBED_FILES ${embed_files}
)

component_compile_options(
-ffast-math
-O3
-Wno-error=format
-Wno-format
)