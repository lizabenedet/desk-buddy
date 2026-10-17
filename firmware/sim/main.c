// Desktop simulator: same app code as the firmware, rendered in an SDL
// window at the panel's resolution, mouse acting as the touch screen.
//
// Dev keys: 0-9 and '-' set the resting mood, 'r' a short happy reaction,
// 's' makes the buddy say something, 'w' cycles fake weather, 'n' toggles
// Wi-Fi, 'a' raises an alert.
//
// Agent connection: ws://127.0.0.1:8765/ws (token printed at startup).
//
// --shots <dir>             renders each mood and screen to BMPs and exits
// --snap-after <ms> <file>  runs normally, saves the screen after <ms> and exits
// --trace                   prints every bus event to stdout

#include "app.h"
#include "event_bus.h"
#include "face.h"
#include "lvgl.h"
#include "weather.h"
#include "ws_server.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

#define SIM_H_RES   480
#define SIM_V_RES   320
#define SIM_WS_PORT 8765

// Stand-ins for what the board's fetcher would report.
static void publish_fake_weather(void)
{
    static const int codes[] = {2, 63, 0, 95, 45};
    static int next;
    int code = codes[next++ % (int)(sizeof(codes) / sizeof(codes[0]))];

    weather_report_t r = {.valid = true, .updated = time(NULL), .code = code, .day_code = code};
    r.temp = 18.0f + (float)(next * 3 % 11);
    r.temp_min = r.temp - 4.0f;
    r.temp_max = r.temp + 6.0f;
    if (weather_kind(code) == WEATHER_RAIN || weather_kind(code) == WEATHER_THUNDER) {
        for (int h = 14; h < 19; h++) {
            r.hourly_rain[h] = 70;
        }
        r.day_rain_prob = 70;
    }
    weather_store_set(&r);
    event_bus_publish_simple(EV_WEATHER, 0);
}

static void on_key(lv_event_t *e)
{
    static bool wifi = true;
    uint32_t key = lv_event_get_key(e);
    if (key < 128) {
        key = (uint32_t)tolower((int)key); // Caps Lock shouldn't matter
    }
    if (key >= '0' && key <= '9') {
        event_bus_publish_simple(EV_FACE_MOOD, (int32_t)(key - '0'));
    } else if (key == '-') {
        event_bus_publish_simple(EV_FACE_MOOD, MOOD_TIRED);
    } else if (key == 'r') {
        event_t ev = {.id = EV_FACE_REACT, .value = MOOD_HAPPY, .arg = 2000};
        event_bus_publish(&ev);
    } else if (key == 's') {
        event_t ev = {.id = EV_SAY, .arg = 3000};
        strcpy(ev.text, "Olá! Ação não é fácil, viu?");
        event_bus_publish(&ev);
    } else if (key == 'w') {
        publish_fake_weather();
    } else if (key == 'n') {
        wifi = !wifi;
        event_bus_publish_simple(EV_WIFI, wifi);
    } else if (key == 'a') {
        event_bus_publish_text(EV_ALERT, "Hora da pausa! 5 minutos.", 0);
    }
}

static void setup_dev_keys(void)
{
    lv_group_t *group = lv_group_create();
    lv_indev_set_group(lv_sdl_keyboard_create(), group);

    // Invisible receiver for key events.
    lv_obj_t *sink = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(sink);
    lv_obj_set_size(sink, 0, 0);
    lv_group_add_obj(group, sink);
    lv_obj_add_event_cb(sink, on_key, LV_EVENT_KEY, NULL);
}

static void run_for(uint32_t ms)
{
    uint32_t start = lv_tick_get();
    while (lv_tick_elaps(start) < ms) {
        lv_timer_handler();
        sleep_ms(5);
    }
}

// Writes the active screen as a 24-bit BMP (LVGL's RGB888 is BGR in memory, like BMP).
static bool save_screen_bmp(const char *path)
{
    lv_draw_buf_t *buf = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888);
    if (!buf) {
        return false;
    }
    uint32_t w = buf->header.w, h = buf->header.h;
    uint32_t row = (w * 3 + 3) & ~3u;
    uint32_t data_size = row * h;
    uint8_t hdr[54] = {'B', 'M'};
    uint32_t file_size = 54 + data_size;
    memcpy(&hdr[2], &file_size, 4);
    hdr[10] = 54;
    hdr[14] = 40;
    memcpy(&hdr[18], &w, 4);
    memcpy(&hdr[22], &h, 4);
    hdr[26] = 1;
    hdr[28] = 24;
    memcpy(&hdr[34], &data_size, 4);

    FILE *f = fopen(path, "wb");
    if (!f) {
        lv_draw_buf_destroy(buf);
        return false;
    }
    fwrite(hdr, 1, sizeof(hdr), f);
    uint8_t pad[3] = {0};
    for (uint32_t y = h; y-- > 0;) {
        fwrite(buf->data + y * buf->header.stride, 1, w * 3, f);
        fwrite(pad, 1, row - w * 3, f);
    }
    fclose(f);
    lv_draw_buf_destroy(buf);
    return true;
}

static int take_shots(const char *dir)
{
    char path[512];
    for (int m = 0; m < MOOD_COUNT; m++) {
        event_bus_publish_simple(EV_FACE_MOOD, m);
        run_for(600);
        snprintf(path, sizeof(path), "%s/%02d.bmp", dir, m);
        if (!save_screen_bmp(path)) {
            fprintf(stderr, "failed to write %s\n", path);
            return 1;
        }
    }

    // Then every module screen (tiles to the right of home).
    lv_obj_t *tiles = lv_obj_get_child(lv_screen_active(), 0);
    for (uint32_t i = 1; i < lv_obj_get_child_count(tiles); i++) {
        lv_tileview_set_tile_by_index(tiles, i, 0, LV_ANIM_OFF);
        run_for(400);
        snprintf(path, sizeof(path), "%s/screen_%u.bmp", dir, (unsigned)i);
        if (!save_screen_bmp(path)) {
            fprintf(stderr, "failed to write %s\n", path);
            return 1;
        }
    }

    // Rainy day on the weather screen, then an alert on home.
    publish_fake_weather();
    lv_tileview_set_tile_by_index(tiles, 2, 0, LV_ANIM_OFF);
    run_for(400);
    snprintf(path, sizeof(path), "%s/rain.bmp", dir);
    save_screen_bmp(path);

    lv_tileview_set_tile_by_index(tiles, 0, 0, LV_ANIM_OFF);
    event_bus_publish_text(EV_ALERT, "Hora da pausa! 5 minutos.", 0);
    run_for(700);
    // The alert border lives on the top layer, which snapshots of the screen skip.
    snprintf(path, sizeof(path), "%s/alert.bmp", dir);
    save_screen_bmp(path);
    return 0;
}

static void trace_event(const event_t *ev, void *ctx)
{
    (void)ctx;
    printf("event %d value=%d arg=%d text=\"%s\"\n", ev->id, (int)ev->value, (int)ev->arg, ev->text);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    const char *shots_dir = NULL;
    const char *snap_path = NULL;
    uint32_t snap_after_ms = 0;
    bool trace = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--shots") == 0 && i + 1 < argc) {
            shots_dir = argv[++i];
        } else if (strcmp(argv[i], "--snap-after") == 0 && i + 2 < argc) {
            snap_after_ms = (uint32_t)atoi(argv[++i]);
            snap_path = argv[++i];
        } else if (strcmp(argv[i], "--trace") == 0) {
            trace = true;
        }
    }
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    lv_init();
    lv_display_t *disp = lv_sdl_window_create(SIM_H_RES, SIM_V_RES);
    lv_sdl_window_set_title(disp, "desk-buddy");
    lv_sdl_mouse_create();

    if (trace) {
        event_bus_subscribe(EV_ANY, trace_event, NULL);
    }
    app_start();
    setup_dev_keys();

    // The PC already has network and a set clock.
    event_t wifi = {.id = EV_WIFI, .value = 1};
    strcpy(wifi.text, "127.0.0.1");
    event_bus_publish(&wifi);
    event_bus_publish_simple(EV_TIME_SYNCED, 0);
    publish_fake_weather();

    if (shots_dir) {
        return take_shots(shots_dir);
    }

    ws_server_start(SIM_WS_PORT);
    uint32_t start = lv_tick_get();
    for (;;) {
        ws_server_poll();
        uint32_t idle = lv_timer_handler();
        if (snap_path && lv_tick_elaps(start) >= snap_after_ms) {
            return save_screen_bmp(snap_path) ? 0 : 1;
        }
        sleep_ms(idle < 5 ? idle : 5);
    }
}
