#include <ncurses.h>
#include "memory.h"
#include "ts_list.h"
#include "gui_core.h"

FILE* logerr = NULL;
bool g_ncurses_init = false;

void init_ncurses_once() {
    if (g_ncurses_init)
        return;
    if (initscr() == NULL) {
        fprintf(stderr, "[FATAL] initscr failed (run from a real terminal)\n");
        return;
    }
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    g_ncurses_init = true;
}

bool ask_active(gui_scene_t* scene) {
    bool active = false;
    uv_mutex_lock(&scene->scene_mutex);
        active = scene->active;
    uv_mutex_unlock(&scene->scene_mutex);
    return active;
}

bool ask_active(window_t* window) {
    bool active = false;
    uv_mutex_lock(&window->win_mutex);
        active = window->active;
    uv_mutex_unlock(&window->win_mutex);
    return active;
}

void terminate_scene(gui_scene_t* scene) {
    uv_mutex_lock(&scene->scene_mutex);
        scene->active = false;
        for (size_t i = 0; i < MAX_WINDOWS; i ++) {
            WINDOW* erase_win = scene->windows[i].win;
            if (!erase_win)
                continue;
            delwin(erase_win);
            scene->windows[i].win = NULL;
            scene->windows[i].active = false;
        }
    uv_mutex_unlock(&scene->scene_mutex);

    if (g_ncurses_init) {
        erase();
        refresh();
    }
}

void cleanup_scene(gui_scene_t* scene) {
    if (!scene)
        return;
    if (scene->windows[0].attrs)
        FREE(scene->windows[0].attrs);
    if (scene->scene_attrs)
        FREE(scene->scene_attrs);
    delete scene->pending_events;
    FREE(scene);
}
