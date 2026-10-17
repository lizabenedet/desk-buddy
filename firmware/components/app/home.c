#include "home.h"

#include "event_bus.h"
#include "face.h"
#include "face_view.h"

LV_FONT_DECLARE(font_pt_20);

#define TAP_REACT_MS    1200
#define HOLD_REACT_MS   2500
#define BUBBLE_MAX_W    400

static face_t face;
static lv_obj_t *bubble;
static lv_timer_t *bubble_timer;

static void hide_bubble(lv_timer_t *t)
{
    lv_timer_pause(t);
    lv_obj_add_flag(bubble, LV_OBJ_FLAG_HIDDEN);
}

static void say(const char *text, uint32_t duration_ms)
{
    lv_label_set_text(bubble, text);
    lv_obj_remove_flag(bubble, LV_OBJ_FLAG_HIDDEN);
    lv_timer_set_period(bubble_timer, duration_ms);
    lv_timer_reset(bubble_timer);
    lv_timer_resume(bubble_timer);
}

static void on_event(const event_t *ev, void *ctx)
{
    (void)ctx;
    switch (ev->id) {
    case EV_FACE_MOOD:
        face_set_mood(&face, (mood_t)ev->value);
        break;
    case EV_FACE_REACT:
        face_react(&face, (mood_t)ev->value, (uint32_t)ev->arg);
        break;
    case EV_SAY:
        say(ev->text, (uint32_t)ev->arg);
        break;
    case EV_FACE_TOUCHED:
        // Poke = startled, petting = happy. The pet module (phase 5) also
        // listens to this to update its meters.
        if (ev->value == TOUCH_HOLD) {
            face_react(&face, MOOD_HAPPY, HOLD_REACT_MS);
        } else {
            face_react(&face, MOOD_SURPRISED, TAP_REACT_MS);
        }
        break;
    default:
        break;
    }
}

static void on_touch(lv_event_t *e)
{
    touch_kind_t kind = lv_event_get_code(e) == LV_EVENT_LONG_PRESSED ? TOUCH_HOLD : TOUCH_TAP;
    event_bus_publish_simple(EV_FACE_TOUCHED, kind);
}

static lv_obj_t *create_bubble(lv_obj_t *parent)
{
    lv_obj_t *b = lv_label_create(parent);
    lv_obj_set_style_text_font(b, &font_pt_20, 0);
    lv_obj_set_style_text_color(b, lv_color_black(), 0);
    lv_obj_set_style_text_align(b, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(b, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 14, 0);
    lv_obj_set_style_pad_hor(b, 14, 0);
    lv_obj_set_style_pad_ver(b, 8, 0);
    lv_obj_set_style_max_width(b, BUBBLE_MAX_W, 0);
    lv_obj_set_width(b, LV_SIZE_CONTENT);
    lv_label_set_long_mode(b, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
    return b;
}

void home_create(lv_obj_t *parent)
{
    face_init(&face, lv_tick_get() | 1);

    lv_obj_t *view = face_view_create(parent, &face);
    lv_obj_add_flag(view, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(view, on_touch, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(view, on_touch, LV_EVENT_LONG_PRESSED, NULL);

    bubble = create_bubble(parent);
    bubble_timer = lv_timer_create(hide_bubble, 1000, NULL);
    lv_timer_pause(bubble_timer);

    event_bus_subscribe(EV_ANY, on_event, NULL);
}
