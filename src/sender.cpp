#include "threads_safe.h"
#include "sender.h"
#include "messages.h"
#include "memory.h"
#include "list.h"
#include "list_debug.h"

enum class SENDER_STATUS : int {
    ACTIVE,
    SHUTDOWN
};

SENDER_STATUS status = SENDER_STATUS::SHUTDOWN;

ERR_STAT send_message(server_t* server, chat_session_t* session, chat_message_t* msg) {
    if (session->state != CLIENT_STATE::LOGGED_IN &&
        session->state != CLIENT_STATE::CONNECTED)
    {
        fprintf(stderr, "[ERROR] Trying to send message, addressant is not online; session_id=[%llu]\n",
            session->client_id);
        return ERR_STAT::CLIENT_OFFLINE;
    }

    fprintf(stderr, "[INFO] Sending message to client; session_id=[%llu]\n", session->client_id);
    buffer_t* buf = create_type_buffer(msg->msg_type);
    msg->write_message(msg, buf);

    call_async_write(session->client_stream, buf);
    destroy_type_buffer(buf);

    fprintf(stderr, "[INFO] Message sent to client; session_id=[%llu]\n", session->client_id);
    return ERR_STAT::SUCCESS;
}

ERR_STAT broadcast_message(server_t* server, chat_message_t* msg) {
    for (size_t i = 0; i < MAX_SESSIONS_COUNT; i ++) {
        chat_session_t* client_session = server->active_sessions + i;
        fprintf(stderr, "[INFO] session_id=[%llu], state=[%u]\n", client_session->client_id, client_session->state);
        bool client_online = client_session->state == CLIENT_STATE::CONNECTED ||
                             client_session->state == CLIENT_STATE::LOGGED_IN;
        if (client_online) send_message(server, client_session, msg);
    }
    return ERR_STAT::SUCCESS;
}


ERR_STAT send_message(client_t* client, chat_message_t* msg) {
    buffer_t* buf = create_type_buffer(msg->msg_type);
    msg->write_message(msg, buf);

    call_async_write(client->server_dest, buf);
    destroy_type_buffer(buf);

    return ERR_STAT::SUCCESS;
}
