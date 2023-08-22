#include "chat_base.h"
#include "messages.h"
#include "server.h"
#include "chat_configs.h"

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

//reply always in format: ERROR_MSG err_stat error_info_body
//request in format:      SERVER from command

//command: in$namepassword
//command: reg$namepassword
//command: online$
//command: out$

//server's reply
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

commands get_command (char cmd_body[]) {
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
    server->event_loop = NULL;

    list_init(server->incoming_msg, 10);
    list_init(server->outgoing_msg, 10);
}

void destroy_server(chat_server* server) {
    destoy_client_base(server->client_base);
    list_destroy(server->incoming_msg);
    list_destroy(server->outgoing_msg);
    //destroy event loop?? server->event_loop

    FREE(server);
}


//-------------------------------------------BACKEND--------------------------------------//

//calbacks

//return error stat
int start_server_backend(chat_server* server, char* const ip_address, size_t port) {
    server->event_loop = uv_default_loop();

    struct sockaddr_in addr = {};
    uv_tcp_t server_endpoint = 0;

    uv_tcp_init(server->event_loop, &server_endpoint);
    uv_ip4_addr(ip_address, port, &addr);

    int bind_stat = uv_tcp_bind( &server, (const struct sockaddr *)&addr, 0);

    if (bind_stat) {
        //log error
        //return BIND_FAIL
    }

    int listen_stat = uv_listen((uv_stream_t *)&server, 1024, on_new_connection);

    if (listen_stat){
        //log server error
        //return LISTEN_FAILED
    }

    int run_stat = uv_run(server->event_loop, UV_RUN_DEFAULT);
    
    if (run_stat) {
        //backend finished with error, log
        //return LOOP_CRASHED
    }
}

//---------------------------------------------------------------------------------------------------//


//---------------------------------------------INTERFACE---------------------------------------------//


int start_server_interface(chat_server* server) {

    //semaphores are needed
    //semaphores pass in two cases: 
    //first - server is dead, second - msg list is not empty

    //REMOVE while(true)

    while (true) {
        if (!is_alive(server))
            return 0; //if not, return and join threads 

        while (READ_ALLOWED(server->incoming_msg) && list_size != 0) {
            LOCK_WRITE(server->incoming_msg);

            chat_message_t* msg = NULL;

            msg = get_elem(server->incoming_msg, HEAD_ID);
            list_delete(server->incomming_msg, HEAD_ID);

            UNLOCK_WRITE(server->incoming_msg);

            operate_request(msg);
        }

        if (list_size == 0)
            LOCK_INTERFACE(server);

        //repeate
    }
}

//------------------------------------------------------------------------------------------------------//


//------------------------------------------------------SENDER------------------------------------------//

int start_server_sender(chat_server* server) {

    //semaphores are needed
    //semaphores pass in two cases: 
    //first - server is dead, second - msg list is not empty

    //REMOVE while(true)

    while (true) {
        if (!is_alive(server))
            return 0; //if not, return and join threads 

        while (READ_ALLOWED(server->outgoing_msg) && list_size != 0) {
            LOCK_WRITE(server->outgoing_msg);

            chat_message_t* msg = NULL;
            
            msg = get_elem(server->outgoing_msg, HEAD_ID);
            list_delete(server->outgoing_msg, HEAD_ID);

            UNLOCK_WRITE(server->outgoing_msg);

            send_message(msg);
        }

        if (list_size == 0)
            LOCK_SENDER(server);

        //repeat
    }
}

//--------------------------------------------------------------------------------------------------------//