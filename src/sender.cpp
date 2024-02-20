#include "threads_safe.h"
#include "sender.h"
#include "messages.h"
#include "memory.h"
#include "list.h"
#include "list_debug.h"

enum class SENDER_STATUS : int {
    ACTIVE,
    SHUTDOWN
};

SENDER_STATUS status = SENDER_STATUS::SHUTDOWN;

ERR_STAT send_message(server_t* server, chat_message_t* msg) {
    base_client_t* client = get_client(server->client_base, msg->to);
    if (client) {
        buffer_t* buf = create_type_buffer(msg->msg_type);
        msg->write_message(msg, buf);

        call_async_write(client->client_stream, buf);
        destroy_type_buffer(buf);

        return ERR_STAT::SUCCESS;
    }

    return ERR_STAT::UNKNOWN_CLIENT; 
}


ERR_STAT send_message(client_t* client, chat_message_t* msg) {
    buffer_t* buf = create_type_buffer(msg->msg_type);
    msg->write_message(msg, buf);

    call_async_write(client->server_dest, buf);
    destroy_type_buffer(buf);

    return ERR_STAT::SUCCESS;
}


void run_sender(server_t* server) {
    while (status == SENDER_STATUS::ACTIVE) {
        chat_message_t msg = {};

        server->outgoing_msg->wait(); //block until recieve something
        
        //thread safe
        msg = server->outgoing_msg->get_head()->data;
        server->outgoing_msg->delete_head();

        if (msg.sys_command == COMMAND::SHUTDOWN_SEND) {
            status = SENDER_STATUS::SHUTDOWN;
            continue;
        }

        ERR_STAT send_stat = ERR_STAT::SUCCESS;
        send_stat = send_message(server, &msg);


        if (send_stat != ERR_STAT::SUCCESS) {
            chat_message_t err_msg = create_chat_message(MSG_TYPE::ERROR_MSG);
            err_msg.error_stat = send_stat;
            
            server->incoming_msg->insert_head(err_msg);
        }
        else 
            fprintf(stderr, "Message Sent!\n");
    }
} 

void run_sender(client_t* client) {
    while (status == SENDER_STATUS::ACTIVE) { 
        chat_message_t msg = {};

        client->outgoing_msg->wait(); //block until recieve something
        
        //thread safe
        msg = client->outgoing_msg->get_head()->data;
        client->outgoing_msg->delete_head();

        if (msg.sys_command == COMMAND::SHUTDOWN_SEND) {
            status = SENDER_STATUS::SHUTDOWN;
            continue;
        }

        ERR_STAT send_stat = ERR_STAT::SUCCESS;
        send_stat = send_message(client, &msg);

        if (send_stat != ERR_STAT::SUCCESS) {
            chat_message_t err_msg = create_chat_message(MSG_TYPE::ERROR_MSG);
            err_msg.error_stat = send_stat;

            client->incoming_msg->insert_head(err_msg);
        }
        else
            fprintf(stderr, "Message Sent!\n");
    }

}

void* start_sender(void* args) {
    fprintf(stderr, "<<<<<<     Sender Started     >>>>>>\n");
    thread_args* thr_args = (thread_args*)args;

    status = SENDER_STATUS::ACTIVE;
    
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