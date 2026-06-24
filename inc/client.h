#ifndef CLIENT_H
#define CLIENT_H

#include <uv.h>
#include "chat_configs.h"
#include "chat_session.h"
#include "ts_list.h"

typedef struct client_ {
    STATUS       status;
    uint64_t     name_hash;
    uv_stream_t* server_dest;
    uv_loop_t*   event_loop;
    ThreadSafeList<chat_message_t>* incoming_msg;
    chat_session_t session;
} client_t;


client_t* create_client();
void      destroy_client(client_t* client);

void run_client_backend(client_t* client, size_t port, const char* ip);



#endif // CLIENT_H