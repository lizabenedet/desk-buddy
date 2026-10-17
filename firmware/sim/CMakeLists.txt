cmake_minimum_required(VERSION 3.22)
project(desk_buddy_sim C)

set(CMAKE_C_STANDARD 11)
if(MSVC)
    add_compile_options(/utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)  # plain C string functions are fine here
endif()
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
# Download only (no CMakeLists in SOURCE_SUBDIR): cJSON.c is compiled into core.
FetchContent_Declare(cjson
    URL https://github.com/DaveGamble/cJSON/archive/refs/tags/v1.7.19.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR download-only
)
FetchContent_MakeAvailable(sdl2 cjson)
find_package(SDL2 REQUIRED CONFIG PATHS ${sdl2_SOURCE_DIR}/cmake NO_DEFAULT_PATH)

set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE STRING "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(lvgl)
target_link_libraries(lvgl PUBLIC SDL2::SDL2)

# Same source dirs as the ESP-IDF components (SRC_DIRS there).
file(GLOB CORE_SRCS CONFIGURE_DEPENDS ${COMPONENTS_DIR}/core/*.c)
add_library(core STATIC ${CORE_SRCS} ${cjson_SOURCE_DIR}/cJSON.c)
target_include_directories(core PUBLIC ${COMPONENTS_DIR}/core/include PRIVATE ${cjson_SOURCE_DIR})

file(GLOB APP_SRCS CONFIGURE_DEPENDS
    ${COMPONENTS_DIR}/app/*.c
    ${COMPONENTS_DIR}/app/modules/*.c
    ${COMPONENTS_DIR}/app/fonts/*.c
)
add_library(app STATIC ${APP_SRCS} storage_file.c)
target_include_directories(app PUBLIC ${COMPONENTS_DIR}/app/include)
target_link_libraries(app PUBLIC core lvgl)

# GCC on the ESP rejects implicit declarations; make MSVC do the same.
if(MSVC)
    target_compile_options(core PRIVATE /W3 /we4013)
    target_compile_options(app PRIVATE /W3 /we4013)
endif()

add_executable(desk_buddy_sim main.c ws_server.c)
target_link_libraries(desk_buddy_sim PRIVATE app ws2_32)
add_custom_command(TARGET desk_buddy_sim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:desk_buddy_sim>
)

enable_testing()
file(GLOB TEST_SRCS CONFIGURE_DEPENDS ${COMPONENTS_DIR}/core/test/test_*.c)
foreach(src ${TEST_SRCS})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE core)
    target_compile_definitions(${name} PRIVATE FIXTURE_DIR="${COMPONENTS_DIR}/core/test")
    add_test(NAME ${name} COMMAND ${name})
endforeach()
