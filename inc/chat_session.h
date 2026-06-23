#ifndef CHAT_SESSION_H
#define CHAT_SESSION_H

#include <assert.h>
#include "uv.h"

typedef struct server_ server_t;

enum CLIENT_STATE {
    CONNECTED = 0,
    DISCONNECTED = 1,
    LOGGED_IN = 2,
    UNDEFINED = 3,
};

typedef struct session {
    CLIENT_STATE state = CLIENT_STATE::DISCONNECTED;
    uv_stream_t* client_stream = NULL;
    uint64_t client_id   = 0; // client stream pointer hash
    uint64_t client_name = 0; // client name string hash
} chat_session_t;

chat_session_t* add_new_session(chat_session_t sessions[], uv_stream_t* stream, uint64_t client_id);

chat_session_t create_new_session(uv_stream_t* stream, uint64_t client_id);

void close_session(chat_session_t* session);

// returns bool flag weather state changed or not
bool change_state_session(chat_session_t sessions[], uint64_t client_id, CLIENT_STATE new_state);

chat_session_t* get_current_session(chat_session_t sessions[], uint64_t client_id);

chat_session_t* get_current_session_by_name(chat_session_t sessions[], uint64_t client_name);

CLIENT_STATE get_state_session(chat_session_t sessions[], uint64_t client_id);

#endif // CHAT_SESSION_H
