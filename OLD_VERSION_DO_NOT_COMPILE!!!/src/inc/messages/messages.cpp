#include "messages.h"
#include "memory.h"

size_t min (size_t num_1, size_t num_2) {
    return num_1 <= num_2 ? num_1 : num_2;
}

#ifdef DEBUG_VERSION
#define ASSERT(buffer)                        \
do {                                          \
    assert(buffer);                           \
    assert(buffer->buf);                      \
    assert(buffer->size <= buffer->capacity); \
    assert(buffer->size >= 0);                \
                                              \
} while(0)                                    \

#else 
#define ASSERT(buffer) (void*)0

#endif

int read_server_msg   (chat_message_t* msg, const char* buffer);
int read_text_msg     (chat_message_t* msg, const char* buffer);
int read_error_msg    (chat_message_t* msg, const char* buffer);
int read_broadcast_msg(chat_message_t* msg, const char* buffer);

int write_server_msg   (chat_message_t* msg, char* buffer);
int write_text_msg     (chat_message_t* msg, char* buffer);
int write_error_msg    (chat_message_t* msg, char* buffer);
int write_broadcast_msg(chat_message_t* msg, char* buffer);

const int RW_HANDLERS_NUM = 4;

const read_handler_t read_handlers[] = {
    read_server_msg,
    read_text_msg,
    read_error_msg,
    read_broadcast_msg
};

const write_handler_t write_handlers[] = {
    write_server_msg,
    write_text_msg,
    write_error_msg,
    write_broadcast_msg
};


int read_server_msg(chat_message_t* msg, const char* buffer) { 
    assert(buffer);

    int read_size = NAME_SIZE + MSG_SIZE;

    strncpy((char*)&msg->from, buffer, NAME_SIZE);
    strncpy(msg->msg_body, buffer + NAME_SIZE, MSG_SIZE);

    return read_size; 
};

int read_text_msg(chat_message_t* msg, const char* buffer) { 
    assert(buffer);

    int read_size = 2*NAME_SIZE + MSG_SIZE;

    strncpy((char*)&msg->from, buffer, NAME_SIZE);
    strncpy((char*)&msg->to,   buffer + NAME_SIZE, NAME_SIZE);
    strncpy(msg->msg_body, buffer + 2*NAME_SIZE, MSG_SIZE);

    return read_size;
};

int read_error_msg(chat_message_t* msg, const char* buffer) {
    assert(buffer);

    int read_size = sizeof(ERR_STAT) + MSG_SIZE;

    strncpy ((char*)&(msg->error_stat), buffer, sizeof(ERR_STAT));
    strncpy(msg->msg_body, buffer, MSG_SIZE);

    return read_size; 
};

int read_broadcast_msg(chat_message_t* msg, const char* buffer) { 
    assert(buffer);

    int read_size = MSG_SIZE;

    strncpy(msg->msg_body, buffer, MSG_SIZE);

    return read_size; 
};


int write_server_msg(chat_message_t* msg, char* buffer) { 
    assert(buffer);

    int write_size = NAME_SIZE + MSG_SIZE;

    strncpy(buffer, (char*)&msg->from, NAME_SIZE);
    strncpy(buffer + NAME_SIZE, msg->msg_body, MSG_SIZE);

    return write_size; 
};

int write_text_msg(chat_message_t* msg, char* buffer) { 
    assert(buffer);

    int write_size = 2*NAME_SIZE + MSG_SIZE;

    strncpy(buffer, (char*)&msg->from, NAME_SIZE);
    strncpy(buffer + NAME_SIZE, (char*)&msg->to, NAME_SIZE);
    strncpy(buffer + 2*NAME_SIZE, msg->msg_body, MSG_SIZE);

    return write_size;
};


int write_broadcast_msg(chat_message_t* msg, char* buffer) { 
    assert(buffer);

    int write_size = MSG_SIZE;

    strncpy(buffer, msg->msg_body, MSG_SIZE);

    return MSG_SIZE; 
};


int write_error_msg(chat_message_t* msg, char* buffer) {
    assert(buffer);

    int write_size = sizeof(ERR_STAT) + MSG_SIZE;

    strncpy (buffer, (char*)&(msg->error_stat), sizeof(ERR_STAT));
    strncpy (buffer, msg->msg_body, MSG_SIZE);

    return write_size;
};


chat_message_t* create_chat_message (MSG_TYPE type) {
    chat_message_t* msg = CALLOC(1, chat_message_t);
    int handle_indx = static_cast<int>(type);
    assert(handle_indx >= 0 && "Invalid msg type on creation");

    msg->msg_type = type;
    if (handle_indx < RW_HANDLERS_NUM) {
        msg->read_message  = read_handlers[static_cast<int>(type)];
        msg->write_message = write_handlers[static_cast<int>(type)];
    }
    //othrewise emty "header-kind" message

    return msg;
};

void destroy_chat_message (chat_message_t* msg) {
    bzero((void*)msg, sizeof(chat_message_t));
    FREE(msg);

    return;
};

buffer_t* create_buffer(MSG_TYPE type) {
    if (type == MSG_TYPE::SYSTEM) {
        char* buf = CALLOC(MSG_SIZE + NAME_SIZE + sizeof(MSG_TYPE), char);
        size_t buffer_size = MSG_SIZE + NAME_SIZE + sizeof(MSG_TYPE);

        buffer_t* buffer = CALLOC(1, buffer_t);
        buffer->buf  = buf;
        buffer->size = buffer_size;

        return buffer;
    }
    else if (type == MSG_TYPE::TXT_MSG) {
        char* buf = CALLOC(2*NAME_SIZE + MSG_SIZE + sizeof(MSG_TYPE), char);
        size_t buffer_size = MSG_SIZE + 2*NAME_SIZE + sizeof(MSG_TYPE);

        buffer_t* buffer = CALLOC(1, buffer_t);
        buffer->buf  = buf;
        buffer->size = buffer_size;

        return buffer;
    }
    else if (type == MSG_TYPE::BROADCAST) {
        char* buf = CALLOC(MSG_SIZE + sizeof(MSG_TYPE), char);
        size_t buffer_size = MSG_SIZE + sizeof(MSG_TYPE);

        buffer_t* buffer = CALLOC(1, buffer_t);
        buffer->buf  = buf;
        buffer->size = buffer_size;

        return buffer;

    }
    else if (type == MSG_TYPE::ERROR_MSG) {
        char* buf = CALLOC(sizeof(ERR_STAT) + MSG_SIZE + sizeof(MSG_TYPE), char);
        size_t buffer_size = MSG_SIZE + sizeof(ERR_STAT) + sizeof(MSG_TYPE);

        buffer_t* buffer = CALLOC(1, buffer_t);
        buffer->buf  = buf;
        buffer->size = buffer_size;

        return buffer;
    }
    else 
        return NULL;
};


void destroy_type_buffer(buffer_t* type_buffer) {
    FREE(type_buffer->buf);
    FREE(type_buffer);
};

void fill_body(buffer_t* buffer, void* source, size_t bytes_size) {
    ASSERT(buffer);

    char* source_to_copy = (char*)source;
    strncpy(buffer->buf + buffer->size, source_to_copy, bytes_size);
    buffer->size += bytes_size;

    ASSERT(buffer);
}

void fill_text_message_buffer(buffer_t* buffer, uint64_t from, uint64_t to, const char* msg_body) {
    assert(msg_body);
    ASSERT(buffer);

    MSG_TYPE type = MSG_TYPE::TXT_MSG;
    size_t msg_size = 0;

    fill_body(buffer, (void*)&type, sizeof(MSG_TYPE));
    fill_body(buffer, (void*)&from, NAME_SIZE);
    fill_body(buffer, (void*)&to,   NAME_SIZE);

    size_t msg_body_size = min(strlen(msg_body), MSG_SIZE);
    fill_body(buffer, (void*)msg_body, msg_body_size);

    ASSERT(buffer);

    return;
};

void fill_server_message_buffer(buffer_t* buffer, uint64_t from, CMD_CODE code) {
    ASSERT(buffer);

    MSG_TYPE type = MSG_TYPE::SYSTEM;
    
    fill_body(buffer, (void*)&type, sizeof(MSG_SIZE));
    fill_body(buffer, (void*)&from, sizeof(NAME_SIZE));

    int code_int = static_cast<int>(code);

    if (code_int >= 0) {
        size_t msg_body_size = min(strlen(commands[code_int]), MSG_SIZE);
        fill_body(buffer, (void*)commands[code_int], msg_body_size);
    }
    else 
        fprintf(stderr, "BAD CODE in %s, CODE: %d\n", __func__, code);

    ASSERT(buffer);

    return;
};


void fill_error_message_buffer(buffer_t* buffer, ERR_STAT err_code, const char* error_msg) {
    assert(error_msg);
    ASSERT(buffer);

    MSG_TYPE type = MSG_TYPE::ERROR_MSG;

    fill_body(buffer, (void*)&type, sizeof(MSG_SIZE));
    fill_body(buffer, (void*)&err_code, sizeof(ERR_STAT));

    size_t msg_body_size = min(strlen(error_msg), MSG_SIZE);
    fill_body(buffer, (void*)error_msg, msg_body_size);

    ASSERT(buffer);
    return;
};

void fill_broadcast_message_buffer(buffer_t* buffer, const char* msg_body) {
    assert(msg_body);
    ASSERT(buffer);

    MSG_TYPE type = MSG_TYPE::BROADCAST;

    fill_body(buffer, (void*)&type, sizeof(MSG_TYPE));

    size_t msg_body_size = min(strlen(msg_body), MSG_SIZE);
    fill_body(buffer, (void*)msg_body, msg_body_size);

    ASSERT(buffer);

    return;
};

MSG_TYPE get_msg_type (const char* msg_buffer) {
    assert(msg_buffer);

    MSG_TYPE msg_type;

    strncpy((char*)&msg_type, msg_buffer, sizeof(MSG_TYPE));

    return msg_type;
};
