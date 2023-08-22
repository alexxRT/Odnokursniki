#ifndef MESSAGES_H
#define MESSAGES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "memory.h"
#include "chat_configs.h"

//interface scratch to work with chat messages

//init_message(msg, type);
// msg->read_message(msg, buffer);
// msg->write_message(msg, buffer);


//buffer format: "msg_typefrom000000to00000000msg_body00000000"
//               |INT_SIZE||NAME_SIZE||NAME_SIZE||   MSG_SIZE   |  <--- parse info

enum MSG_TYPE : int {
    SERVER    = 0,
    TXT_MSG   = 1,
    BROADCAST = 2,
    ERROR_MSG = 3
};

struct message_; //forward declaration to init fill message handler

typedef void (*read_handler_t)(struct message_*,  const char*);
typedef void (*write_handler_t)(struct message_*, char*);

typedef struct message_ {
    MSG_TYPE msg_type_;
    ERR_STAT error_stat;

    char from[NAME_SIZE];
    char to[NAME_SIZE];
    char msg_body[MSG_SIZE];

    write_handler_t write_message;
    read_handler_t  read_message;

} chat_message_t;

typedef struct buffer_ {
    char* buf;
    size_t size;
} buffer_t;

buffer_t* create_text_message_buffer(const char* from, const char* to, const char* msg_body);
buffer_t* create_server_message_buffer(const char* from, const char* msg_body);
buffer_t* create_error_message_buffer(ERR_STAT err_code, const char* error_msg);
buffer_t* create_broadcast_message_buffer(const char* msg_body);

MSG_TYPE get_msg_type (const char* msg_buffer); 

void delete_type_buffer(buffer_t* type_buffer);


chat_message_t* create_chat_message (MSG_TYPE type);
void delete_chat_message (chat_message_t* msg);

#endif