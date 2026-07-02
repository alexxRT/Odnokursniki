#include <ncurses.h>
#include <cstring>
#include <string>
#include <vector>
#include "memory.h"
#include "ts_list.h"
#include "gui_core.h"
#include "gui_events.h"
#include "chat_scene.h"
#include "sender.h"

struct MockClient {
    const char* name;
    bool online;
    std::vector<std::string> messages;
};

static MockClient mock_clients[] = {
    {"QUIT",  true,  {}},
    {"alice", true,  {}},
    {"bob",   false, {}},
    {"leha",  true,  {}},
    {"masha", false, {}},
};

static const int MOCK_CLIENT_COUNT = sizeof(mock_clients) / sizeof(mock_clients[0]);

static const int CHAT_MENU_WIN   = 0;
static const int CHAT_DIALOG_WIN = 1;
static const int CHAT_INPUT_WIN  = 2;

static void calc_chat_layout(int screen_h, int screen_w,
                             int* menu_x, int* menu_y, int* menu_w, int* menu_h,
                             int* dialog_x, int* dialog_y, int* dialog_w, int* dialog_h,
                             int* input_x, int* input_y, int* input_w, int* input_h) {
    *menu_x = 1;
    *menu_y = 1;
    *menu_w = screen_w / 3;
    *menu_h = screen_h - 1;

    *dialog_w = (2 * screen_w / 3) - 3;
    *dialog_x = screen_w / 3 + 3;
    *dialog_y = 1;
    int total_right_h = screen_h - 1;
    *dialog_h = 3 * total_right_h / 4;

    *input_w = *dialog_w;
    *input_x = *dialog_x;
    *input_y = *dialog_y + *dialog_h;
    *input_h = total_right_h / 4;
}

chat_win_state_t* get_chat_state(gui_scene_t* scene) {
    return (chat_win_state_t*)scene->scene_attrs;
}

static void draw_menu_win(gui_scene_t* scene) {
    window_t* menu = &scene->windows[CHAT_MENU_WIN];
    chat_win_state_t* state = get_chat_state(scene);

    werase(menu->win);
    for (int i = 0; i < MOCK_CLIENT_COUNT; i++) {
        if (state->highlight_idx == i)
            wattron(menu->win, A_REVERSE);

        mvwprintw(menu->win, i + 1, 1, "%s", mock_clients[i].name);

        if (i > 0) {
            const char* status = mock_clients[i].online ? "online" : "offline";
            int status_x = menu->size.width - (int)strlen(status) - 2;
            if (status_x < 1)
                status_x = 1;
            mvwprintw(menu->win, i + 1, status_x, "%s", status);
        }

        wattroff(menu->win, A_REVERSE);
    }
    box(menu->win, 0, 0);
}

static void draw_dialog_win(gui_scene_t* scene) {
    window_t* dialog = &scene->windows[CHAT_DIALOG_WIN];
    chat_win_state_t* state = get_chat_state(scene);

    werase(dialog->win);

    if (state->active_client_idx > 0) {
        MockClient& client = mock_clients[state->active_client_idx];
        int max_lines = dialog->size.hight - 2;
        int msg_count = (int)client.messages.size();
        int start = msg_count > max_lines ? msg_count - max_lines : 0;
        int row = 1;
        for (int i = start; i < msg_count; i++) {
            mvwprintw(dialog->win, row, 1, "%s", client.messages[i].c_str());
            row++;
        }
    }

    box(dialog->win, 0, 0);
}

static void draw_input_win(gui_scene_t* scene) {
    window_t* input = &scene->windows[CHAT_INPUT_WIN];
    chat_win_state_t* state = get_chat_state(scene);

    werase(input->win);
    if (state->input_len > 0)
        mvwprintw(input->win, 1, 1, "%s", state->input_buf);

    if (state->focus == FOCUS_INPUT) {
        int cursor_x = state->input_len + 1;
        if (cursor_x < input->size.width - 1)
            mvwchgat(input->win, 1, cursor_x, 1, A_REVERSE, 0, NULL);
    }

    box(input->win, 0, 0);
}

static void flush_chat_scene(gui_scene_t* scene) {
    for (int i = 0; i < 3; i++)
        touchwin(scene->windows[i].win);
    wrefresh(scene->windows[CHAT_MENU_WIN].win);
    wrefresh(scene->windows[CHAT_DIALOG_WIN].win);
    wrefresh(scene->windows[CHAT_INPUT_WIN].win);
}

static void redraw_chat_scene(gui_scene_t* scene) {
    draw_menu_win(scene);
    draw_dialog_win(scene);
    draw_input_win(scene);
    flush_chat_scene(scene);
}

static void set_chat_focus(gui_scene_t* scene, ChatFocus focus) {
    chat_win_state_t* state = get_chat_state(scene);
    state->focus = focus;

    scene->windows[CHAT_MENU_WIN].active = (focus == FOCUS_MENU);
    scene->windows[CHAT_INPUT_WIN].active = (focus == FOCUS_INPUT);

    if (focus == FOCUS_MENU)
        curs_set(0);
    else
        curs_set(1);

    redraw_chat_scene(scene);
}

static void send_chat_message(gui_scene_t* scene) {
    chat_win_state_t* state = get_chat_state(scene);
    if (state->input_len <= 0 || state->active_client_idx <= 0)
        return;

    // display message on dialog window
    mock_clients[state->active_client_idx].messages.emplace_back(state->input_buf);
    if (scene->client && scene->client->session.state == CLIENT_STATE::CONNECTED) { // send message to addressant via backend
        ERR_STAT send_stat = send_message_text(scene->client, mock_clients[state->active_client_idx].name,
                                                              state->input_buf);
        if (send_stat != ERR_STAT::SUCCESS)
            fprintf(logerr, "[ERROR] Failed to send text message to addressant; error_id=[%d]\n", send_stat);
    }

    memset(state->input_buf, 0, sizeof(state->input_buf));
    state->input_len = 0;

    draw_dialog_win(scene);
    draw_input_win(scene);
    flush_chat_scene(scene);
}

void handle_chat_input_key(gui_scene_t* scene, int ch) {
    chat_win_state_t* state = get_chat_state(scene);

    if (ch == 'q') {
        gui_event_t finalize_event = {.event_id = GUI_EVENT::FINALIZE};
        scene->pending_events->insert_head(finalize_event);
        handle_pending_events(scene);
        return;
    }

    if (ch == KEY_RESIZE) {
        int screen_h = 0;
        int screen_w = 0;
        getmaxyx(stdscr, screen_h, screen_w);
        if (scene->hight != screen_h || scene->width != screen_w) {
            gui_event_t resize_event = {
                .event_id    = GUI_EVENT::RESIZE,
                .scene_width = screen_w,
                .scene_hight = screen_h,
            };
            scene->pending_events->insert_head(resize_event);
            handle_pending_events(scene);
        }
        return;
    }

    if (ch == KEY_LEFT) {
        gui_event_t left_event = {.event_id = GUI_EVENT::ARROW_LEFT};
        scene->pending_events->insert_head(left_event);
        handle_pending_events(scene);
        return;
    }

    if (ch == 10) {
        send_chat_message(scene);
        return;
    }

    if (ch == 127 || ch == KEY_BACKSPACE) {
        if (state->input_len > 0) {
            state->input_len--;
            state->input_buf[state->input_len] = '\0';
            draw_input_win(scene);
            flush_chat_scene(scene);
        }
        return;
    }

    if (ch >= 32 && ch < 127 && state->input_len < MAX_INPUT_LEN - 1) {
        state->input_buf[state->input_len] = (char)ch;
        state->input_len++;
        state->input_buf[state->input_len] = '\0';
        draw_input_win(scene);
        flush_chat_scene(scene);
    }
}

bool handle_chat_resize_event(gui_scene_t* scene, gui_event_t* event) {
    int screen_h = event->scene_hight;
    int screen_w = event->scene_width;

    erase();
    refresh();

    int menu_x, menu_y, menu_w, menu_h;
    int dialog_x, dialog_y, dialog_w, dialog_h;
    int input_x, input_y, input_w, input_h;
    calc_chat_layout(screen_h, screen_w,
                     &menu_x, &menu_y, &menu_w, &menu_h,
                     &dialog_x, &dialog_y, &dialog_w, &dialog_h,
                     &input_x, &input_y, &input_w, &input_h);

    window_t* wins[] = {
        &scene->windows[CHAT_MENU_WIN],
        &scene->windows[CHAT_DIALOG_WIN],
        &scene->windows[CHAT_INPUT_WIN],
    };
    int xs[] = {menu_x, dialog_x, input_x};
    int ys[] = {menu_y, dialog_y, input_y};
    int ws[] = {menu_w, dialog_w, input_w};
    int hs[] = {menu_h, dialog_h, input_h};

    for (int i = 0; i < 3; i++) {
        wresize(wins[i]->win, hs[i], ws[i]);
        mvwin(wins[i]->win, ys[i], xs[i]);
        wins[i]->position.pos_x = xs[i];
        wins[i]->position.pos_y = ys[i];
        wins[i]->size.width = ws[i];
        wins[i]->size.hight = hs[i];
    }

    scene->hight = screen_h;
    scene->width = screen_w;
    redraw_chat_scene(scene);
    return true;
}

static int schedule_chat_menu_event(gui_event_t* event) {
    if (event->event_id == GUI_EVENT::ARROW_UP ||
        event->event_id == GUI_EVENT::ARROW_DOWN)
        return 0;
    if (event->event_id == GUI_EVENT::ENTER)
        return 1;
    return -1;
}

static int schedule_chat_input_event(gui_event_t* event) {
    if (event->event_id == GUI_EVENT::ARROW_LEFT)
        return 0;
    if (event->event_id == GUI_EVENT::ENTER)
        return 1;
    return -1;
}

static bool handle_chat_menu_arrow(gui_scene_t* scene, gui_event_t* event) {
    chat_win_state_t* state = get_chat_state(scene);
    if (state->focus != FOCUS_MENU)
        return true;

    if (event->event_id == GUI_EVENT::ARROW_UP) {
        if (state->highlight_idx > 0)
            state->highlight_idx--;
    } else {
        if (state->highlight_idx < MOCK_CLIENT_COUNT - 1)
            state->highlight_idx++;
    }

    draw_menu_win(scene);
    flush_chat_scene(scene);
    return true;
}

static bool handle_chat_menu_enter(gui_scene_t* scene, gui_event_t* event) {
    chat_win_state_t* state = get_chat_state(scene);

    if (state->highlight_idx == 0) {
        scene->result = SCENE_RESULT_BACK_TO_ENTER;
        terminate_scene(scene);
        return true;
    }

    state->active_client_idx = state->highlight_idx;
    memset(state->input_buf, 0, sizeof(state->input_buf));
    state->input_len = 0;

    draw_dialog_win(scene);
    draw_input_win(scene);
    set_chat_focus(scene, FOCUS_INPUT);
    return true;
}

static bool handle_chat_input_left(gui_scene_t* scene, gui_event_t* event) {
    set_chat_focus(scene, FOCUS_MENU);
    return true;
}

static bool handle_chat_input_enter(gui_scene_t* scene, gui_event_t* event) {
    send_chat_message(scene);
    return true;
}

gui_scene_t* create_chat_scene() {
    gui_scene_t* chat_scene = CALLOC(1, gui_scene_t);
    chat_scene->pending_events = new ThreadSafeList<gui_event_t>(10);
    chat_scene->active = true;
    chat_scene->scene_kind = SCENE_KIND_CHAT;
    chat_scene->result = SCENE_RESULT_CONTINUE;
    uv_mutex_init(&chat_scene->scene_mutex);

    int screen_h = 0;
    int screen_w = 0;
    getmaxyx(stdscr, screen_h, screen_w);
    chat_scene->hight = screen_h;
    chat_scene->width = screen_w;

    int menu_x, menu_y, menu_w, menu_h;
    int dialog_x, dialog_y, dialog_w, dialog_h;
    int input_x, input_y, input_w, input_h;
    calc_chat_layout(screen_h, screen_w,
                     &menu_x, &menu_y, &menu_w, &menu_h,
                     &dialog_x, &dialog_y, &dialog_w, &dialog_h,
                     &input_x, &input_y, &input_w, &input_h);

    chat_win_state_t* chat_state = CALLOC(1, chat_win_state_t);
    chat_state->focus = FOCUS_MENU;
    chat_state->highlight_idx = 1;
    chat_state->active_client_idx = -1;
    chat_state->input_len = 0;
    chat_scene->scene_attrs = chat_state;

    chat_scene->windows[CHAT_MENU_WIN] = {
        .active = true,
        .win = newwin(menu_h, menu_w, menu_y, menu_x),
        .position = {menu_x, menu_y},
        .size = {menu_w, menu_h},
        .event_scheduler = schedule_chat_menu_event,
    };
    chat_scene->windows[CHAT_MENU_WIN].event_handlers[0] = handle_chat_menu_arrow;
    chat_scene->windows[CHAT_MENU_WIN].event_handlers[1] = handle_chat_menu_enter;

    chat_scene->windows[CHAT_DIALOG_WIN] = {
        .active = false,
        .win = newwin(dialog_h, dialog_w, dialog_y, dialog_x),
        .position = {dialog_x, dialog_y},
        .size = {dialog_w, dialog_h},
    };

    chat_scene->windows[CHAT_INPUT_WIN] = {
        .active = false,
        .win = newwin(input_h, input_w, input_y, input_x),
        .position = {input_x, input_y},
        .size = {input_w, input_h},
        .event_scheduler = schedule_chat_input_event,
    };
    chat_scene->windows[CHAT_INPUT_WIN].event_handlers[0] = handle_chat_input_left;
    chat_scene->windows[CHAT_INPUT_WIN].event_handlers[1] = handle_chat_input_enter;

    for (size_t i = 0; i < MAX_WINDOWS; i++)
        uv_mutex_init(&chat_scene->windows[i].win_mutex);

    redraw_chat_scene(chat_scene);

    chat_scene->event_scheduler   = schedule_scene_events;
    chat_scene->event_handlers[0] = handle_resize_event;
    chat_scene->event_handlers[1] = handle_finalize_event;

    return chat_scene;
}
