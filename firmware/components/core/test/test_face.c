#include "check.h"
#include "face.h"

static void run(face_t *f, uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 10) {
        face_update(f, 10);
    }
}

static void test_lerp_endpoints_and_color(void)
{
    const face_params_t *a = face_preset(MOOD_NEUTRAL);
    const face_params_t *b = face_preset(MOOD_ANGRY);
    face_params_t at0 = face_lerp(a, b, 0.0f);
    face_params_t at1 = face_lerp(a, b, 1.0f);
    CHECK(NEAR(at0.lid_angle, a->lid_angle) && at0.color == a->color);
    CHECK(NEAR(at1.lid_angle, b->lid_angle) && at1.color == b->color);

    face_params_t black = {0}, white = {0};
    white.color = 0xFFFFFF;
    CHECK(face_lerp(&black, &white, 0.5f).color == 0x808080);
}

static void test_mood_transition_completes(void)
{
    face_t f;
    face_init(&f, 1);
    face_set_mood(&f, MOOD_SAD);
    run(&f, 50);
    CHECK(!NEAR(f.current.mouth_curve, face_preset(MOOD_SAD)->mouth_curve)); // still easing
    run(&f, 500);
    CHECK(NEAR(f.current.mouth_curve, face_preset(MOOD_SAD)->mouth_curve));
    CHECK(f.current.color == face_preset(MOOD_SAD)->color);
}

static void test_reaction_returns_to_base(void)
{
    face_t f;
    face_init(&f, 1);
    face_set_mood(&f, MOOD_SLEEPY);
    face_react(&f, MOOD_HAPPY, 1000);
    CHECK(f.shown_mood == MOOD_HAPPY);

    // Changing the base mid-reaction doesn't cut the reaction short.
    face_set_mood(&f, MOOD_TIRED);
    run(&f, 900);
    CHECK(f.shown_mood == MOOD_HAPPY);

    run(&f, 200);
    CHECK(f.shown_mood == MOOD_TIRED);
}

static void test_blinks_and_reopens(void)
{
    face_t f;
    face_init(&f, 42);
    float min_open = 1.0f;
    for (int i = 0; i < 700; i++) { // 7 s: at least one blink (max gap 6 s)
        face_update(&f, 10);
        float open = face_output(&f).eye_open;
        if (open < min_open) {
            min_open = open;
        }
    }
    CHECK(min_open < 0.2f);
    run(&f, 200);
    CHECK(f.blinking || face_output(&f).eye_open > 0.99f);
}

static void test_asleep_never_blinks_or_glances(void)
{
    face_t f;
    face_init(&f, 7);
    face_set_mood(&f, MOOD_ASLEEP);
    run(&f, 500); // let any in-flight blink finish
    for (int i = 0; i < 2000; i++) {
        face_update(&f, 10);
        CHECK(!f.blinking);
        CHECK(NEAR(face_output(&f).look_x, face_preset(MOOD_ASLEEP)->look_x));
    }
}

static void test_invalid_mood_ignored(void)
{
    face_t f;
    face_init(&f, 1);
    face_set_mood(&f, MOOD_COUNT);
    face_react(&f, MOOD_COUNT, 1000);
    CHECK(f.base_mood == MOOD_NEUTRAL && f.shown_mood == MOOD_NEUTRAL);
}

int main(void)
{
    test_lerp_endpoints_and_color();
    test_mood_transition_completes();
    test_reaction_returns_to_base();
    test_blinks_and_reopens();
    test_asleep_never_blinks_or_glances();
    test_invalid_mood_ignored();
    puts("face: all tests passed");
    return 0;
}
