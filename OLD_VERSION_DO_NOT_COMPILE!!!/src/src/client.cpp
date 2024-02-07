#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "client.h"
#include "memory.h"
#include "networking.h"
#include "list_debug.h"


#ifdef DEBUG_VERSION
    #define FPRINTF(str) fprintf(stderr, str)
#else 
    #define FPRINTF(str) (void*)0
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

    client->incoming_msg = new ts_list(10);
    client->outgoing_msg = new ts_list(10);
    
    client->status     = STATUS::ONLINE;
    client->name_hash  = 0;
    client->event_loop = NULL;
    
    return client;
}

void destroy_client(client_t* client) {
    delete client->incoming_msg;
    delete client->outgoing_msg;

    FREE(client);
}

void call_sender_finalization(client_t* client) {
    assert(client);
    chat_message_t msg = create_chat_message(MSG_TYPE::SENDER_SHUTDOWN);
    client->outgoing_msg->insert_head(msg);

    return;
}

void call_networking_finalization(client_t* client) {
    assert(client);
    // if uv_loop wasnt finalized
    if(uv_loop_alive(client->event_loop)) {
        FPRINTF("event loop stop called\n");
        call_async_stop();
    }
}

void* start_client_interface(void* args) {
    thread_args* client_args = (thread_args*)args;
    client_t* client = (client_t*)client_args->owner_struct;

    fprintf(stderr, "<<<<<<     Client Interface Started     >>>>>>\n");

    while (client->status == STATUS::ONLINE) {
        client->incoming_msg->wait();

        chat_message_t msg = client->incoming_msg->get_head()->data;
        client->incoming_msg->delete_head();

        if (msg.msg_type == MSG_TYPE::ON_CLOSE) {
            fprintf(stderr, "Disconnect confirmed!\n");
            client->status = STATUS::OFFLINE;

            call_sender_finalization(client);
            call_networking_finalization(client);

            continue;
        }

        #ifdef DEBUG_VERSION
            fprintf (stderr, "Client Message Recieved!\n");
            fprintf (stderr, "Message Type: %lu\n", msg.msg_type);
        #endif
        
    }


    fprintf(stderr, "<<<<<<     Interface Finalization     >>>>>>\n");

    pthread_exit(NULL);
    return NULL;
}


void run_client_backend(client_t* client, size_t port, const char* ip) {
    thread_args args = {};

    args.owner        = OWNER::CLIENT;
    args.owner_struct = (void*)client;
    args.ip_addr      = ip;
    args.port         = port; 

    pthread_t thread_id[THREAD_NUM];

    InitLogFile("../log_client.txt");
    
    pthread_create(&thread_id[0], NULL, start_networking,       (void*)&args);
    pthread_create(&thread_id[1], NULL, start_client_interface, (void*)&args);
    pthread_create(&thread_id[2], NULL, start_sender,           (void*)&args);
    
    for (int i = THREAD_NUM - 1; i >= 0; i --) {
        pthread_join(thread_id[i], NULL);
    }
}