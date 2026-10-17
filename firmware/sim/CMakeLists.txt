cmake_minimum_required(VERSION 3.22)
project(desk_buddy_sim C)

set(CMAKE_C_STANDARD 11)
set(COMPONENTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../components)

include(FetchContent)

# Same LVGL version as firmware/dependencies.lock.
FetchContent_Declare(lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG v9.5.0
    GIT_SHALLOW TRUE
)
FetchContent_Declare(sdl2
    URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.10/SDL2-devel-2.30.10-VC.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(sdl2)
find_package(SDL2 REQUIRED CONFIG PATHS ${sdl2_SOURCE_DIR}/cmake NO_DEFAULT_PATH)

set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE STRING "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(lvgl)
target_link_libraries(lvgl PUBLIC SDL2::SDL2)

add_library(core STATIC ${COMPONENTS_DIR}/core/event_bus.c)
target_include_directories(core PUBLIC ${COMPONENTS_DIR}/core/include)

add_library(app STATIC
    ${COMPONENTS_DIR}/app/app.c
    ${COMPONENTS_DIR}/app/modules.c
    ${COMPONENTS_DIR}/app/modules/clock.c
)
target_include_directories(app PUBLIC ${COMPONENTS_DIR}/app/include)
target_link_libraries(app PUBLIC core lvgl)

add_executable(desk_buddy_sim main.c)
target_link_libraries(desk_buddy_sim PRIVATE app)
add_custom_command(TARGET desk_buddy_sim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:desk_buddy_sim>
)

enable_testing()
add_executable(test_event_bus ${COMPONENTS_DIR}/core/test/test_event_bus.c)
target_link_libraries(test_event_bus PRIVATE core)
add_test(NAME event_bus COMMAND test_event_bus)
