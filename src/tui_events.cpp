#include <ncurses.h>
#include <cassert>
#include "ts_list.h"
#include "gui_core.h"
#include "gui_events.h"
#include "enter_scene.h"
#include "chat_scene.h"

static int pool_event(gui_scene_t* scene, gui_event_t* new_event) {
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
        gui_event_t event = {};

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

SceneResult scene_render(gui_scene_t* scene) {
    fprintf(stderr, "[INFO] Starting render scene; scene_id=[%llu]\n", scene->scene_id);
    scene->result = SCENE_RESULT_CONTINUE;

    timeout(scene->scene_kind == SCENE_KIND_CHAT ? 40 : -1);
    while (ask_active(scene)) {
        chat_win_state_t* chat_state = nullptr;
        if (scene->scene_kind == SCENE_KIND_CHAT) {
            receive_chat_messages(scene);
            chat_state = get_chat_state(scene);
        }

        if (chat_state && chat_state->focus == FOCUS_INPUT) {
            handle_chat_input_key(scene, getch());
            continue;
        }

        int ch = getch();
        if (chat_state && (ch == KEY_PPAGE || ch == KEY_NPAGE)) {
            handle_chat_input_key(scene, ch);
            continue;
        }
        if (ch == 'q') {
            gui_event_t finalize_event = {.event_id = GUI_EVENT::FINALIZE};
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
        } else if (ch == KEY_UP) {
            gui_event_t arrow_up_event = {.event_id = GUI_EVENT::ARROW_UP};
            scene->pending_events->insert_head(arrow_up_event);
        } else if (ch == KEY_DOWN) {
            gui_event_t arrow_down_event = {.event_id = GUI_EVENT::ARROW_DOWN};
            scene->pending_events->insert_head(arrow_down_event);
        } else if (ch == KEY_LEFT) {
            gui_event_t arrow_left_event = {.event_id = GUI_EVENT::ARROW_LEFT};
            scene->pending_events->insert_head(arrow_left_event);
        } else if (ch == 10) {
            gui_event_t enter_event = {.event_id = GUI_EVENT::ENTER};
            scene->pending_events->insert_head(enter_event);
        }
        handle_pending_events(scene);
    }

    fprintf(stderr, "[INFO] Scene terminated; scene_id=[%llu]\n", scene->scene_id);
    return scene->result;
}

int schedule_scene_events(gui_event_t* event) {
    if (event->event_id == GUI_EVENT::RESIZE)
        return 0;
    if (event->event_id == GUI_EVENT::FINALIZE)
        return 1;
    return -1;
}

bool handle_resize_event(gui_scene_t* scene, gui_event_t* event) {
    if (scene->scene_kind == SCENE_KIND_CHAT)
        return handle_chat_resize_event(scene, event);
    return handle_enter_resize_event(scene, event);
}

bool handle_finalize_event(gui_scene_t* scene, gui_event_t* event) {
    assert(scene && "Scene nullptr in handle finalize event");
    assert(event && "Event nullptr in handle finalize event");
    assert(event->event_id == GUI_EVENT::FINALIZE && "Handle finalize event_id mismatch");

    scene->result = SCENE_RESULT_EXIT;
    terminate_scene(scene);
    endwin();
    g_ncurses_init = false;
    return true;
}
