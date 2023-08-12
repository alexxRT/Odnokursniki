#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "memory.h"

//interface scratch to work with chat messages

//init_message(msg, type);
// msg->read_message(msg, buffer);
// msg->write_message(msg, buffer);


//buffer format: "msg_typefrom000000to00000000msg_body00000000"
//               |INT_SIZE||NAME_SIZE||NAME_SIZE||   MSG_SIZE   |  <--- parse info

const int MSG_SIZE = 100;

//client constant -----> client.h
const int NAME_SIZE = 10;

enum MSG_TYPE : int {
    SERVER    = 0,
    TXT_MSG   = 1,
    BROADCAST = 2,
    ERROR_MSG = 3
};

enum ERR_STAT : int{
    SUCCESS = 0,
    CONNECTION_LOST = 1,
    INVALID_MSG = 2,
    //....... fill further then .......
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

char* create_text_message_buffer(const char* from, const char* to, const char* msg_body);
char* create_server_message_buffer(const char* from, const char* msg_body);
char* create_error_message_buffer(ERR_STAT err_code, const char* error_msg);
char* create_broadcast_message_buffer(const char* msg_body);

void delete_type_buffer(char* type_buffer);

chat_message_t* create_chat_message (MSG_TYPE type);
void delete_chat_message (chat_message_t* msg);