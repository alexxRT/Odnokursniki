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

int read_system_msg   (chat_message_t* msg, buffer_t* buffer);
int read_text_msg     (chat_message_t* msg, buffer_t* buffer);
int read_error_msg    (chat_message_t* msg, buffer_t* buffer);
int read_broadcast_msg(chat_message_t* msg, buffer_t* buffer);

int write_system_msg   (chat_message_t* msg, buffer_t* buffer);
int write_text_msg     (chat_message_t* msg, buffer_t* buffer);
int write_error_msg    (chat_message_t* msg, buffer_t* buffer);
int write_broadcast_msg(chat_message_t* msg, buffer_t* buffer);

const int RW_HANDLERS_NUM = 4;

const read_handler_t read_handlers[] = {
    read_system_msg,
    read_text_msg,
    read_broadcast_msg,
    read_error_msg
};

const write_handler_t write_handlers[] = {
    write_system_msg,
    write_text_msg,
    write_broadcast_msg,
    write_error_msg
};

int read_system_msg(chat_message_t* msg, buffer_t* buffer) { 
    ASSERT(buffer);
    assert(msg);
    assert(msg->msg_type == buffer->type && "msg and buffer different types");

    // we start parse from msg_type offset
    size_t position = sizeof(MSG_TYPE);

    // fprintf(stderr, "buffer size: %lu\n", buffer->size);
    // fprintf(stderr, "buffer capacity: %lu\n", buffer->capacity);
    // fprintf(stderr, "position: %lu\n", position);

    // print_buffer(buffer, 0);

    memcpy(msg->to, buffer->buf + position, NAME_SIZE);
    position += NAME_SIZE;

    memcpy(msg->from, buffer->buf + position, NAME_SIZE);
    position += NAME_SIZE;

    memcpy((char*)&msg->sys_command, buffer->buf + position, sizeof(COMMAND));
    position += sizeof(COMMAND);

    // print_buffer(buffer, position);
    memcpy(msg->msg_body, buffer->buf + position, MSG_SIZE);
    position += MSG_SIZE;

    // fprintf(stderr, "buffer size: %lu\n", buffer->size);
    // fprintf(stderr, "buffer capacity: %lu\n", buffer->capacity);
    // fprintf(stderr, "position: %lu\n", position);

    assert(position == buffer->capacity && "msg read overlaping");
    ASSERT(buffer);

    return position; 
};

int read_text_msg(chat_message_t* msg, buffer_t* buffer) { 
    ASSERT(buffer);
    assert(msg);
    assert(msg->msg_type == buffer->type && "msg and buffer different types");

    size_t position = sizeof(MSG_TYPE);

    // fprintf(stderr, "buffer size: %lu\n", buffer->size);
    // fprintf(stderr, "buffer capacity: %lu\n", buffer->capacity);
    // fprintf(stderr, "position: %lu\n", position);

    memcpy(msg->to, buffer->buf + position, NAME_SIZE);
    position += NAME_SIZE;

    memcpy(msg->from, buffer->buf + position, NAME_SIZE);
    position += NAME_SIZE;

    memcpy(msg->msg_body, buffer->buf + position, MSG_SIZE);
    position += MSG_SIZE;

    // fprintf(stderr, "buffer size: %lu\n", buffer->size);
    // fprintf(stderr, "buffer capacity: %lu\n", buffer->capacity);
    // fprintf(stderr, "position: %lu\n", position);

    assert(position == buffer->capacity && "msg read overlaping");
    ASSERT(buffer);

    return position;
};

int read_error_msg(chat_message_t* msg, buffer_t* buffer) {
    ASSERT(buffer);
    assert(msg);
    assert(msg->msg_type == buffer->type && "msg and buffer different types");

    size_t position = sizeof(MSG_TYPE);
    
    memcpy(msg->to, buffer->buf + position, NAME_SIZE);
    position += NAME_SIZE;

    memcpy(msg->from, buffer->buf + position, NAME_SIZE);
    position += NAME_SIZE;

    memcpy ((char*)&(msg->error_stat), buffer->buf + position, sizeof(ERR_STAT));
    position += sizeof(ERR_STAT);

    memcpy(msg->msg_body, buffer->buf + position, MSG_SIZE);
    position += MSG_SIZE;

    assert(position == buffer->capacity && "msg read overlaping");
    ASSERT(buffer);

    return position; 
};

int read_broadcast_msg(chat_message_t* msg, buffer_t* buffer) { 
    ASSERT(buffer);
    assert(msg);
    assert(msg->msg_type == buffer->type && "msg and buffer different types");

    size_t position = sizeof(MSG_TYPE);

    memcpy(msg->to, buffer->buf + position, NAME_SIZE);
    position += NAME_SIZE;

    memcpy(msg->from, buffer->buf + position, NAME_SIZE);
    position += NAME_SIZE;

    memcpy(msg->msg_body, buffer->buf + position, MSG_SIZE);
    position += MSG_SIZE;

    assert(position == buffer->capacity && "msg read overlaping");
    ASSERT(buffer);

    return position; 
};

int write_system_msg(chat_message_t* msg, buffer_t* buffer) { 
    ASSERT(buffer);
    assert(msg);
    assert(msg->msg_type == buffer->type && "msg and buffer different types");
    
    fill_body(buffer, msg->to, NAME_SIZE);

    fill_body(buffer, msg->from, NAME_SIZE);

    fill_body(buffer, (void*)&msg->sys_command, sizeof(COMMAND));

    fill_body(buffer, msg->msg_body, MSG_SIZE);

    ASSERT(buffer);

    return buffer->size; 
};

int write_text_msg(chat_message_t* msg, buffer_t* buffer) { 
    ASSERT(buffer);
    assert(msg);
    assert(msg->msg_type == buffer->type && "msg and buffer different types");
    
    fill_body(buffer, msg->to, NAME_SIZE);

    fill_body(buffer, msg->from, NAME_SIZE);

    fill_body(buffer, msg->msg_body, MSG_SIZE);

    ASSERT(buffer);

    return buffer->size; 
};


int write_broadcast_msg(chat_message_t* msg, buffer_t* buffer) { 
    ASSERT(buffer);
    assert(msg);
    assert(msg->msg_type == buffer->type && "msg and buffer different types");
    
    fill_body(buffer, msg->to, NAME_SIZE);

    fill_body(buffer, msg->from, NAME_SIZE);

    fill_body(buffer, msg->msg_body, MSG_SIZE);

    ASSERT(buffer);

    return buffer->size;
};


int write_error_msg(chat_message_t* msg, buffer_t* buffer) {
    ASSERT(buffer);
    assert(msg);
    assert(msg->msg_type == buffer->type && "msg and buffer different types");
    
    fill_body(buffer, msg->to, NAME_SIZE);

    fill_body(buffer, msg->from, NAME_SIZE);

    fill_body(buffer, (void*)&msg->error_stat, sizeof(ERR_STAT));

    fill_body(buffer, msg->msg_body, MSG_SIZE);

    ASSERT(buffer);

    return buffer->size;
};


chat_message_t create_chat_message (MSG_TYPE type) {
    chat_message_t msg = {};
    int indx = static_cast<int>(type);
    assert(indx >= 0 && "Invalid msg type on creation");
    assert(indx < RW_HANDLERS_NUM && "Handlers index overflow");

    msg.msg_type = type;
    msg.read_message  = read_handlers[indx];
    msg.write_message = write_handlers[indx];
    
    return msg;
};

buffer_t* create_type_buffer(MSG_TYPE type) {
    buffer_t* buffer = CALLOC(1, buffer_t);
    buffer->size = 0;
    buffer->type = type;

    if (type == MSG_TYPE::SYSTEM) {
        size_t buffer_capacity = MSG_SIZE + 2*NAME_SIZE + sizeof(COMMAND) + sizeof(MSG_TYPE);
        buffer->capacity = buffer_capacity;

        char* buf = CALLOC(buffer_capacity, char);
        buffer->buf  = buf;

        fill_body(buffer, (void*)&type, sizeof(MSG_TYPE));

        return buffer;
    }

    if (type == MSG_TYPE::TXT_MSG) {
        size_t buffer_capacity = MSG_SIZE + 2*NAME_SIZE + sizeof(MSG_TYPE);
        buffer->capacity = buffer_capacity;

        char* buf = CALLOC(buffer_capacity, char);
        buffer->buf  = buf;
    
        fill_body(buffer, (void*)&type, sizeof(MSG_TYPE));

        return buffer;
    }
    else if (type == MSG_TYPE::BROADCAST) {
        size_t buffer_capacity = MSG_SIZE + 2*NAME_SIZE + sizeof(MSG_TYPE);
        buffer->capacity = buffer_capacity;

        char* buf = CALLOC(buffer_capacity, char);
        buffer->buf  = buf;

        fill_body(buffer, (void*)&type, sizeof(MSG_TYPE));

        return buffer;

    }
    else if (type == MSG_TYPE::ERROR_MSG) {
        size_t buffer_capacity = MSG_SIZE + 2*NAME_SIZE + sizeof(ERR_STAT) + sizeof(MSG_TYPE);
        buffer->capacity = buffer_capacity;

        char* buf = CALLOC(buffer_capacity, char);
        buffer->buf  = buf;

        fill_body(buffer, (void*)&type, sizeof(MSG_TYPE));

        return buffer;
    }

    return NULL; 
};


void destroy_type_buffer(buffer_t* type_buffer) {
    FREE(type_buffer->buf);
    FREE(type_buffer);
};

void fill_body(buffer_t* buffer, void* source, size_t bytes_size) {
    ASSERT(buffer);

    char* source_to_copy = (char*)source;
    memcpy(buffer->buf + buffer->size, source_to_copy, bytes_size);
    buffer->size += bytes_size;

    ASSERT(buffer);
}


MSG_TYPE get_msg_type (const char* msg_buffer) {
    assert(msg_buffer);

    MSG_TYPE msg_type;

    memcpy((char*)&msg_type, msg_buffer, sizeof(MSG_TYPE));

    return msg_type;
};

void print_buffer(buffer_t* buffer, size_t position) {
    assert(position < buffer->size);

    for (size_t i = position; i < buffer->size; i ++) {
        if (buffer->buf[i] == '\0')
            fprintf (stderr, "_");

        fprintf(stderr, "%c", buffer->buf[i]);
    }
    fprintf(stderr, "\n");
}

void print_msg_body(chat_message_t* msg) {
    for (size_t i = 0; i < MSG_SIZE; i ++) {
        if (msg->msg_body[i] == '\0')
            fprintf (stderr, "_");

        fprintf(stderr, "%c", msg->msg_body[i]);
    }
    fprintf(stderr, "\n");
}