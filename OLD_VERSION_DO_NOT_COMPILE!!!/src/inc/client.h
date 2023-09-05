#ifndef CLIENT_H
#define CLIENT_H

#include "chat_configs.h"
#include <uv.h>


typedef struct client_ {
    uv_stream_t* client_stream;
    char name[NAME_SIZE];
    uint64_t  name_hash;
    uint64_t  pswd_hash;
    STATUS status;
} chat_client_t;




#endif