set(src_dirs
    "NVS"
    "JSON"
)

set(include_dirs
    "NVS"
    "JSON"
)

set(requires
    cjson    
    nvs_flash
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