set(src_dirs
"KEY_MANAGER"
"SENSOR_MANAGER"
"SENSOR_MQTT_BRIDGE"
"LED_STATUS"
)


set(include_dirs
"KEY_MANAGER"
"SENSOR_MANAGER"
"SENSOR_MQTT_BRIDGE"
"LED_STATUS"
)


set(requires
BSP
Middlewares
Protocol
)


idf_component_register(
SRC_DIRS ${src_dirs}
INCLUDE_DIRS ${include_dirs}
REQUIRES ${requires}
)