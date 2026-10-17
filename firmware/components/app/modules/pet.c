// Pet meters: drive the resting mood, react to petting, persist across reboots.

#include "icons.h"
#include "module.h"
#include "pc_link.h"
#include "pet.h"
#include "storage.h"
#include "ui.h"

#define STORAGE_KEY     "pet"
#define UPDATE_MS       60000
#define SAVE_EVERY      10      // updates

static pet_t pet;
static uint32_t last_tick;
static int updates;
static mood_t published_mood = MOOD_COUNT;

static lv_obj_t *energy_bar;
static lv_obj_t *happy_bar;

static int current_hour(void)
{
    struct tm tm;
    return ui_local_time(&tm) ? tm.tm_hour : -1;
}

static void publish_mood(void)
{
    pc_metrics_t pc;
    pc_metrics_get(&pc);
    mood_t m = pet_resting_mood(&pet, current_hour(), pc_metrics_hot(&pc));
    if (m != published_mood) {
        published_mood = m;
        event_bus_publish_simple(EV_FACE_MOOD, m);
    }
}

static void refresh_bars(void)
{
    if (energy_bar) {
        lv_bar_set_value(energy_bar, (int32_t)pet.energy, LV_ANIM_ON);
        lv_bar_set_value(happy_bar, (int32_t)pet.happiness, LV_ANIM_ON);
    }
}

static void tick(lv_timer_t *t)
{
    (void)t;
    uint32_t dt = lv_tick_elaps(last_tick);
    last_tick = lv_tick_get();
    pc_metrics_t pc;
    pc_metrics_get(&pc);
    pet_update(&pet, dt, current_hour(), pc_metrics_busy(&pc));
    publish_mood();
    refresh_bars();
    if (++updates % SAVE_EVERY == 0) {
        storage_save(STORAGE_KEY, &pet, sizeof(pet));
    }
}

static void on_event(const event_t *ev)
{
    switch (ev->id) {
    case EV_FACE_TOUCHED:
        if (ev->value == TOUCH_HOLD) {
            pet_on_petted(&pet);
        } else {
            pet_on_poked(&pet);
        }
        publish_mood();
        refresh_bars();
        break;
    case EV_CLAUDE:
        if (ev->value == CLAUDE_DONE) {
            pet_on_good_news(&pet);
            refresh_bars();
        }
        break;
    case EV_TIME_SYNCED:     // the hour decides whether it's bedtime
    case EV_PC_METRICS:      // ...and PC heat whether it's hot
    case EV_PC_CONNECTED:
        publish_mood();
        break;
    default:
        break;
    }
}

static void init(void)
{
    pet_t stored;
    if (storage_load(STORAGE_KEY, &stored, sizeof(stored)) && stored.energy >= 0 && stored.energy <= 100 &&
        stored.happiness >= 0 && stored.happiness <= 100) {
        pet = stored;
    } else {
        pet_init(&pet);
    }
    last_tick = lv_tick_get();
    publish_mood();
    lv_timer_create(tick, UPDATE_MS, NULL);
}

static lv_obj_t *meter(lv_obj_t *parent, const char *icon, const char *name, uint32_t color, int32_t y)
{
    lv_obj_t *ic = lv_label_create(parent);
    lv_obj_set_style_text_font(ic, &icons_20, 0);
    lv_obj_set_style_text_color(ic, lv_color_hex(color), 0);
    lv_label_set_text(ic, icon);
    lv_obj_align(ic, LV_ALIGN_TOP_LEFT, 40, y);

    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, &font_pt_20, 0);
    lv_label_set_text(label, name);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 70, y - 2);

    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_size(bar, 400, 22);
    lv_obj_set_style_bg_color(bar, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, y + 32);
    return bar;
}

static void create_screen(lv_obj_t *parent)
{
    ui_title(parent, "Como estou");
    energy_bar = meter(parent, ICON_PET, "Energia", 0x40E0FF, 70);
    happy_bar = meter(parent, ICON_HEART, "Felicidade", 0xFF6FA0, 170);
    refresh_bars();
}

const module_t pet_module = {
    .name = "pet",
    .init = init,
    .on_event = on_event,
    .create_screen = create_screen,
};
