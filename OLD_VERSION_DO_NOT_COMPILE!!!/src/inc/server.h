#ifndef SERVER_H
#define SERVER_H

#include "chat_base.h"
#include "chat_configs.h"
#include "list.h"
#include "uv.h"

//PLAN//
//three threads: 
//backend  :        accept connection, recieve messages, send messages
//interfece:        pop incoming, change server state, push reply to outgoing
//sender   :        pop ougoing list, init send callback

//LOGIC//ACTIONS//
//backend:
//recieve bytes->formate msg_type->add to incomming list msg
//accept connections (init tcp client)
//send bytes

//interfece:
//pop incomming msg->perform logic->create response && change server state->push into outgoing msg

//sender: 
//look up for updates in outgoing msg list
//create bytes message, create callback

//mutexes:
//1) backend writes into incomming msg list, interfece reads
//2) interfece writes into outgoing msg list, sennder reads

//hot parts: write-read incomming list
//           write-read outgoing list

typedef struct server_ {
    chat_base_t* client_base;
    my_list* incoming_msg; 
    my_list* outgoing_msg;

    uv_loop_t* event_loop;
}chat_server;






#endif