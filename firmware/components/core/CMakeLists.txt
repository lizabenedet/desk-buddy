idf_component_register(
    SRCS "event_bus.c" "face.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES freertos
)
