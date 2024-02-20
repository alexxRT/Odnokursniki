#ifndef MESSAGES_H
#define MESSAGES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "memory.h"
#include "chat_configs.h"
#include <uv.h>

//interface scratch to work with chat messages

//init_message(msg, type);
// msg->read_message(msg, buffer);
// msg->write_message(msg, buffer);


//buffer format: "msg_typefrom000000to00000000msg_body00000000"
//               |INT_SIZE||NAME_SIZE||NAME_SIZE||   MSG_SIZE   |  <--- parse info



enum class MSG_TYPE : int64_t {
    SYSTEM    = 0,
    TXT_MSG   = 1,
    BROADCAST = 2,
    ERROR_MSG = 3
};

typedef struct buffer_ {
    char* buf;
    MSG_TYPE type;
    size_t size;
    size_t capacity;
} buffer_t;

struct message_; //forward declaration to init fill message handler

typedef int (*read_handler_t)(struct message_*,  buffer_t*);
typedef int (*write_handler_t)(struct message_*, buffer_t*);

typedef struct message_ {
    MSG_TYPE msg_type;
    ERR_STAT error_stat;
    COMMAND  sys_command;

    char from    [NAME_SIZE];
    char to      [NAME_SIZE];
    char msg_body[MSG_SIZE];

    write_handler_t write_message;
    read_handler_t  read_message;

    uv_stream_t* client_endpoint = NULL;
} chat_message_t;


buffer_t* create_type_buffer(MSG_TYPE type);
void fill_body(buffer_t* buffer, void* source, size_t bytes_size);
void print_buffer(buffer_t* buffer, size_t position);
void destroy_type_buffer(buffer_t* type_buffer);

chat_message_t create_chat_message (MSG_TYPE type);
void print_msg_body(chat_message_t* msg);
MSG_TYPE get_msg_type (const char* msg_buffer); 

#endif