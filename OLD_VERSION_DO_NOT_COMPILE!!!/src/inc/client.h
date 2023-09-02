#ifndef CLIENT_H
#define CLIENT_H

#include "chat_configs.h"


typedef struct client_ {
    uv_tcp_t* client_stream;
    char name[NAME_SIZE];
    uint64_t  name_hash;
    uint64_t  pswd_hash;
    status status;
} chat_client_t;




#endif