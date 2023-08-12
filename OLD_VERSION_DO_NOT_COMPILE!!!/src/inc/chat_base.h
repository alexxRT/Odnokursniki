#include <stdlib.h>
#include "memory.h"
#include <uv.h>

const int PSWD_SIZE = 10;
const int NAME_SIZE = 10;

const int BASE_SIZE = 100;

enum client_status {
    ONLINE = 1,
    OFFLINE = 0
};

typedef struct client_ {
    uv_tcp_t* client_stream ;
    uint64_t  name_hash;
    uint64_t  pswd_hash;
    client_status status;
} chat_client_t;

typedef uint64_t (*hash_base_t)(unsigned char* str);

typedef struct client_base_ {
    size_t  size;
    size_t  max_size;
    chat_client_t* base;
    hash_base_t hash_client;
} chat_base_t;


chat_base_t* create_client_base(size_t base_size);
void destoy_client_base(chat_base_t* base);

//log_out_buf = "namepassword"
bool add_new_client(chat_base_t* base, uv_tcp_t* stream, const char* log_in_buf);

//log_out_buf = "namepassword"
bool log_in_client (chat_base_t* base, uv_tcp_t* stream, const char* log_in_buf);

//log_out_buf = "name"
bool log_out_client(chat_base_t* base, const char* log_out_buf);