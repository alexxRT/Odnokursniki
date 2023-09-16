#ifndef CLIENT_H
#define CLIENT_H

#include <uv.h>
#include "chat_configs.h"
#include "list.h"
#include "threads_safe.h"


typedef struct client_ {
    ALIVE_STAT   alive_stat;
    STATUS       status;
    uint64_t     name_hash;
    uv_stream_t* server_dest;
    uv_loop_t*   event_loop;

    lock*  sem_lock;
    list_* incoming_msg;
    list_* outgoing_msg;

} client_t;


client_t* create_client();
void      destroy_client(client_t* client);

void run_client_backend(client_t* client, size_t port, const char* ip);



#endif // CLIENT_H