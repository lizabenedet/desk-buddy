idf_component_register(
    SRCS "app.c" "modules.c" "modules/clock.c"
    INCLUDE_DIRS "include"
    REQUIRES core lvgl__lvgl
)
