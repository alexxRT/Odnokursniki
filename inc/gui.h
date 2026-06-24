#ifndef OK_GUI_H
#define OK_GUI_H

const int MAX_WINDOWS = 10;
const int MAX_WIN_HANDLERS = 10;

typedef struct gui_event {
    // something add later
    uint64_t event_id;
} gui_event_t;

typedef bool (event_handler_t*)(gui_event_t* event);
typedef int (event_scheduler_t*)(gui_event* event); // returns handler index, -1 on fail

typedef struct window {
    WINDOW* win = NULL;
    bool active = false; // flag indicates window is active or not
    event_scheduler_t event_scheduler;
    event_handler_t event_handlers[MAX_WIN_HANDLERS];
} window_t;

typedef struct gui_scene {
    event_scheduler_t event_scheduler;
    event_handler_t event_handlers[MAX_WIN_HANDLERS];
    window_t windows[MAX_WINDOWS];

    uint64_t scene_id;
    bool active = false;
    uv_mutex_t scene_mutex;


} gui_scene_t;

#endif // OK_GUI_H