set(src_dirs
"MOS_Protocol"
"SEN_Protocol"
)


set(include_dirs
"MOS_Protocol"
"SEN_Protocol"
)


set(requires
BSP
Common
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