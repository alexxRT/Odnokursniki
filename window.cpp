#include <ncurses.h>
#include <uv.h>
#include <pthread.h>
#include "inc/memory.h"
#include "inc/ts_list.h"
#include "inc/gui.h"

FILE* logerr = NULL;
#define stderr logerr

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
            werase(erase_win);
            wrefresh(erase_win);
            delwin(erase_win);
            scene->windows[i].win = NULL;
        }
    uv_mutex_unlock(&scene->scene_mutex);
}

int pool_event(gui_scene_t* scene, gui_event_t* new_event) {
    scene->pending_events->wait();
    list_elem_t<gui_event_t>* head = scene->pending_events->get_head();
    if (!head)
        return -1;
    gui_event_t event = head->data;
    scene->pending_events->delete_head();
    (*new_event) = event;
    return 0;
}

void handle_pending_events(gui_scene_t* scene) {
    while (scene->pending_events->count() > 0 && ask_active(scene)) {
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
            bool handled = scene->event_handlers[event_handler_idx](scene, &event);
            handled ? fprintf(stderr, "[INFO] Scene event handled; event_id=[%llu], scene_id=[%llu]\n", event.event_id, scene->scene_id)
                    : fprintf(stderr, "[ERROR] Invoked handler failed handle event; scene_id=[%llu]\n", scene->scene_id);
            continue;
        }

        fprintf(stderr, "[DEBUG] Scene skipping unhandled event; event_id=[%llu], scene_id=[%llu]\n",
                    event.event_id, scene->scene_id);

        for (size_t i = 0; i < MAX_WINDOWS; i ++) {
            if (ask_active(scene->windows + i)) {
                int win_scheduler_idx = scene->windows[i].event_scheduler(&event);
                if (win_scheduler_idx >= 0) {
                    bool handled = scene->windows[i].event_handlers[win_scheduler_idx](scene, &event);
                    handled ? fprintf(stderr, "[INFO] Window event handled; event_id=[%llu], scene_id=[%llu]\n", event.event_id, scene->scene_id)
                            : fprintf(stderr, "[ERROR] Invoked failed to handle window event; scene_id=[%llu], window=[%zu]; event_id=[%llu]\n",
                                                    scene->scene_id, i, event.event_id);
                }
            }
        }
    }
}


// that is a thread routine, before starting new scene, previous must be joined
void scene_render(gui_scene_t* scene) {
    fprintf(stderr, "[INFO] Starting render scene; scene_id=[%llu]\n", scene->scene_id);

    while (ask_active(scene)) {
        int ch = getch();
        if (ch == 'q') {
            gui_event_t finalize_event = {
                .event_id = GUI_EVENT::FINALIZE,
            };
            scene->pending_events->insert_head(finalize_event);
        } else if (ch == KEY_RESIZE) {
            int screen_hight = 0;
            int screen_width = 0;
            getmaxyx(stdscr, screen_hight, screen_width);

            if (scene->hight != screen_hight || scene->width != screen_width) {
                gui_event_t resize_event = {
                    .event_id    = GUI_EVENT::RESIZE,
                    .scene_width = screen_width,
                    .scene_hight = screen_hight,
                };
                scene->pending_events->insert_head(resize_event);
            }
        }
        handle_pending_events(scene);
    }
    fprintf(stderr, "[INFO] Scene terminated; scene_id=[%llu]\n", scene->scene_id);
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
    fprintf(stderr, "[WARNING:SCHEDULER] Unknown event id, event ignored; event_id=[%llu]\n", event->event_id);
    return unknown_event_indx;
}

void draw_enter_window(window_t* window) {
    werase(window->win);
    box(window->win, 0, 0);

    int header_y = 1;
    int header_x = (window->size.width - strlen("ODNOKURSNIKI")) / 2;
    header_x = header_x < 0 ? 0 : header_x;
    mvwprintw (window->win, header_y, header_x, "ODNOKURSNIKI");

    header_y += 1;
    header_x = (window->size.width - strlen("SIGN IN/REGISTER?")) / 2;
    header_x = header_x < 0 ? 0 : header_x;
    mvwprintw (window->win, header_y, header_x, "SIGN IN/REGISTER?");

    int bottom_option_y = window->size.hight - 3;
    int bottom_option_x = (window->size.width - strlen("SIGN_IN")) / 2;
    bottom_option_x = bottom_option_x < 0 ? 0 : bottom_option_x;
    mvwprintw (window->win, bottom_option_y, bottom_option_x, "SIGN IN");

    bottom_option_y += 1;
    bottom_option_x = (window->size.width - strlen("REGISTER")) / 2;
    bottom_option_x = bottom_option_x < 0 ? 0 : bottom_option_x;
    mvwprintw (window->win, bottom_option_y, bottom_option_x, "REGISTER");

    wrefresh(window->win);
}

bool handle_resize_event(gui_scene_t* scene, gui_event_t* event) {
    assert(event->event_id == GUI_EVENT::RESIZE && "Handle resize event id mismatch");
    assert(scene && "Scene nullptr when handling resize event");

    window_t* enter_window = &scene->windows[0];

    assert(enter_window->win && "Enter win is NULL in handle resize event");

    int screen_hight = event->scene_hight;
    int screen_width = event->scene_width;

    int new_y = ((int)screen_hight - enter_window->size.hight) / 2;
    int new_x = ((int)screen_width - enter_window->size.width) / 2;
    if (new_y < 0) {
        new_y = 0;
    }
    if (new_x < 0) {
        new_x = 0;
    }

    werase(enter_window->win);
    wrefresh(enter_window->win);

    mvwin(enter_window->win, new_y, new_x);
    enter_window->position.pos_x = new_x;
    enter_window->position.pos_y = new_y;

    draw_enter_window(enter_window);
    refresh();

    scene->hight = screen_hight;
    scene->width = screen_width;

    return true;
}

bool handle_finalize_event(gui_scene_t* scene, gui_event_t* event) {
    assert(scene && "Scene nullptr in handle finalize event");
    assert(event && "Event nullptr in handle finalize event");
    assert(event->event_id == GUI_EVENT::FINALIZE && "Handle finalize event_id mismatch");

    window_t* enter_window = &scene->windows[0];

    assert(enter_window->win && "Window nullptr in finalize event");

    terminate_scene(scene);
    endwin();
    return true;
}


gui_scene_t* create_enter_scene() {
    if (initscr() == NULL) {
        fprintf(stderr, "[FATAL] initscr failed (run from a real terminal)\n");
        return NULL;
    }

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    gui_scene_t* enter_scene = CALLOC(1, gui_scene_t);
    enter_scene->pending_events = new ThreadSafeList<gui_event_t>(10);
    enter_scene->active = true;
    uv_mutex_init(&enter_scene->scene_mutex);

    int screen_height = 0;
    int screen_width  = 0;
    getmaxyx(stdscr, screen_height, screen_width);

    enter_scene->hight = screen_height;
    enter_scene->width = screen_width;

    int enter_win_height = 10;
    int enter_win_width  = 30;

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
    uv_mutex_init(&enter_scene->windows[0].win_mutex);

    refresh();
    draw_enter_window(&enter_scene->windows[0]);

    // set scene event scheduler
    enter_scene->event_scheduler = schedule_scene_events;

    // set scene event handlers
    enter_scene->event_handlers[0] = handle_resize_event;
    enter_scene->event_handlers[1] = handle_finalize_event;

    return enter_scene;
}

void run_gui_app() {
    logerr = fopen("logs.txt", "w");
    gui_scene_t* enter_scene = create_enter_scene();
    if (!enter_scene)
        return;

    pthread_t pid;
    scene_render(enter_scene);

    delete enter_scene->pending_events;
    FREE(enter_scene);
}

int main() {

    run_gui_app();

    return 0;
}
