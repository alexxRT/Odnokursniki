#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "client.h"
#include "memory.h"
#include "networking.h"

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

    client->sem_lock = CALLOC(1, lock);
    client->sem_lock->sem_array = CALLOC(1, sembuf_);
    client->sem_lock->sem_array->sem_num = SEM_NUM;
    client->sem_lock->semid = ftok("client.cpp", IPC_CREAT | 0777);

    assert(client->sem_lock->semid > 0);

    #ifdef DEBUG_VERSION
        fprintf(stderr, "\n\n--------------DEBUG INFO---------------\n")
        fprintf(stderr, "sem id value: %lu\n", client->sem_lock->semid);
        fprintf(stderr, "-----------------------------------------\n\n");
    #endif

    list_init(client->incoming_msg, 10, destroy_chat_message);
    list_init(client->outgoing_msg, 10, destroy_chat_message);
    
    client->alive_stat = ALIVE_STAT::ALIVE;
    client->status     = STATUS::OFFLINE;
    client->name_hash  = 0;
    client->event_loop = NULL;
    
    return client;
}

void destroy_client(client_t* client) {
    list_destroy(client->incoming_msg);
    list_destroy(client->outgoing_msg);

    FREE(client->server_dest);
    FREE(client->sem_lock->sem_array);
    FREE(client->sem_lock);
    FREE(client);
}

void* start_interface(void* args) {
    //TO DO//
    return NULL;
}


void run_client_backend(client_t* client, size_t port, const char* ip) {
    thread_args args = {};

    args.owner = OWNER::CLIENT;
    args.owner_struct = (void*)client;
    args.ip   = ip;
    args.port = port; 

    pthread_t thread_id[THREAD_NUM];
    
    pthread_create(&thread_id[0], NULL, start_networking, (void*)&args);
    pthread_create(&thread_id[1], NULL, start_interface,  (void*)&args);
    pthread_create(&thread_id[2], NULL, start_sender,     (void*)&args);
    pthread_create(&thread_id[3], NULL, governer,         (void*)&args);

    for (int i = THREAD_NUM - 1; i >= 0; i --) {
        pthread_join(thread_id[i], NULL);
    }
}