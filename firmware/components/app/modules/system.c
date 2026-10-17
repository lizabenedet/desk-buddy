// "Sistema" screen: how to reach the buddy and the pairing token for the agent.

#include <string.h>

#include "module.h"
#include "pc_link.h"
#include "ui.h"

LV_FONT_DECLARE(font_pt_20);

static char token[PC_LINK_TOKEN_LEN];
static char ip[EVENT_TEXT_MAX] = "";
static bool wifi, pc;

static lv_obj_t *wifi_label;
static lv_obj_t *pc_label;

static void refresh(void)
{
    if (!wifi_label) {
        return;
    }
    if (wifi) {
        lv_label_set_text_fmt(wifi_label, "Wi-Fi: %s  (buddy.local)", ip[0] ? ip : "conectado");
    } else {
        lv_label_set_text(wifi_label, "Wi-Fi: desconectado");
    }
    lv_label_set_text(pc_label, pc ? "PC: conectado" : "PC: desconectado");
}

static void on_event(const event_t *ev)
{
    switch (ev->id) {
    case EV_WIFI:
        wifi = ev->value != 0;
        strncpy(ip, ev->text, sizeof(ip) - 1);
        refresh();
        break;
    case EV_PC_CONNECTED:
        pc = ev->value != 0;
        refresh();
        break;
    default:
        break;
    }
}

static void init(void)
{
    pc_link_token(token);
}

static lv_obj_t *line(lv_obj_t *parent, int32_t y)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, &font_pt_20, 0);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y);
    return l;
}

static void create_screen(lv_obj_t *parent)
{
    ui_title(parent, "Sistema");
    wifi_label = line(parent, 56);
    pc_label = line(parent, 92);

    lv_obj_t *caption = line(parent, 160);
    lv_obj_set_style_text_color(caption, lv_color_hex(UI_TEXT_DIM), 0);
    lv_label_set_text(caption, "Token para o agente do PC:");

    lv_obj_t *t = lv_label_create(parent);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_letter_space(t, 4, 0);
    lv_label_set_text(t, token);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 196);

    refresh();
}

const module_t system_module = {
    .name = "system",
    .init = init,
    .on_event = on_event,
    .create_screen = create_screen,
};
