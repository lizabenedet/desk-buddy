// Reactions to Claude Code hooks, relayed by the PC agent.

#include <stdio.h>

#include "face.h"
#include "module.h"
#include "pc_link.h"

#define WORKING_MS      (10 * 60 * 1000)   // long reactions: replaced by the next hook
#define DONE_MS         3000
#define DONE_SAY_MS     6000
#define HELLO_MS        1500

static bool busy;   // thinking or waiting: needs clearing if the PC goes away

static void react(mood_t mood, uint32_t ms)
{
    event_t ev = {.id = EV_FACE_REACT, .value = mood, .arg = (int32_t)ms};
    event_bus_publish(&ev);
}

static void say(const char *project, const char *what, uint32_t ms)
{
    char text[EVENT_TEXT_MAX];
    if (project[0]) {
        snprintf(text, sizeof(text), "%s: %s", project, what);
    } else {
        snprintf(text, sizeof(text), "%s", what);
    }
    event_bus_publish_text(EV_SAY, text, (int32_t)ms);
}

static void on_event(const event_t *ev)
{
    switch (ev->id) {
    case EV_CLAUDE:
        switch ((claude_event_t)ev->value) {
        case CLAUDE_SESSION_START:
            react(MOOD_HAPPY, HELLO_MS);
            say(ev->text, "oi!", 3000);
            break;
        case CLAUDE_PROMPT:
            busy = true;
            react(MOOD_THINKING, WORKING_MS);
            event_bus_publish_text(EV_SAY, "", 0);
            break;
        case CLAUDE_WAITING:
            busy = true;
            react(MOOD_ALERT, WORKING_MS);
            say(ev->text, "esperando você", 0);
            break;
        case CLAUDE_DONE:
            busy = false;
            react(MOOD_HAPPY, DONE_MS);
            say(ev->text, "pronto!", DONE_SAY_MS);
            break;
        case CLAUDE_SESSION_END:
            busy = false;
            react(MOOD_SLEEPY, 2000);
            event_bus_publish_text(EV_SAY, "", 0);
            break;
        }
        break;
    case EV_PC_CONNECTED:
    case EV_PC_SLEEP:
        // No more hooks are coming: don't stay stuck thinking/waiting.
        if (busy && (ev->id == EV_PC_SLEEP || ev->value == 0)) {
            busy = false;
            react(MOOD_NEUTRAL, 1);
            event_bus_publish_text(EV_SAY, "", 0);
        }
        break;
    default:
        break;
    }
}

const module_t claude_module = {
    .name = "claude",
    .on_event = on_event,
};
