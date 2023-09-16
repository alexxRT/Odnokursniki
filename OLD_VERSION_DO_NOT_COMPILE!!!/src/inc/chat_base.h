#ifndef CHAT_BASE_H
#define CHAT_BASE_H

#include <stdlib.h>
#include "memory.h"
#include "chat_configs.h"
#include <uv.h>

const int BASE_SIZE = 100;

typedef uint64_t (*hash_base_t)(unsigned char* str);

typedef struct base_client_ {
    uv_stream_t* client_stream;
    char name[NAME_SIZE];
    uint64_t  name_hash;
    uint64_t  pswd_hash;
    STATUS status;
} base_client_t;

typedef struct client_base_ {
    size_t  size;
    size_t  max_size;
    base_client_t* base;
    hash_base_t hash_client;
} chat_base_t;


chat_base_t* create_client_base(size_t base_size);
void destoy_client_base(chat_base_t* base);

//reg_buf = "namepassword"
base_client_t* registr_client(chat_base_t* base, uv_stream_t* stream, const char* reg_buf);

//log_in_buf = "namepassword"
base_client_t* log_in_client (chat_base_t* base, const char* log_in_buf);

//log_out_buf = "name"
base_client_t* log_out_client(chat_base_t* base, const char* log_out_buf);

//access functions//
//clients can be found by:
// 1) their name hash
// 2) endpoint address, aka uv_tcp_t* (is created on connection)
int look_up_client(chat_base_t* base, uint64_t name_hash);

// TO DO://
int look_up_client(chat_base_t* base, uv_stream_t* endpoint);
base_client_t* get_client(chat_base_t* base, uint64_t name_hash);
base_client_t* get_client(chat_base_t* base, uv_stream_t* endpoint);
// --------- //

void change_status(base_client_t* client);

#endif // CHAT_BASE_H