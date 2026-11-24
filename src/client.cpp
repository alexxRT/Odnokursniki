#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "client.h"
#include "memory.h"
#include "networking.h"
#include "list_debug.h"
#include "sender.h"
#include <thread>


#ifdef DEBUG_VERSION
    #define FPRINTF(...) fprintf(stderr, __VA_ARGS__)
#else
    #define FPRINTF(...) (void*)0
#endif

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
//command: online$name
//command: out$name


//client SERVER type messages body fill

//client backend plan:
//i made:
// 1) messages api (chat_msg_t, read & write handlers, buffer_t with maintainable capacity,
// 2) threads safe api for messages queues ---> thread_safe.h
// 3) backend libuv - same for client and server
// 4) server sender with same architecture == also done!!!
// I need two types of clients :  one for storing on server, second for client_obj - states for client.cpp


// to do:
// client interface
// ?? backend & frontend connection ??

//for client backend


client_t* create_client() {
    client_t* client = CALLOC(1, client_t);

    client->incoming_msg = new ThreadSafeList<chat_message_t>(10);

    client->status     = STATUS::ONLINE;
    client->event_loop = NULL;

    return client;
}

void set_client_username(client_t* client, const char* username) {
    size_t username_len = strlen(username);
    username_len = username_len > NAME_SIZE ? NAME_SIZE : username_len;

    memset(client->username, 0, NAME_SIZE);
    memmove(client->username, username, username_len);
}


void destroy_client(client_t* client) {
    uv_sem_destroy(&client->backend_ready);
    FREE(client->event_loop);
    delete client->incoming_msg;

    FREE(client);
}

pthread_t run_client_backend(client_t* client, size_t port, const char* ip) {
    thread_args_t& args = client->backend_args;
    uv_sem_init(&client->backend_ready, 0);

    args.owner        = OWNER::CLIENT;
    args.owner_struct = (void*)client;
    args.ip           = ip;
    args.port         = port;

    pthread_t networking_tid;

    #ifdef DEBUG_VERSION
        InitLogFile("../log_client.txt");
    #endif

    pthread_create(&networking_tid, NULL, start_networking, (void*)&args);

    uv_sem_wait(&client->backend_ready);
    return networking_tid;
}