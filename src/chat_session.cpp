#include <assert.h>
#include "chat_session.h"
#include "chat_configs.h"
#include "networking.h"
#include "uv.h"

chat_session_t* add_new_session(chat_session_t sessions[], uv_stream_t* stream, uint64_t client_id) {
    assert(stream && "[ERROR] Client stream must be valid to create new session");

    chat_session_t new_session = {
        .client_id = client_id,
        .client_stream = stream,
        .state = CLIENT_STATE::CONNECTED
    };

    // return NULL if session exists; user must use get_session();
    for (size_t i = 0; i < MAX_SESSIONS_COUNT; i ++) {
        if (client_id == sessions[i].client_id) {
            fprintf(stderr, "[ERROR] Trying create already existing session\n");
            return NULL;
        }
    }

    for (size_t i = 0; i < MAX_SESSIONS_COUNT; i ++) {
        if (sessions[i].client_id == 0) {
            sessions[i] = new_session;
            return sessions + i;
        }
    }

    fprintf(stderr, "[ERROR] Server sessions capacity reached; no session created\n");
    return NULL;
}

chat_session_t create_new_session(uv_stream_t* stream, uint64_t client_id) {
    fprintf(stderr, "[INFO] Storing session info; future session_id=[%llu]\n", client_id);
    return chat_session_t({
        .client_id = client_id,
        .client_stream = stream,
        .state = CLIENT_STATE::CONNECTED
    });
}

void close_session(chat_session_t* session) {
    uv_handle_t* on_close_handle = (uv_handle_t*)session->client_stream;
    uv_handle_set_data(on_close_handle, (void*)on_close_handle);

    fprintf(stderr, "[INFO] Initilize close connetion; session_id=[%llu]\n", session->client_id);
    call_async_close((uv_handle_t*)session->client_stream);
}


chat_session_t* get_current_session(chat_session_t sessions[], uint64_t client_id) {
    for (size_t i = 0; i < MAX_SESSIONS_COUNT; i ++) {
        if (sessions[i].client_id == client_id) {
            return sessions + i;
        }
    }
    return NULL;
}

chat_session_t* get_current_session_by_name(chat_session_t sessions[], uint64_t client_name) {
    for (size_t i = 0; i < MAX_SESSIONS_COUNT; i ++) {
        if (sessions[i].client_name == client_name) {
            return sessions + i;
        }
    }
    return NULL;
}

bool change_state(chat_session_t* session, CLIENT_STATE new_state) {
    switch (session->state) {
        case CLIENT_STATE::DISCONNECTED:
            if (new_state != CLIENT_STATE::CONNECTED) {
                fprintf(stderr, "[ERROR] Before log in client must be connected first\n");
                return false;
            }
            break;
        case CLIENT_STATE::LOGGED_IN:
            if (new_state != CLIENT_STATE::CONNECTED) {
                fprintf(stderr, "[ERROR] When logged in, client must log out first\n");
                return false;
            }
            break;
        case CLIENT_STATE::CONNECTED:
            break;
        case CLIENT_STATE::UNDEFINED:
            fprintf(stderr, "[ERROR] Failed change state when UNDEFINED\n");
            return false;
    }
    session->state = new_state;
    return true;
}

bool change_state_session(chat_session_t sessions[], uint64_t client_id, CLIENT_STATE new_state) {
    for (size_t i = 0; i < MAX_SESSIONS_COUNT; i ++) {
        if (sessions[i].client_id == client_id &&
            sessions[i].state == new_state) {
                fprintf(stderr, "[WARNING] Changing client state to current state has no effect\n");
                return false;
        }
        if (sessions[i].client_id == client_id) {
            CLIENT_STATE old_state = sessions[i].state;
            bool state_change = change_state(sessions + i, new_state);
            if (!state_change) {
                fprintf(stderr, "[ERROR] Invalid state change, can not change [%d] -> [%d]\n", old_state, new_state);
                return state_change;
            }
            fprintf(stderr, "[INFO] Client state change: [%d] -> [%d]\n", old_state, new_state);
            return state_change;
        }
    }
    return false;
}

CLIENT_STATE get_state_session(chat_session_t sessions[], uint64_t client_id) {
    for (size_t i = 0; i < MAX_SESSIONS_COUNT; i ++) {
        if (sessions[i].client_id == client_id)
            return sessions[i].state;
    }
    fprintf(stderr, "[WARNING] Session not found for client_id=[%llu]\n", client_id);
    return CLIENT_STATE::UNDEFINED; // No session found for passed client_id
}

