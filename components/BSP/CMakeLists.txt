set(src_dirs
"LED"
"KEY"
"UART"
"MOS"
"Device"
)


set(include_dirs
"LED"
"KEY"
"UART"
"MOS"
"Device"
)


set(requires
esp_driver_gpio
esp_driver_uart
efuse
esp_timer
)


idf_component_register(
SRC_DIRS ${src_dirs}
INCLUDE_DIRS ${include_dirs}
REQUIRES ${requires}
)


component_compile_options(
-Wno-error=format
-Wno-format
)