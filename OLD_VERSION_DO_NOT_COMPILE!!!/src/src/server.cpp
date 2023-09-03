#include "chat_base.h"
#include "messages.h"
#include "server.h"
#include "chat_configs.h"

const int THREAD_NUM = 4;
const int SEM_NUM = 4;

const int SERVER_LIVE = 1;
const int SERVER_DEAD = 0;

chat_server* server = NULL;

typedef struct args__{
    const char* ip;
    size_t port;

}thread_args;

//SEND MESSAGE
//1) creat char buffer for body
//2) fill body
//3) create appropriate buffer_t
//4) send

//RECIEVE MESSAGE
//1) read type
//2) create appropriate buffer_t buf
//3) read bytes from socket into buf
//4) fill msg from buf


//system commands describtion. Client <--> Server interaction

//command: in$namepassword
//command: reg$namepassword
//command: online$
//command: out$

// 0 -- incoming sem index
// 1 -- outgoing sem index
// 2 -- incoming try read index
// 3 -- outgoing try read index


//------------------------------------------THREADS SYNCHRO----------------------------------------//

#define LOCK_INCOMING()                                        \
do {                                                           \
    server->lock->sem_array->sem_op  = -1;                     \
    server->lock->sem_array->sem_flg = 0;                      \
                                                               \
    semop(0, server->lock->sem_array, server->lock->semid);    \
                                                               \
} while(0)                                                     \

#define LOCK_OUTGOING()                                        \
do {                                                           \
    server->lock->sem_array->sem_op  = -1;                     \
    server->lock->sem_array->sem_flg = 0;                      \
                                                               \
    semop(1, server->lock->sem_array, server->lock->semid);    \
                                                               \
} while(0)                                                     \

#define UNLOCK_INCOMING()                                      \
do {                                                           \
    server->lock->sem_array->sem_op  = 1;                      \
    server->lock->sem_array->sem_flg = 0;                      \
                                                               \
    semop(0, server->lock->sem_array, server->lock->semid);    \
                                                               \
}while(0)                                                      \

#define UNLOCK_OUTGOING()                                      \
do {                                                           \
    server->lock->sem_array->sem_op  = 1;                      \
    server->lock->sem_array->sem_flg = 0;                      \
                                                               \
    semop(1, server->lock->sem_array, server->lock->semid);    \
                                                               \
}while(0)                                                      \

#define TRY_READ_INCOMING()                                    \
do{                                                            \
    server->lock->sem_array->sem_op  = -1;                     \
    server->lock->sem_array->sem_flg =  0;                     \
                                                               \
    semop(2, server->lock->sem_array, server->lock->semid);    \
                                                               \
}while(0)                                                      \

#define TRY_READ_OUTGOING()                                    \
do{                                                            \
    server->lock->sem_array->sem_op  = -1;                     \
    server->lock->sem_array->sem_flg =  0;                     \
                                                               \
    semop(3, server->lock->sem_array, server->lock->semid);    \
                                                               \
}while(0)                                                      \

#define PUSH_INCOMING()                                        \
do{                                                            \
    server->lock->sem_array->sem_op  = 1;                      \
    server->lock->sem_array->sem_flg = 0;                      \
                                                               \
    semop(2, server->lock->sem_array, server->lock->semid);    \
                                                               \
}while(0)                                                      \

#define PUSH_OUTGOING()                                        \
do{                                                            \
    server->lock->sem_array->sem_op  = 1;                      \
    server->lock->sem_array->sem_flg = 0;                      \
                                                               \
    semop(3, server->lock->sem_array, server->lock->semid);    \
                                                               \
}while(0)                                                      \

#define THREADS_TERMINATE()                                    \
do {                                                           \
    server->alive_stat = SERVER_DEAD;                          \
                                                               \
    PUSH_OUTGOING();                                           \
    PUSH_INCOMING();                                           \
                                                               \
    if (uv_loop_alive(server->event_loop)) {                   \
        uv_stop(server->event_loop)                            \
        uv_loop_close(server->event_loop)                      \
    }                                                          \
                                                               \
    return;                                                    \
}while(0)                                                      \


#define SEND(msg)                                           \
do{                                                         \
    LOCK_OUTGOING();                                        \
        list_insert(server->outgoing_msg, msg);             \
        PUSH_OUTGOING();                                    \
    UNLOCK_OUTGOING();                                      \
                                                            \
}while(0)                                                   \

#define RECV(msg)                                           \
do{                                                         \
    LOCK_INCOMING(server->incomming_msg);                   \
        list_insert(server->incoming_msg, HEAD_ID, msg);    \
        PUSH_INCOMING();                                    \
    UNLOCK_INCOMING(server->incoming_msg);                  \
}while(0)                                                   \

#define ALIVE_STAT() server->alive_stat

//-------------------------------------------------------------------------------------------------------//

void fill_err_stat(ERR_STAT err_code, char error_msg[]) {

    if (err_code == SUCCESS) {
        char* msg = "Opperation proceded successfuly";
        strncpy(error_msg, msg, strlen(msg));

        return;
    }

    if (err_code == INVALID_MSG) {
        char* msg = "Your client version is not supported";
        strncpy(error_msg, msg, strlen(msg));

        return;
    }

    if (err_code == NAME_TAKEN) {
        char* msg = "User with this name already exists";
        strncpy(error_msg, msg, strlen(msg));

        return;
    }

    if (err_code == INCORRECT_USRN || err_code == INCORRECT_PSWD) {
        char* msg = "Incorrect user name or password";
        strncpy(error_msg, msg, strlen(msg));

        return;
    }
}

cmd_code get_command (char cmd_body[]) {
    size_t offset = sizeof(MSG_TYPE) + NAME_SIZE;

    if (!strncmp(cmd_body + offset, "in$", IN_SIZE)) {
        return LOG_IN;
    }

    if(!strncmp(cmd_body + offset, "online$", ONLINE_SIZE)) {
        return ONLINE;
    }

    if (!strncmp(cmd_body + offset, "out$", OUT_SIZE)) {
        return LOG_OUT;
    }

    if (!strncmp(cmd_body + offset, "reg$", REG_SIZE)) {
        return REGISTR;
    }

    return UNKNOWN;
}

chat_server* create_server(size_t client_num) {
    chat_server* server = CALLOC(1, chat_server);
    server->client_base = create_client_base(client_num);
    server->alive_stat = SERVER_LIVE;

    server->sem_lock            = CALLOC(1, lock);
    server->sem_lock->sem_array = CALLOC(1, sembuf_);

    server->sem_lock->sem_array->sem_num = SEM_NUM;

    size_t key    = ftok("server.cpp", 0);
    size_t sem_id = semget(key, SEM_NUM, 0777 | IPC_CREAT);

    assert(sem_id > 0 && "Invalid sem id");

    server->sem_lock->semid = sem_id;

    list_init(server->incoming_msg, 10);
    list_init(server->outgoing_msg, 10);
}

void destroy_server(chat_server* server) {
    destoy_client_base(server->client_base);
    list_destroy(server->incoming_msg);
    list_destroy(server->outgoing_msg);

    FREE(server->sem_lock->sem_array);
    FREE(server->sem_lock);
    FREE(server);
}


//-------------------------------------------BACKEND--------------------------------------//

//calbacks
void alloc_cb (uv_handle_t* alloc_handle, size_t suggested_size, uv_buf_t* buf) {
    if (buf->len == 0) {
        buf->base = CALLOC(suggested_size, char);
        buf->len  = suggested_size;
    }
    else {
        buf->base = REALLOC(buf->base, suggested_size, char);
        buf->len  = suggested_size; 
    }

    uv_handle_set_data(alloc_handle, buf->base);
}

//close handle = (uv_handle_t*)(uv_stream_t type)
void on_close_connection(uv_handle_t* close_handle) {

    //....
    fprintf(stderr, "Connection closed\n");
}

void on_read(uv_stream_t* client_endpoint, ssize_t nread, const uv_buf_t* buf) {
    if (nread > 0) {
        while (nread > 0) {
            MSG_TYPE   msg_type = get_msg_type(buf->base);
            chat_message_t* msg = create_chat_message(msg_type);

            int read_size = (msg->read_message)(msg, buf->base);
            msg->client_endpoint = client_endpoint;

            RECV(msg);

            nread -= (read_size + sizeof(MSG_TYPE));
        }
    }
    else if (nread < 0) {
        fprintf(stderr, "Error while reading, nread: %d\n", nread);
        fprintf(stderr, "Closing connection...\n");

        uv_close(client_endpoint, on_close_connection);
    }

    //nread == 0 ---> do nothing
    return;
}

void on_write(uv_write_t* write_handle, int status) {
    if (status) {
        fprintf(stderr, "Error while writing: %d\n", write_handle->error);
        fprintf(stderr, "Closing connection...\n");

        uv_close((uv_handle_t*)write_handle->send_handle, on_close_connection);
    }

    FREE(write_handle);
}


void on_new_connection(uv_stream_t* server_endpoint, int status) {
    if (status < 0) {
        fprintf (stderr, "Connection error\n");
        return;
    }

    uv_tcp_t* client_endpoint = CALLOC(1, uv_tcp_t);
    uv_tcp_init(server_endpoint->loop, client_endpoint);

    int accept_stat = uv_accept(server_endpoint, client_endpoint);

    if (accept_stat) {
        fprintf(stderr, "Client accepted\n");
        uv_read_start(client_endpoint, alloc_cb, on_read);
    }
    else {
        fprintf (stderr, "Error while accepting\n");
        fprintf (stderr, "Closing connection...\n");

        uv_close(client_endpoint, on_close_connection);
    }
}


void* start_backend(void* args) {
    thread_args* args = (thread_args*)args;

    uv_loop_t* event_loop = uv_default_loop();

    struct sockaddr_in addr = {};
    uv_tcp_t server_endpoint = 0;

    uv_tcp_init(event_loop, &server_endpoint);
    uv_ip4_addr(args->ip_address, args->port, &addr);

    int bind_stat = uv_tcp_bind(&server_endpoint, (const struct sockaddr *)&addr, 0);

    if (bind_stat) {
        fprintf(stderr, "Bind error: %d\n", bind_stat);
        fprintf(stdin, "exit");

        pthread_exit(NULL);
        return NULL;
    }

    int listen_stat = uv_listen((uv_stream_t *)&server, 1024, on_new_connection);

    if (listen_stat){
        fprintf(stderr, "Listen new connections error: %d\n", listen_stat);
        fprintf(stdin, "exit");

        pthread_exit(NULL);
        return NULL;
    }

    server->event_loop = event_loop;

    int run_stat = uv_run(event_loop, UV_RUN_DEFAULT);
    
    if (run_stat) {
        fprintf(stderr, "Event loop error: %d\n", run_stat);
        fprintf(stdin, "exit");

        pthread_exit(NULL);
        return NULL;
    }

    fprintf(stderr, "Server finalization\n");
    
    pthread_exit(NULL);
    return NULL;
}

//---------------------------------------------------------------------------------------------------//


//---------------------------------------------INTERFACE---------------------------------------------//

void send_all_status_changed(chat_client_t* client) {
    assert(client != NULL);

    buffer_t* buf = create_server_message_buffer(0, CHANGED_STAT);
    fill_body(buf, client->name);

    for (int i = 0; i < server->client_base->size; i ++) {
        chat_client_t* client = server->client_base->base + i;
        if (client->status) {  //user is online
            chat_message_t* msg = create_chat_message(SYSTEM);
            strncpy(msg->to, client->name, NAME_SIZE);
            msg->write_message(msg, buf->buf);

            SEND(msg);
        }
    }
    destroy_type_buffer(buf);
}

void send_online_list(chat_client_t* client) {
    assert(client != NULL);

    buffer_t* buf = create_server_message_buffer(0, ONLINE);

    for (int i = 0; i < server->client_base->size; i ++) {
        chat_client_t* client = server->client_base->base + i;
        if (client->status)
            fill_body(buf, client->name); //user is online
        
    }

    chat_message_t* msg = create_chat_message(SYSTEM);
    strncpy(msg->to, client->name, NAME_SIZE);
    msg->write_message(msg, buf->buf);

    SEND(msg);

    destroy_type_buffer(buf);
}

ERR_STAT operate_command(cmd_code code, chat_message_t* msg) {
    assert(msg != NULL);

    switch(code){
        case LOG_IN: {
            chat_client_t* client = log_in_client(server->client_base, msg->msg_body + IN_SIZE);

            if (client) {
                send_all_status_changed(client);
                change_status(client);

                fprintf(stderr, "New log in, client: [%s]\n", client->name);
                return SUCCESS;
            }

            fprintf(stderr, "BAD LOG IN\n");
            fprintf(stderr, "Client request %s\n\n", msg->msg_body);
            return INCORRECT_LOG_IN;
        }
        break;

        case LOG_OUT: {
            chat_client_t* client = log_out_client(server->client_base, msg->msg_body + OUT_SIZE);

            if (client) {
                change_status(client);
                send_all_status_changed(client);

                fprintf(stderr, "Client [%s] log out\n\n", client->name);
                return SUCCESS;
            }
            fprintf(stderr, "BAD LOG OUT. Client was not found\n");
            fprintf(stderr, "Client request: %s\n\n", msg->msg_body);
        }
        break;

        case REGISTR: {
            chat_client_t* client = registr_client(server->client_base, msg->client_endpoint, msg->msg_body + REG_SIZE);

            if (client) {
                send_all_status_changed(client);
                change_status(client);

                fprintf(stderr, "Client [%s] have just registred!\n\n", client->name);
                return SUCCESS;
            }
            fprintf(stderr, "BAD REGISTRATION\n");
            fprintf(stderr, "Client request: %s\n\n", msg->msg_body);

            return NAME_TAKEN;
        }
        break;

        case ONLINE: {
            send_online_list(msg->client_endpoint);
            fprintf(stderr, "Online list sent\n");
        }
    } 
}



ERR_STAT operate_request(chat_message_t* msg) {
    assert(msg != NULL);

    MSG_TYPE msg_type = msg->msg_type;

    switch (msg_type) {
        case TXT_MSG:
            SEND(msg);
        break;

        case SYSTEM: {
            cmd_code code     = get_command(msg->msg_body);
            ERR_STAT cmd_stat = operate_command(code, msg);

            if (cmd_stat)
                return cmd_stat;
        }
        break;

        case default: {
            fprintf(stderr, "Unexpected msg_type: %d\n", msg_type);
            fprintf(stderr, "Request ignored...\n\n");

            return BAD_REQUEST;
        }
        break;
    }

    return SUCCESS;
}

void send_error_stat(ERR_STAT err_st) {
    chat_message_t* err_msg = create_chat_message(ERROR_MSG);
    err_msg->error_stat = err_st;
    fill_err_stat(err_st, err_msg->msg_body);

    SEND(err_msg)
}

void* start_interface(void* args) {
    while (ALIVE_STAT()) {

        //block until smth will apear to read ---> avoid useless "while" looping
        TRY_READ_INCOMING(); 

        LOCK_INCOMING(server->incoming_msg);
            chat_message_t* msg = NULL;
            if (server->incoming_msg->size) {
                msg = get_elem(server->incoming_msg, HEAD_ID);
                list_delete(server->incomming_msg, HEAD_ID);
            }
        UNLOCK_INCOMING(server->incomming_msg);

        ERR_STAT request_stat = operate_request(msg);

        if (request_stat)
            send_error_stat(msg)
    }

    fprintf(stderr, "Server interface finalization...\n");

    pthread_exit(NULL);
    return NULL;
}

//------------------------------------------------------------------------------------------------------//


//------------------------------------------------------SENDER------------------------------------------//

ERR_STAT send_message(chat_message_t* msg) {
    chat_client_t* client = get_client(server->client_base, msg->to);

    if (client) {
        buffer_t* buf = create_buffer(msg->msg_type);
        msg->write_message(msg, buf->buf);

        //on write request//
        uv_write_t* write_req = CALLOC(uv_write_t, 1);
        uv_write(write_req, client, (uv_buf_t*)buf, 1, on_write);

        destroy_type_buffer(buf);
        return SUCCESS;
    }

    return UNKNOWN_CLIENT;
}


void* start_sender(void* args) {
    while (ALIVE_STAT(server)) {
        chat_message_t* msg = NULL;

        TRY_READ_OUTGOING(); //block until recieve something
        
        LOCK_OUTGOING(server->outgoing_msg);
            if (server->outgoing_msg->size) {
                msg = get_elem(server->outgoing_msg, HEAD_ID);
                list_delete(server->outgoing_msg, HEAD_ID);
            }
        UNLOCK_OUTGOING(server->outgoing_msg);

        ERR_STAT send_stat = send_message(msg);

        if (send_stat)
            send_error_stat(send_stat)
    }

    fprintf(stderr, "Sender thread finalization")

    pthread_exit(NULL);
    return NULL;
}

//--------------------------------------------------------------------------------------------------------//

//simply finish server application on command "exit"
void* governer(void* args) {
    char command[NAME_SIZE];
    fscanf(stdin, "%s", command);

    if (!strncmp("exit", command, strlen("exit")))
        THREADS_TERMINATE();
}


void run_server(const char* ip_address, size_t port) {
    pthread_t   pid[THREAD_NUM];
    thread_args args = {};

    args.ip   = ip_address;
    args.port = port;

    pthread_create(&pid[0], NULL, start_backend,   (void*)&args);
    pthread_create(&pid[1], NULL, start_interface, NULL);
    pthread_create(&pid[2], NULL, start_sender,    NULL);
    pthread_create(&pid[3], NULL, governer,        NULL);

    for (int i = THREAD_NUM - 1; i >= 0; i--)
        pthread_join(pid[i]);
}

int main() {

    server = create_server(10);

    run_server("127.0.0.1", 7000);

    destroy_server();

    return 0;
}