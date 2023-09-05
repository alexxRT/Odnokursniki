#include "messages.h"

size_t min (size_t num_1, size_t num_2) {
    return num_1 <= num_2 ? num_1 : num_2;
}

int read_server_msg   (chat_message_t* msg, const char* buffer);
int read_text_msg     (chat_message_t* msg, const char* buffer);
int read_error_msg    (chat_message_t* msg, const char* buffer);
int read_broadcast_msg(chat_message_t* msg, const char* buffer);

int write_server_msg   (chat_message_t* msg, char* buffer);
int write_text_msg     (chat_message_t* msg, char* buffer);
int write_error_msg    (chat_message_t* msg, char* buffer);
int write_broadcast_msg(chat_message_t* msg, char* buffer);

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
    chat_message_t* msg = (chat_message_t*)calloc(1, sizeof(chat_message_t));

    msg->msg_type = type;
    msg->read_message = read_handlers[type];
    msg->write_message = write_handlers[type];

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
    else if (type == TXT_MSG) {
        char* buf = CALLOC(2*NAME_SIZE + MSG_SIZE + sizeof(MSG_TYPE), char);
        size_t buffer_size = MSG_SIZE + 2*NAME_SIZE + sizeof(MSG_TYPE);

        buffer_t* buffer = CALLOC(1, buffer_t);
        buffer->buf  = buf;
        buffer->size = buffer_size;

        return buffer;
    }
    else if (type == BROADCAST) {
        char* buf = CALLOC(MSG_SIZE + sizeof(MSG_TYPE), char);
        size_t buffer_size = MSG_SIZE + sizeof(MSG_TYPE);

        buffer_t* buffer = CALLOC(1, buffer_t);
        buffer->buf  = buf;
        buffer->size = buffer_size;

        return buffer;

    }
    else if (type == ERROR_MSG) {
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

buffer_t* create_text_message_buffer(uint64_t from, uint64_t to, const char* msg_body) {
    assert(msg_body);

    buffer_t* buffer = create_buffer(TXT_MSG);
    int type = TXT_MSG;

    strncpy(buffer->buf, (char*)&type, sizeof(MSG_TYPE));

    strncpy (buffer->buf + sizeof(MSG_TYPE), (char*)&from, NAME_SIZE);

    strncpy(buffer->buf + NAME_SIZE + sizeof(MSG_TYPE), (char*)&to, NAME_SIZE);

    size_t msg_body_size = min(strlen(msg_body), MSG_SIZE);
    strncpy(buffer->buf + 2*NAME_SIZE + sizeof(MSG_TYPE), msg_body, msg_body_size);

    return buffer;
};


//refactor//
buffer_t* create_server_message_buffer(uint64_t from, CMD_CODE code) {
    buffer_t* buffer = create_buffer(MSG_TYPE::SYSTEM);
    MSG_TYPE type = MSG_TYPE::SYSTEM;

    strncpy(buffer->buf, (char*)&type, sizeof(MSG_TYPE));
    strncpy (buffer->buf + sizeof(MSG_TYPE), (char*)&from, sizeof(from));

    int code_int = static_cast<int>(code);

    if (code_int >= 0) {
        size_t msg_body_size = min(strlen(commands[code_int]), MSG_SIZE);
        strncpy(buffer->buf + NAME_SIZE + sizeof(MSG_TYPE), commands[code_int], msg_body_size);
    }
    else 
        fprintf(stderr, "BAD CODE in %s, CODE: %d\n", __func__, code);

    return buffer;
};


buffer_t* create_error_message_buffer(ERR_STAT err_code, const char* error_msg) {
    assert(error_msg);

    buffer_t* buffer = create_buffer(ERROR_MSG);
    int type = ERROR_MSG;

    strncpy(buffer->buf, (char*)&type, sizeof(MSG_TYPE));

    strncpy(buffer->buf + sizeof(MSG_TYPE), (char*)&err_code, sizeof(ERR_STAT));

    size_t msg_body_size = min(strlen(error_msg), MSG_SIZE);
    strncpy(buffer->buf + sizeof(MSG_TYPE) + sizeof(ERR_STAT), error_msg, msg_body_size);

    return buffer;
};

buffer_t* create_broadcast_message_buffer(const char* msg_body) {
    assert(msg_body);

    buffer_t* buffer = create_buffer(BROADCAST);
    int type = BROADCAST;

    strncpy(buffer->buf, (char*)&type, sizeof(MSG_TYPE));

    size_t msg_body_size = min(strlen(msg_body), MSG_SIZE);
    strncpy(buffer->buf + sizeof(MSG_TYPE), msg_body, msg_body_size);

    return buffer;
};

MSG_TYPE get_msg_type (const char* msg_buffer) {
    assert(msg_buffer);

    MSG_TYPE msg_type;

    strncpy((char*)&msg_type, msg_buffer, sizeof(MSG_TYPE));

    return msg_type;
};