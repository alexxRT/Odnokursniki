#ifndef OK_CHAT_SCENE_H
#define OK_CHAT_SCENE_H

#include "gui.h"

enum ChatFocus {
    FOCUS_MENU  = 0,
    FOCUS_INPUT = 1
};

typedef struct chat_win_state {
    ChatFocus focus;
    int highlight_idx;
    int active_client_idx;
    char input_buf[MAX_INPUT_LEN];
    int input_len;
} chat_win_state_t;

chat_win_state_t* get_chat_state(gui_scene_t* scene);
void receive_chat_messages(gui_scene_t* scene);
void handle_chat_input_key(gui_scene_t* scene, int ch);
bool handle_chat_resize_event(gui_scene_t* scene, gui_event_t* event);
gui_scene_t* create_chat_scene();

#endif // OK_CHAT_SCENE_H
