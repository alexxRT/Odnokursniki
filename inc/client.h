#ifndef CLIENT_H
#define CLIENT_H

#include <uv.h>
#include "chat_configs.h"
#include "chat_session.h"
#include "ts_list.h"

typedef struct client_ {
    uv_sem_t backend_ready;
    thread_args_t backend_args;
    STATUS       status;
    uv_loop_t*   event_loop;
    ThreadSafeList<chat_message_t>* incoming_msg;
    chat_session_t session;
    char username[NAME_SIZE];
} client_t;


client_t* create_client();
void      set_client_username(client_t* client, const char* username);
void      destroy_client(client_t* client);

pthread_t run_client_backend(client_t* client, size_t port, const char* ip);



#endif // CLIENT_H