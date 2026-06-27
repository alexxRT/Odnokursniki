#include <ncurses.h>
#include <cstdio>
#include <uv.h>
#include <pthread.h>
#include "inc/ts_list.h"
#include "inc/gui.h"


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

void terminate_scene(gui_event_t* scene) {
    uv_mutex_lock(&scene->scene_mutex);
        scene->active = false;
        for (size_t i = 0; i < MAX_WINDOWS; i ++) {
            WINDOW* erase_win = scene->windows[i].win;
            if (!erase_win)
                continue;
            werase(erase_win);
            wrefresh(erase_win);
        }
    uv_mutex_unlock(&scene->scene_mutex);
}

int pool_event(gui_scene_t* scene, gui_event_t* new_event) {
    scene->pending_events->wait();
    gui_event_t event = scene->pending_events->get_head()->data;
    scene->pending_events->delete_head();
    (*new_event) = event;
    return 0;
}


// that is a thread routine, before starting new scene, previous must be joined
void scene_render(gui_scene_t* scene) {
    fprintf(stderr, "[INFO] Starting render thread; scene_id=[%llu]\n", scene->scene_id);
    while (ask_active(scene)) {
        gui_event_t event = {
        };

        int pool_stat = pool_event(scene, &event);
        fprintf(stderr, "[INFO] New event pooled; event_id=[%llu], scene_id=[%llu]\n",
                            event.event_id, scene->scene_id);

        if (pool_stat < 0) {
            fprintf(stderr, "[FATAL] Scene failed to pool new event; scene_id=[%llu]\n", scene->scene_id);
            terminate_scene(scene);
            break;
        }

        int event_handler_idx = scene->event_scheduler(&event);
        if (event_handler_idx >= 0) {
            bool handled = scene->event_handlers[event_handler_idx](&event);
            handled ? fprintf(stderr, "[INFO] Scene event handled; event_id=[%llu], scene_id=[%llu]\n", event.event_id, scene->scene_id)
                    : fprintf(stderr, "[ERROR] Invoked handler failed handle event; scene_id=[%llu]\n", scene->scene_id);
            continue;
        }

        fprintf(stderr, "[DEBUG] Scene skipping unhandled event; event_id=[%llu], scene_id=[%llu]\n",
                    event.event_id, scene->scene_id);

        for (size_t i = 0; i < MAX_WINDOWS; i ++) {
            if (ask_active(scene->windows + i)) {
                int win_scheduler_idx = scene->windows[i].event_scheduler(&event);
                if (event_handler_idx >= 0) {
                    bool handled = scene->windows[i].event_handlers[event_handler_idx](&event);
                    handled ? fprintf(stderr, "[INFO] Window event handled; event_id=[%llu], scene_id=[%llu]\n", event.event_id, scene->scene_id)
                            : fprintf(stderr, "[ERROR] Invoked failed to handle window event; scene_id=[%llu], window=[%u]; event_id=[%llu]\n",
                                                    scene->scene_id, i, event.event_id);
                }
            }
        }
    }
    fprintf(stderr, "[INFO] Scene terminated; scene_id=[%llu]\n", scene->scene_id);
}


void event_producer(void* thread_args) {
    event_args_t* args = (event_args_t*)thread_args;
    ThreadSafeList<gui_event_t>* event_queue =
            static_cast<ThreadSafeList<gui_event_t>>(args->queue);

    size_t scene_width = args->width;
    size_t scene_hight = args->hight;

    while (true) {
        char ch = getchar();

        if (ch == 'q') {
            fprinf(stderr, "[INFO] 'q' button pressed, exiting GUI event loop\n");
            gui_event_t finalize_event = {
                .event_id = GUI_EVENT::FINALIZE
            };
            event_queue->insert_head(finalize_event);
            break;
        }

        if (ch == KEY_RESIZE) {
            if (scene_hight != LINES || scene_width != COLS) {
                fprintf(stderr, "[INFO] Resize event triggered\n");
                gui_event_t resize_event = {
                    .event_id = GUI_EVENT::RESIZE,
                    .scene_width = COLS,
                    .scene_hight = LINES,
                };
                event_queue->insert_head(resize_event);
            }
            continue;
        }
    }
    fprintf(stderr, "[INFO] Event producer GUI loop terminated\n");
}

int schedule_scene_events(gui_event_t* event) {
    if (event->event_id == GUI_EVENT::RESIZE) {
        const int resize_handler_indx = 0;
        fprintf(stderr, "[INFO:SCHEDULER] Scheduled GUI event to handler; index=[%d]\n", resize_handler_indx);
        return resize_handler_indx;
    }

    if (event->event_id == GUI_EVENT::FINALIZE) {
        const int finalize_handler_indx = 1;
        fprintf(stderr, "[INFO:SCHEDULER] Scheduled GUI event to handler; index=[%d]\n", finalize_handler_indx);
        return finalize_handler_indx;
    }

    const int unknown_event_indx = -1;
    fprintf(stderr, "[WARNING:SCHEDULER] Unknown event id, event ignored; event_id=[%lu]\n", event->event_id);
    return unknown_event_indx;
}

void draw_enter_window(window_t* window) {
    box(window->win, 0, 0);
    mvwprintw(window->win, window->height / 2, 2, "Press q to quit");
    wrefresh(window->win);
}

bool handle_resize_event(gui_scene_t* scene, gui_event_t* event) {
    assert(event->event_id == GUI_EVENT::RESIZE && "Handle resize event id mismatch");
    assert(scene && "Scene nullptr when handling resize event");

    window_t enter_window = scene->windows[0];

    assert(enter_window.win && "Enter win is NULL in handle resize event");

    size_t screen_hight = event->scene_hight;
    size_t screen_width = event->scene_width;

    int new_y = (screen_hight - enter_window.hight) / 2;
    int new_x = (screen_width - enter_window.width) / 2;
    if (new_y < 0) {
        new_y = 0;
    }
    if (new_x < 0) {
        new_x = 0;
    }

    werase(enter_window.win);
    wrefresh(enter_window.win);

    mvwin(enter_window.win, new_x, new_y);
    draw_enter_window(&enter_window);

    return true;
}

bool handle_finalize_event(gui_scene_t* scene, gui_event_t* event) {
    assert(scene && "Scene nullptr in handle finalize event");
    assert(event && "Event nullptr in handle finalize event");
    assert(event->event_id == GUI_EVENT::FINALIZE && "Handle finalize event_id mismatch");

    window_t enter_window = scene->windows[0];

    assert(enter_window.win && "Window nullptr in finalize event");

    delwin(enter_window.win);
    endwin();
    FREE(scene);
}


gui_scene_t* create_enter_scene() {
    gui_scene_t* enter_scene = CALLOC(1, gui_scene_t);
    ThreadSafeList<gui_event_t> gui_events_queue = new ThreadSafeList<gui_event_t>(10);
    enter_scene->pending_events = gui_events_queue;

    keypad(stdscr, TRUE);
    curs_set(0);

    int screen_height = 0;
    int screen_width  = 0;
    getmaxyx(stdscr, screen_height, screen_width);

    enter_scene->hight = screen_height;
    enter_scene->width = screen_width;

    int enter_win_height = 10;
    int enter_win_width  = 20;

    int pos_x = (screen_width - enter_win_width) / 2;
    int pos_y = (screen_height - enter_win_height) / 2;
    if (pos_y < 0) pos_y = 0;
    if (pos_x < 0) pos_x = 0;

    WINDOW* enter_win = newwin(enter_win_height, enter_win_width, pos_y, pos_x);
    enter_scene->windows[0] = {
        .active = true,
        .win = enter_win,
        .position = {pos_x, pos_y},
        .size     = {enter_win_width, enter_win_height}
    };

    draw_enter_window(enter_scene->windows);

    // set scene event scheduler
    enter_scene->event_scheduler = (event_scheduler_t)schedule_scene_events;

    // set scene event handlers
    enter_scene->event_handlers[0] = (event_handler_t)handle_resize_event;
    enter_scene->event_handlers[1] = (event_handler_t)handle_finalize_event;

    return enter_scene;
}

void run_gui_app() {
    gui_scene_t* enter_scene = create_enter_scene();

    event_args_t args = {
        .hight = enter_scene->hight,
        .width = enter_scene->width,
        .queue = enter_scene->pending_events
    };

    pthread_t pid;
    pthread_create(&pid, NULL, &event_producer, (void*)&args);

    scene_render(enter_scene);

    pthread_join(pid, NULL);
}


int main() {

    run_gui_app();

    return 0;
}
