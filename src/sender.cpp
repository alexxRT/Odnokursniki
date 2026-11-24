#include "sender.h"
#include "messages.h"
#include "memory.h"
#include "networking.h"

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
        bool client_online = client_session->state == CLIENT_STATE::LOGGED_IN;
        if (client_online) send_message(server, client_session, msg);
    }
    return ERR_STAT::SUCCESS;
}


ERR_STAT send_message_register(client_t* client, const char username[NAME_SIZE],
                                                 const char password[PSWD_SIZE])
{
    static const char server_name[NAME_SIZE] = {0};
    buffer_t* buffer = create_type_buffer(MSG_TYPE::SYSTEM);

    fill_body(buffer, (void*)server_name,  NAME_SIZE);
    fill_body(buffer, (void*)username, NAME_SIZE);

    COMMAND command = COMMAND::REGISTR;
    fill_body(buffer, (void*)&command, sizeof(COMMAND));

    fill_body(buffer, (void*)username, NAME_SIZE);
    fill_body(buffer, (void*)password, PSWD_SIZE);

    chat_message_t msg = create_chat_message(MSG_TYPE::SYSTEM);
    msg.read_message(&msg, buffer);

    destroy_type_buffer(buffer);
    return send_message(client, &msg);
}

ERR_STAT send_message_text(client_t* client, const char addressant[NAME_SIZE],
                                             const char message_txt[MSG_SIZE])
{
    buffer_t* buffer = create_type_buffer(MSG_TYPE::TXT_MSG);
    fill_body(buffer, (void*)addressant, NAME_SIZE);
    fill_body(buffer, (void*)client->username, NAME_SIZE);

    size_t message_len = strlen(message_txt);
    message_len = message_len > MSG_SIZE ? MSG_SIZE : message_len;
    fill_body(buffer, (void*)message_txt, message_len);

    chat_message_t msg = create_chat_message(MSG_TYPE::TXT_MSG);
    msg.read_message(&msg, buffer);
    destroy_type_buffer(buffer);
    return send_message(client, &msg);
}

ERR_STAT send_message_login(client_t* client, const char username[NAME_SIZE],
                                              const char password[PSWD_SIZE])
{
    static const char server_name[NAME_SIZE] = {0};
    buffer_t* buffer = create_type_buffer(MSG_TYPE::SYSTEM);

    fill_body(buffer, (void*)server_name,  NAME_SIZE);
    fill_body(buffer, (void*)username, NAME_SIZE);

    COMMAND command = COMMAND::LOG_IN;
    fill_body(buffer, (void*)&command, sizeof(COMMAND));

    fill_body(buffer, (void*)username, NAME_SIZE);
    fill_body(buffer, (void*)password, PSWD_SIZE);

    chat_message_t msg = create_chat_message(MSG_TYPE::SYSTEM);
    msg.read_message(&msg, buffer);

    destroy_type_buffer(buffer);
    return send_message(client, &msg);
}

ERR_STAT send_message(client_t* client, chat_message_t* msg) {
    buffer_t* buf = create_type_buffer(msg->msg_type);
    msg->write_message(msg, buf);

    pthread_mutex_lock(&sessions_mutex);
    bool connected = client->session.client_stream != NULL;
    if (connected)
        call_async_write(client->session.client_stream, buf);
    pthread_mutex_unlock(&sessions_mutex);
    destroy_type_buffer(buf);

    return connected ? ERR_STAT::SUCCESS : ERR_STAT::CONNECTION_LOST;
}
