#include <ncurses.h>
#include <cstring>
#include "memory.h"
#include "ts_list.h"
#include "gui_core.h"
#include "gui_events.h"
#include "enter_scene.h"
#include "sender.h"
#include "messages.h"

enum STATE {
    MAIN = 0,
    SIGN_IN_PROMPT = 1,
    REG_PROMPT = 2
};

typedef struct enter_win_state {
    STATE state;
    size_t size;
    int light_idx;
    bool show_error;
} enter_win_state_t;

static const int PROMPT_LABEL_COL   = 2;
static const int SIGN_USERNAME_ROW  = 5;
static const int SIGN_PASSWORD_ROW  = 6;
static const int SIGN_USERNAME_COL  = 12;
static const int SIGN_PASSWORD_COL  = 12;
static const int REG_USERNAME_ROW   = 5;
static const int REG_PASSWORD_ROW   = 6;
static const int REG_USERNAME_COL   = 19;
static const int REG_PASSWORD_COL   = 19;

static char* win_input(WINDOW* win, int start_y, int start_x, bool echo_mode) {
    char* input = CALLOC(MAX_INPUT_LEN, char);
    int pointer = 0;

    noecho();
    keypad(win, TRUE);
    curs_set(1);
    wmove(win, start_y, start_x);
    wrefresh(win);

    while (true) {
        int c = wgetch(win);

        if (c == 127 || c == KEY_BACKSPACE) {
            if (pointer > 0) {
                pointer--;
                input[pointer] = '\0';
                wmove(win, start_y, start_x + pointer);
                wclrtoeol(win);
                box(win, 0, 0);
                wmove(win, start_y, start_x + pointer);
                wrefresh(win);
            }
        } else if (c == 10 || c == '\n' || c == KEY_ENTER) {
            if (pointer > 0) {
                input[pointer] = '\0';
                curs_set(0);
                return input;
            }
        } else if (c != ERR && c >= 32 && c < 127 && pointer < MAX_INPUT_LEN - 1) {
            input[pointer] = (char)c;
            char shown = echo_mode ? (char)c : '*';
            mvwaddch(win, start_y, start_x + pointer, shown);
            pointer++;
            wmove(win, start_y, start_x + pointer);
            wrefresh(win);
        }
    }
}

static void print_y_centered(window_t* window, const char* label, int pos_y) {
    int center_x = (window->size.width - (int)strlen(label)) / 2;
    center_x = center_x < 0 ? 0 : center_x;
    mvwprintw(window->win, pos_y, center_x, label);
}

static void draw_enter_window(window_t* window) {
    enter_win_state_t* state = (enter_win_state_t*)window->attrs;
    werase(window->win);
    box(window->win, 0, 0);

    int header_y = 1;
    print_y_centered(window, "ODNOKURSNIKI", header_y);
    header_y += 1;
    if (state && state->show_error)
        print_y_centered(window, "INCORRECT USER NAME OR PASSWORD!!!", header_y);
    else
        print_y_centered(window, "SIGN IN/REGISTER?", header_y);

    int bottom_option_y = window->size.hight - 3;
    print_y_centered(window, "SIGN IN", bottom_option_y);
    bottom_option_y += 1;
    print_y_centered(window, "REGISTER", bottom_option_y);

    wrefresh(window->win);
}

static void draw_enter_window_sign_in(window_t* window) {
    enter_win_state_t* state = (enter_win_state_t*)window->attrs;
    werase(window->win);
    box(window->win, 0, 0);

    int header_y = 1;
    print_y_centered(window, "ODNOKURSNIKI", header_y);
    header_y += 1;
    if (state && state->show_error)
        print_y_centered(window, "INCORRECT USER NAME OR PASSWORD!!!", header_y);
    else
        print_y_centered(window, "SIGN IN/REGISTER?", header_y);

    int bottom_option_y = window->size.hight - 3;
    wattron(window->win, A_REVERSE);
        print_y_centered(window, "SIGN IN", bottom_option_y);
    wattroff(window->win, A_REVERSE);
    bottom_option_y += 1;
    print_y_centered(window, "REGISTER", bottom_option_y);

    wrefresh(window->win);
}

static void draw_enter_window_register(window_t* window) {
    enter_win_state_t* state = (enter_win_state_t*)window->attrs;
    werase(window->win);
    box(window->win, 0, 0);

    int header_y = 1;
    print_y_centered(window, "ODNOKURSNIKI", header_y);
    header_y += 1;
    if (state && state->show_error)
        print_y_centered(window, "INCORRECT USER NAME OR PASSWORD!!!", header_y);
    else
        print_y_centered(window, "SIGN IN/REGISTER?", header_y);

    int bottom_option_y = window->size.hight - 3;
    print_y_centered(window, "SIGN IN", bottom_option_y);
    bottom_option_y += 1;
    wattron(window->win, A_REVERSE);
        print_y_centered(window, "REGISTER", bottom_option_y);
    wattroff(window->win, A_REVERSE);

    wrefresh(window->win);
}

static void draw_prompt_register(window_t* window) {
    werase(window->win);
    box(window->win, 0, 0);

    int header_y = 1;
    print_y_centered(window, "ODNOKURSNIKI", header_y);
    header_y += 1;
    print_y_centered(window, "CREATE AN ACCOUNT!", header_y);
    header_y += 1;
    print_y_centered(window, "ENTER YOUR USERNAME AND PASSWORD", header_y);

    mvwprintw(window->win, REG_USERNAME_ROW, PROMPT_LABEL_COL, "CREATE USERNAME: ");
    mvwprintw(window->win, REG_PASSWORD_ROW, PROMPT_LABEL_COL, "CREATE PASSWORD: ");

    wrefresh(window->win);
}

static void draw_prompt_sign_in(window_t* window) {
    werase(window->win);
    box(window->win, 0, 0);

    int header_y = 1;
    print_y_centered(window, "ODNOKURSNIKI", header_y);
    header_y += 1;
    print_y_centered(window, "WELCOME BACK!", header_y);
    header_y += 1;
    print_y_centered(window, "ENTER YOUR USERNAME AND PASSWORD", header_y);

    mvwprintw(window->win, SIGN_USERNAME_ROW, PROMPT_LABEL_COL, "USERNAME: ");
    mvwprintw(window->win, SIGN_PASSWORD_ROW, PROMPT_LABEL_COL, "PASSWORD: ");

    wrefresh(window->win);
}

static int schedule_window_event(gui_event_t* event) {
    if (event->event_id == GUI_EVENT::ARROW_UP ||
        event->event_id == GUI_EVENT::ARROW_DOWN)
        return 0;
    if (event->event_id == GUI_EVENT::ENTER)
        return 1;
    if (event->event_id == GUI_EVENT::ARROW_LEFT)
        return 2;
    return -1;
}

static bool perform_sign_in(gui_scene_t* scene, window_t* enter_window) {
    draw_prompt_sign_in(enter_window);
    keypad(enter_window->win, TRUE);

    char* user_name = win_input(enter_window->win, SIGN_USERNAME_ROW, SIGN_USERNAME_COL, true);
    mvwprintw(enter_window->win, SIGN_PASSWORD_ROW, PROMPT_LABEL_COL, "PASSWORD: ");
    wrefresh(enter_window->win);
    char* password  = win_input(enter_window->win, SIGN_PASSWORD_ROW, SIGN_PASSWORD_COL, false);
    noecho();
    curs_set(0);

    if (scene->client && scene->client->session.state == CLIENT_STATE::CONNECTED) {
        ERR_STAT send_stat = send_message_login(scene->client, user_name, password);
        if (send_stat != ERR_STAT::SUCCESS) {
            fprintf(log_file, "[ERROR] Failed to send registger data to server; error_id=[%d]\n", send_stat);
        }
    }

    FREE(user_name);
    FREE(password);

    bool ok = true;
    if (ok) {
        scene->result = SCENE_RESULT_ENTER_CHAT;
        terminate_scene(scene);
        return true;
    }

    enter_win_state_t* state = (enter_win_state_t*)enter_window->attrs;
    state->state = STATE::MAIN;
    state->show_error = true;
    state->light_idx = 0;
    draw_enter_window_sign_in(enter_window);
    return true;
}

static bool perform_register(gui_scene_t* scene, window_t* enter_window) {
    draw_prompt_register(enter_window);
    keypad(enter_window->win, TRUE);

    char* user_name = win_input(enter_window->win, REG_USERNAME_ROW, REG_USERNAME_COL, true);
    mvwprintw(enter_window->win, REG_PASSWORD_ROW, PROMPT_LABEL_COL, "CREATE PASSWORD: ");
    wrefresh(enter_window->win);
    char* password  = win_input(enter_window->win, REG_PASSWORD_ROW, REG_PASSWORD_COL, true);
    noecho();
    curs_set(0);

    if (scene->client && scene->client->session.state == CLIENT_STATE::CONNECTED) {
        ERR_STAT send_stat = send_message_register(scene->client, user_name, password);
        if (send_stat != ERR_STAT::SUCCESS) {
            fprintf(log_file, "[ERROR] Failed to send registger data to server; error_id=[%d]\n", send_stat);
        }
    }

    FREE(user_name);
    FREE(password);

    scene->result = SCENE_RESULT_ENTER_CHAT;
    terminate_scene(scene);
    return true;
}

static bool handle_arrow_event(gui_scene_t* scene, gui_event_t* event) {
    window_t* enter_window = scene->windows;
    enter_win_state_t* state = (enter_win_state_t*)enter_window->attrs;

    if (state->state != STATE::MAIN)
        return true;

    if (event->event_id == GUI_EVENT::ARROW_UP) {
        state->light_idx += 1;
        state->light_idx = state->light_idx >= (int)state->size ? 0 : state->light_idx;
    } else {
        state->light_idx -= 1;
        state->light_idx = state->light_idx < 0 ? 1 : state->light_idx;
    }

    state->show_error = false;

    if (state->light_idx == 0) {
        draw_enter_window_sign_in(enter_window);
        return true;
    }

    draw_enter_window_register(enter_window);
    return true;
}

static bool handle_enter_event(gui_scene_t* scene, gui_event_t* event) {
    window_t* enter_window = scene->windows;
    enter_win_state_t* state = (enter_win_state_t*)enter_window->attrs;

    if (state->state != STATE::MAIN)
        return true;

    if (state->light_idx == 0)
        return perform_sign_in(scene, enter_window);

    return perform_register(scene, enter_window);
}

static bool handle_exit_event(gui_scene_t* scene, gui_event_t* event) {
    window_t* enter_window = scene->windows;
    enter_win_state_t* state = (enter_win_state_t*)enter_window->attrs;

    if (state->state == STATE::MAIN)
        return true;

    state->show_error = false;
    draw_enter_window(enter_window);
    state->state = STATE::MAIN;
    return true;
}

bool handle_enter_resize_event(gui_scene_t* scene, gui_event_t* event) {
    window_t* enter_window = &scene->windows[0];

    int screen_hight = event->scene_hight;
    int screen_width = event->scene_width;

    int new_y = ((int)screen_hight - enter_window->size.hight) / 2;
    int new_x = ((int)screen_width - enter_window->size.width) / 2;
    if (new_y < 0)
        new_y = 0;
    if (new_x < 0)
        new_x = 0;

    werase(enter_window->win);
    wrefresh(enter_window->win);

    mvwin(enter_window->win, new_y, new_x);
    enter_window->position.pos_x = new_x;
    enter_window->position.pos_y = new_y;

    enter_win_state_t* state = (enter_win_state_t*)enter_window->attrs;
    switch (state->state) {
        case STATE::MAIN:
            if (state->light_idx == 0)
                draw_enter_window_sign_in(enter_window);
            else
                draw_enter_window_register(enter_window);
            break;
        case STATE::SIGN_IN_PROMPT:
            draw_prompt_sign_in(enter_window);
            break;
        case STATE::REG_PROMPT:
            draw_prompt_register(enter_window);
            break;
        default:
            break;
    }

    refresh();
    scene->hight = screen_hight;
    scene->width = screen_width;
    return true;
}

gui_scene_t* create_enter_scene() {
    init_ncurses_once();
    if (!g_ncurses_init)
        return NULL;

    clear();
    refresh();

    gui_scene_t* enter_scene = CALLOC(1, gui_scene_t);
    enter_scene->pending_events = new ThreadSafeList<gui_event_t>(10);
    enter_scene->active = true;
    enter_scene->scene_kind = SCENE_KIND_ENTER;
    enter_scene->result = SCENE_RESULT_CONTINUE;
    uv_mutex_init(&enter_scene->scene_mutex);

    int screen_height = 0;
    int screen_width  = 0;
    getmaxyx(stdscr, screen_height, screen_width);

    enter_scene->hight = screen_height;
    enter_scene->width = screen_width;

    int enter_win_height = 10;
    int enter_win_width  = 50;

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

    enter_win_state_t* win_state = CALLOC(1, enter_win_state_t);
    win_state->state = STATE::MAIN;
    win_state->light_idx = 0;
    win_state->size = 2;
    win_state->show_error = false;
    enter_scene->windows[0].attrs = (void*)win_state;
    enter_scene->windows[0].event_scheduler   = schedule_window_event;
    enter_scene->windows[0].event_handlers[0] = handle_arrow_event;
    enter_scene->windows[0].event_handlers[1] = handle_enter_event;
    enter_scene->windows[0].event_handlers[2] = handle_exit_event;
    for (size_t i = 0; i < MAX_WINDOWS; i ++)
        uv_mutex_init(&enter_scene->windows[i].win_mutex);

    refresh();
    draw_enter_window_sign_in(&enter_scene->windows[0]);

    enter_scene->event_scheduler   = schedule_scene_events;
    enter_scene->event_handlers[0] = handle_resize_event;
    enter_scene->event_handlers[1] = handle_finalize_event;

    return enter_scene;
}
