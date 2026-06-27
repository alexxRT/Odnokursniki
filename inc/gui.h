#ifndef OK_GUI_H
#define OK_GUI_H

const int MAX_WINDOWS = 10;
const int MAX_WIN_HANDLERS = 10;

enum GUI_EVENT {
    FINALIZE = 0,
    RESIZE = 1,

};

typedef struct gui_event {
    // something add later
    uint64_t event_id;
    size_t scene_width;
    size_t scene_hight;
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

    ThreadSafeList<gui_event_t>* pending_events;

    uint64_t scene_id;
    bool active = false;
    uv_mutex_t scene_mutex;

    size_t hight;
    size_t width;
} gui_scene_t;

typedef struct event_thread_args {
    ThreadSafeList<gui_event_t>* queue;
    size_t hight;
    size_t width;
} event_args_t;

#endif // OK_GUI_H