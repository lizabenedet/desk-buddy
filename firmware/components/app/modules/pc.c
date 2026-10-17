// PC status screen (CPU/RAM/GPU and temperatures) and the overheating warning.

#include <math.h>
#include <time.h>

#include "module.h"
#include "pc_link.h"
#include "settings.h"
#include "ui.h"

LV_FONT_DECLARE(font_pt_20);

#define BURNING_WARN_EVERY_S (10 * 60)

typedef struct {
    lv_obj_t *bar;
    lv_obj_t *value;
} meter_t;

static meter_t cpu, ram, gpu;
static lv_obj_t *temps;
static lv_obj_t *status;
static bool connected;
static time_t last_burning_warn;

static void set_meter(meter_t *m, float pct)
{
    if (isnan(pct)) {
        lv_bar_set_value(m->bar, 0, LV_ANIM_OFF);
        lv_label_set_text(m->value, "--");
        return;
    }
    lv_bar_set_value(m->bar, (int32_t)pct, LV_ANIM_ON);
    lv_label_set_text_fmt(m->value, "%d%%", (int)(pct + 0.5f));
}

static void format_temp(char *out, size_t len, const char *name, float t)
{
    if (isnan(t)) {
        lv_snprintf(out, len, "%s --", name);
    } else {
        lv_snprintf(out, len, "%s %d°C", name, (int)(t + 0.5f));
    }
}

static void refresh(void)
{
    pc_metrics_t m;
    pc_metrics_get(&m);
    lv_label_set_text(status, connected ? "" : "PC desconectado");
    set_meter(&cpu, m.valid ? m.cpu : NAN);
    set_meter(&ram, m.valid ? m.ram : NAN);
    set_meter(&gpu, m.valid ? m.gpu : NAN);

    char c[24], g[24];
    format_temp(c, sizeof(c), "CPU", m.valid ? m.cpu_temp : NAN);
    format_temp(g, sizeof(g), "GPU", m.valid ? m.gpu_temp : NAN);
    lv_label_set_text_fmt(temps, "%s      %s", c, g);
}

static void check_burning(void)
{
    pc_metrics_t m;
    pc_metrics_get(&m);
    float t = pc_metrics_max_temp(&m);
    time_t now = time(NULL);
    float burning = pc_metrics_hot_threshold() + PC_BURNING_OVER;
    if (isnan(t) || t < burning || (last_burning_warn && now - last_burning_warn < BURNING_WARN_EVERY_S)) {
        return;
    }
    last_burning_warn = now;
    char text[EVENT_TEXT_MAX];
    lv_snprintf(text, sizeof(text), "Tô fervendo! %d°C aqui dentro.", (int)(t + 0.5f));
    event_bus_publish_text(EV_SAY, text, 8000);
}

static void apply_settings(void)
{
    pc_metrics_set_hot((float)settings_get_int("pc_hot_c"));
}

static void on_event(const event_t *ev)
{
    switch (ev->id) {
    case EV_SETTINGS_CHANGED:
        apply_settings();
        break;
    case EV_PC_METRICS:
        refresh();
        check_burning();
        break;
    case EV_PC_CONNECTED:
        connected = ev->value != 0;
        refresh();
        break;
    default:
        break;
    }
}

static meter_t make_meter(lv_obj_t *parent, const char *name, uint32_t color, int32_t y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, &font_pt_20, 0);
    lv_label_set_text(label, name);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 24, y);

    meter_t m;
    m.bar = lv_bar_create(parent);
    lv_obj_set_size(m.bar, 290, 20);
    lv_obj_set_style_bg_color(m.bar, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_align(m.bar, LV_ALIGN_TOP_LEFT, 92, y + 3);

    m.value = lv_label_create(parent);
    lv_obj_set_style_text_font(m.value, &font_pt_20, 0);
    lv_obj_set_style_text_color(m.value, lv_color_hex(UI_TEXT_DIM), 0);
    lv_obj_align(m.value, LV_ALIGN_TOP_RIGHT, -24, y);
    return m;
}

static void create_screen(lv_obj_t *parent)
{
    ui_title(parent, "PC");
    cpu = make_meter(parent, "CPU", 0x40E0FF, 64);
    ram = make_meter(parent, "RAM", 0xA080FF, 114);
    gpu = make_meter(parent, "GPU", 0x50FFB0, 164);

    temps = lv_label_create(parent);
    lv_obj_set_style_text_font(temps, &font_pt_20, 0);
    lv_obj_align(temps, LV_ALIGN_TOP_MID, 0, 220);

    status = lv_label_create(parent);
    lv_obj_set_style_text_font(status, &font_pt_20, 0);
    lv_obj_set_style_text_color(status, lv_color_hex(0xFFC000), 0);
    lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, -14);

    refresh();
}

static const setting_def_t settings[] = {
    {"pc_hot_c", "PC", "Temperatura de \"calor\" (°C)", SETTING_INT, 50, 100, (int32_t)PC_HOT_C, NULL},
};

const module_t pc_module = {
    .name = "pc",
    .init = apply_settings,
    .on_event = on_event,
    .create_screen = create_screen,
    .settings = settings,
    .settings_count = sizeof(settings) / sizeof(settings[0]),
};
