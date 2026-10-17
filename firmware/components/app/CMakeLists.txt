idf_component_register(
    SRCS "app.c" "home.c" "face_view.c" "modules.c" "modules/clock.c" "fonts/font_pt_20.c"
    INCLUDE_DIRS "include"
    REQUIRES core lvgl__lvgl
)
