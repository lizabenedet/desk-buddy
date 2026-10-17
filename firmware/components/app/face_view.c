#include "face_view.h"

#include <math.h>
#include <string.h>

#define FRAME_MS        33

// Geometry for a 480x320 screen, relative to the object's top-left corner.
#define FACE_MID_X      240
#define EYE_W           96
#define EYE_H           120
#define EYE_DX          100     // eye center distance from the middle
#define EYE_CY          135
#define EYE_RADIUS      28
#define EYE_MIN_H       6
#define LOOK_X_PX       30
#define LOOK_Y_PX       18
#define MOUTH_CY        250
#define MOUTH_MAX_W     140
#define MOUTH_MIN_W     16
#define MOUTH_STROKE    10
#define MOUTH_MAX_OPEN  44
#define DIRTY_PAD       4

#define PI_F            3.14159265f

typedef struct {
    face_t *face;
    face_params_t drawn;
    lv_area_t eye_area[2];
    lv_area_t mouth_area;
    uint32_t last_tick;
    lv_timer_t *timer;
} face_view_t;

typedef struct {
    int32_t x1, y1, x2, y2;     // eye rectangle
    int32_t radius;
} eye_geom_t;

typedef struct {
    int32_t cx, cy, w;
    bool open;
    int32_t open_h;
    bool flat;
    bool smile;
    int32_t radius;             // arc
    int32_t center_y;           // arc
    float half_deg;             // arc
    int32_t sag;
} mouth_geom_t;

static int32_t imin(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t imax(int32_t a, int32_t b) { return a > b ? a : b; }

// side: 0 = left eye, 1 = right eye.
static eye_geom_t eye_geom(const face_params_t *p, int side)
{
    float w = EYE_W * p->eye_scale;
    float h = EYE_H * p->eye_scale * p->eye_open;
    if (h < EYE_MIN_H) {
        h = EYE_MIN_H;
    }
    float cx = FACE_MID_X + (side ? EYE_DX : -EYE_DX) + p->look_x * LOOK_X_PX;
    float cy = EYE_CY + p->look_y * LOOK_Y_PX;
    eye_geom_t g = {
        .x1 = (int32_t)(cx - w / 2),
        .x2 = (int32_t)(cx + w / 2),
        .y1 = (int32_t)(cy - h / 2),
        .y2 = (int32_t)(cy + h / 2),
    };
    g.radius = imin((int32_t)(EYE_RADIUS * p->eye_scale), imin(g.x2 - g.x1, g.y2 - g.y1) / 2);
    return g;
}

static mouth_geom_t mouth_geom(const face_params_t *p)
{
    mouth_geom_t g = {
        .cx = (int32_t)(FACE_MID_X + p->look_x * LOOK_X_PX * 0.5f),
        .cy = (int32_t)(MOUTH_CY + p->look_y * LOOK_Y_PX * 0.5f),
        .w = imax(MOUTH_MIN_W, (int32_t)(MOUTH_MAX_W * p->mouth_width)),
    };
    float curve = fabsf(p->mouth_curve);
    if (p->mouth_open > 0.05f) {
        g.open = true;
        g.open_h = imax(MOUTH_STROKE, (int32_t)(MOUTH_MAX_OPEN * p->mouth_open));
    } else if (curve < 0.08f) {
        g.flat = true;
    } else {
        // Arc whose chord is the mouth width; more curve = wider angle.
        g.smile = p->mouth_curve > 0;
        g.half_deg = (30.0f + 110.0f * curve) / 2.0f;
        float half = g.half_deg * PI_F / 180.0f;
        float r = (g.w / 2.0f) / sinf(half);
        g.radius = (int32_t)r;
        g.sag = (int32_t)(r * (1.0f - cosf(half)));
        float chord_y = g.smile ? g.cy - g.sag / 2.0f : g.cy + g.sag / 2.0f;
        g.center_y = (int32_t)(g.smile ? chord_y - r * cosf(half) : chord_y + r * cosf(half));
    }
    return g;
}

static lv_area_t eye_bounds(const face_params_t *p, int side)
{
    eye_geom_t g = eye_geom(p, side);
    return (lv_area_t){g.x1 - DIRTY_PAD, g.y1 - DIRTY_PAD, g.x2 + DIRTY_PAD, g.y2 + DIRTY_PAD};
}

static lv_area_t mouth_bounds(const face_params_t *p)
{
    mouth_geom_t g = mouth_geom(p);
    int32_t half_h = g.open ? g.open_h / 2 : (g.sag + MOUTH_STROKE) / 2 + MOUTH_STROKE;
    int32_t half_w = g.w / 2 + MOUTH_STROKE;
    int32_t pad = DIRTY_PAD;
    return (lv_area_t){g.cx - half_w - pad, g.cy - half_h - pad, g.cx + half_w + pad, g.cy + half_h + pad};
}

static void draw_rect(lv_layer_t *layer, const lv_area_t *a, lv_color_t color, int32_t radius)
{
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = color;
    d.bg_opa = LV_OPA_COVER;
    d.radius = radius;
    lv_draw_rect(layer, &d, a);
}

static void draw_eye(lv_layer_t *layer, const lv_area_t *origin, const face_params_t *p, int side,
                     lv_color_t color, lv_color_t bg)
{
    eye_geom_t g = eye_geom(p, side);
    int32_t ox = origin->x1, oy = origin->y1;
    int32_t x1 = ox + g.x1, x2 = ox + g.x2, y1 = oy + g.y1, y2 = oy + g.y2;
    int32_t h = y2 - y1, w = x2 - x1;

    lv_area_t eye = {x1, y1, x2, y2};
    draw_rect(layer, &eye, color, g.radius);

    // Lids are background-colored shapes drawn over the eye.
    if (p->lid_top > 0.01f) {
        lv_area_t lid = {x1 - 2, y1 - 2, x2 + 2, y1 + (int32_t)(p->lid_top * h)};
        draw_rect(layer, &lid, bg, 0);
    }
    if (fabsf(p->lid_angle) > 0.01f) {
        bool inner_down = p->lid_angle > 0;           // angry
        bool lower_right = (side == 0) == inner_down; // inner side of the left eye is its right
        lv_draw_triangle_dsc_t t;
        lv_draw_triangle_dsc_init(&t);
        t.color = bg;
        t.opa = LV_OPA_COVER;
        t.p[0] = (lv_point_precise_t){x1 - 2, y1 - 2};
        t.p[1] = (lv_point_precise_t){x2 + 2, y1 - 2};
        t.p[2] = (lv_point_precise_t){lower_right ? x2 + 2 : x1 - 2,
                                      (lv_value_precise_t)(y1 + fabsf(p->lid_angle) * h * 0.55f)};
        lv_draw_triangle(layer, &t);
    }
    if (p->eye_smile > 0.01f) {
        int32_t over = w * 15 / 100;
        lv_area_t cheek = {x1 - over, y2 - (int32_t)(p->eye_smile * h), x2 + over, y2 + h};
        draw_rect(layer, &cheek, bg, LV_RADIUS_CIRCLE);
    }
}

static void draw_mouth(lv_layer_t *layer, const lv_area_t *origin, const face_params_t *p, lv_color_t color)
{
    mouth_geom_t g = mouth_geom(p);
    int32_t cx = origin->x1 + g.cx, cy = origin->y1 + g.cy;

    if (g.open || g.flat) {
        int32_t half_w = g.open ? g.w * 4 / 10 : g.w / 2;
        int32_t half_h = (g.open ? g.open_h : MOUTH_STROKE) / 2;
        lv_area_t a = {cx - half_w, cy - half_h, cx + half_w, cy + half_h};
        draw_rect(layer, &a, color, LV_RADIUS_CIRCLE);
        return;
    }

    lv_draw_arc_dsc_t d;
    lv_draw_arc_dsc_init(&d);
    d.color = color;
    d.width = MOUTH_STROKE;
    d.rounded = 1;
    d.center = (lv_point_t){cx, origin->y1 + g.center_y};
    d.radius = (uint16_t)(g.radius + MOUTH_STROKE / 2);
    float mid = g.smile ? 90.0f : 270.0f; // LVGL angles: 0 = right, clockwise
    d.start_angle = (lv_value_precise_t)(mid - g.half_deg);
    d.end_angle = (lv_value_precise_t)(mid + g.half_deg);
    lv_draw_arc(layer, &d);
}

static void draw_cb(lv_event_t *e)
{
    face_view_t *v = lv_event_get_user_data(e);
    lv_obj_t *obj = lv_event_get_current_target_obj(e);
    lv_layer_t *layer = lv_event_get_layer(e);

    lv_area_t origin;
    lv_obj_get_coords(obj, &origin);
    lv_color_t color = lv_color_hex(v->drawn.color);
    lv_color_t bg = lv_obj_get_style_bg_color(lv_screen_active(), 0);

    draw_eye(layer, &origin, &v->drawn, 0, color, bg);
    draw_eye(layer, &origin, &v->drawn, 1, color, bg);
    draw_mouth(layer, &origin, &v->drawn, color);
}

static bool eyes_differ(const face_params_t *a, const face_params_t *b)
{
    return a->eye_open != b->eye_open || a->eye_scale != b->eye_scale || a->lid_top != b->lid_top ||
           a->lid_angle != b->lid_angle || a->eye_smile != b->eye_smile || a->look_x != b->look_x ||
           a->look_y != b->look_y || a->color != b->color;
}

static bool mouth_differs(const face_params_t *a, const face_params_t *b)
{
    return a->mouth_curve != b->mouth_curve || a->mouth_open != b->mouth_open ||
           a->mouth_width != b->mouth_width || a->look_x != b->look_x || a->look_y != b->look_y ||
           a->color != b->color;
}

static void invalidate_rel(lv_obj_t *obj, const lv_area_t *rel)
{
    lv_area_t origin;
    lv_obj_get_coords(obj, &origin);
    lv_area_t abs = *rel;
    lv_area_move(&abs, origin.x1, origin.y1);
    lv_obj_invalidate_area(obj, &abs);
}

static void invalidate_part(lv_obj_t *obj, lv_area_t *prev, lv_area_t next)
{
    lv_area_t both = {
        imin(prev->x1, next.x1), imin(prev->y1, next.y1),
        imax(prev->x2, next.x2), imax(prev->y2, next.y2),
    };
    invalidate_rel(obj, &both);
    *prev = next;
}

static void frame_cb(lv_timer_t *t)
{
    lv_obj_t *obj = lv_timer_get_user_data(t);
    face_view_t *v = lv_obj_get_user_data(obj);

    uint32_t dt = lv_tick_elaps(v->last_tick);
    v->last_tick = lv_tick_get();
    face_update(v->face, dt);

    face_params_t next = face_output(v->face);
    bool eyes = eyes_differ(&next, &v->drawn);
    bool mouth = mouth_differs(&next, &v->drawn);
    v->drawn = next;

    if (eyes) {
        invalidate_part(obj, &v->eye_area[0], eye_bounds(&next, 0));
        invalidate_part(obj, &v->eye_area[1], eye_bounds(&next, 1));
    }
    if (mouth) {
        invalidate_part(obj, &v->mouth_area, mouth_bounds(&next));
    }
}

static void delete_cb(lv_event_t *e)
{
    face_view_t *v = lv_event_get_user_data(e);
    lv_timer_delete(v->timer);
    lv_free(v);
}

lv_obj_t *face_view_create(lv_obj_t *parent, face_t *face)
{
    face_view_t *v = lv_malloc_zeroed(sizeof(*v));
    LV_ASSERT_MALLOC(v);
    v->face = face;
    v->drawn = face_output(face);
    v->eye_area[0] = eye_bounds(&v->drawn, 0);
    v->eye_area[1] = eye_bounds(&v->drawn, 1);
    v->mouth_area = mouth_bounds(&v->drawn);
    v->last_tick = lv_tick_get();

    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
    lv_obj_set_user_data(obj, v);
    lv_obj_add_event_cb(obj, draw_cb, LV_EVENT_DRAW_MAIN, v);
    lv_obj_add_event_cb(obj, delete_cb, LV_EVENT_DELETE, v);

    v->timer = lv_timer_create(frame_cb, FRAME_MS, obj);
    return obj;
}
