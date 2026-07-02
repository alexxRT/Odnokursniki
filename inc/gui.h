#ifndef OK_GUI_H
#define OK_GUI_H

#include <ncurses.h>
#include <cstdint>
#include <uv.h>
#include "client.h"

template <typename ListDataT>
class ThreadSafeList;


const int MAX_WINDOWS = 10;
const int MAX_WIN_HANDLERS = 10;
const int MAX_INPUT_LEN = 256;

enum SceneKind {
    SCENE_KIND_ENTER = 0,
    SCENE_KIND_CHAT  = 1
};

enum SceneResult {
    SCENE_RESULT_CONTINUE      = 0,
    SCENE_RESULT_EXIT          = 1,
    SCENE_RESULT_ENTER_CHAT    = 2,
    SCENE_RESULT_BACK_TO_ENTER = 3
};

enum GUI_EVENT {
    FINALIZE   = 0,
    RESIZE     = 1,
    ARROW_UP   = 2,
    ARROW_DOWN = 3,
    ARROW_LEFT = 4,
    ENTER      = 5
};

typedef struct gui_event {
    uint64_t event_id;
    int scene_width;
    int scene_hight;
    int ch;
} gui_event_t;

typedef struct posistion {
    int pos_x;
    int pos_y;
} pos_t;

typedef struct win_size {
    int width;
    int hight;
} win_size_t;

typedef struct gui_scene gui_scene_t;
typedef struct window window_t;

typedef bool (*event_handler_t)(gui_scene_t* scene, gui_event_t* event);
typedef int (*event_scheduler_t)(gui_event_t* event); // returns handler index, -1 on fail

typedef struct window {
    WINDOW* win = NULL;
    bool active = false; // flag indicates window is active or not
    event_scheduler_t event_scheduler;
    event_handler_t event_handlers[MAX_WIN_HANDLERS];
    uv_mutex_t win_mutex;
    pos_t position;
    win_size_t size;
    void* attrs;
} window_t;

typedef struct gui_scene {
    event_scheduler_t event_scheduler;
    event_handler_t event_handlers[MAX_WIN_HANDLERS];
    window_t windows[MAX_WINDOWS];

    ThreadSafeList<gui_event_t>* pending_events;

    uint64_t scene_id;
    bool active = false;
    uv_mutex_t scene_mutex;

    int hight;
    int width;

    SceneKind scene_kind;
    SceneResult result;
    void* scene_attrs;

    client_t* client;
} gui_scene_t;

typedef struct event_thread_args {
    ThreadSafeList<gui_event_t>* queue;
    int hight;
    int width;
} event_args_t;

typedef struct highlight_list {
    size_t size;
    int light_idx;
} highlight_list_t;

#endif // OK_GUI_H
