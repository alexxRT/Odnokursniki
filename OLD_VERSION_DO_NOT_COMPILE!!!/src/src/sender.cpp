#include "threads_safe.h"
#include "sender.h"
#include "messages.h"
#include "memory.h"
#include "list.h"

ERR_STAT send_message(server_t* server, chat_message_t* msg) {
    base_client_t* client = get_client(server->client_base, msg->to);
    if (client) {
        buffer_t* buf = create_buffer(msg->msg_type);
        msg->write_message(msg, buf->buf);

        call_async_write(client->client_stream, buf);
        destroy_type_buffer(buf);

        return ERR_STAT::SUCCESS;
    }

    return ERR_STAT::UNKNOWN_CLIENT; 
}


ERR_STAT send_message(client_t* client, chat_message_t* msg) {
    buffer_t* buf = create_buffer(msg->msg_type);
    msg->write_message(msg, buf->buf);

    call_async_write(client->server_dest, buf);
    destroy_type_buffer(buf);

    return ERR_STAT::SUCCESS;
}


void run_sender(server_t* server) {
    while (ALIVE_STAT(server)) {
        chat_message_t* msg = NULL;

        TRY_READ_OUTGOING(server); //block until recieve something
        
        //thread safe
        list_delete_left(server->outgoing_msg, 0, &msg);
        assert(msg != NULL && "Message null on list delete!");

        ERR_STAT send_stat = ERR_STAT::SUCCESS;
        if (msg) {
            send_stat = send_message(server, msg);
            destroy_chat_message(msg); //destroy message after sent
        }

        if (send_stat != ERR_STAT::SUCCESS) {
            chat_message_t* err_msg = create_chat_message(MSG_TYPE::ERROR_MSG);
            err_msg->error_stat = send_stat;
            
            list_insert_right(server->incoming_msg, 0, err_msg);
        }
    }
} 

void run_sender(client_t* client) {
    while (ALIVE_STAT(client)) {
        chat_message_t* msg = NULL;

        TRY_READ_OUTGOING(client); //block until recieve something
        
        //thread safe
        list_delete_left(client->outgoing_msg, 0, &msg);
        assert(msg != NULL && "Message null on list delete!");
        
        ERR_STAT send_stat = ERR_STAT::SUCCESS;
        if (msg) {
            send_stat = send_message(client, msg);
            destroy_chat_message(msg); //destroy message after sent
        }

        if (send_stat != ERR_STAT::SUCCESS) {
            chat_message_t* err_msg = create_chat_message(MSG_TYPE::ERROR_MSG);
            err_msg->error_stat = send_stat;

            list_insert_right(client->incoming_msg, 0, err_msg);
            RELEASE_INCOMING(client);
        }
    }

}

void* start_sender(void* args) {
    thread_args* thr_args = (thread_args*)args;
    
    if (thr_args->owner == OWNER::SERVER) {
        server_t* server = (server_t*)thr_args->owner_struct;
        run_sender(server);
    }
    else { 
        client_t* client = (client_t*)thr_args->owner_struct;
        run_sender(client);
    }

    fprintf(stderr, "<<<<<<     Sender finalization     >>>>>>\n");

    pthread_exit(NULL);
    return NULL;
}