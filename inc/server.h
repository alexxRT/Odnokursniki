#ifndef SERVER_H
#define SERVER_H

#include "chat_configs.h"
#include "chat_session.h"
#include "ts_list.h"
#include "chat_base.h"
#include "list.h"
#include "uv.h"


// PLAN//
// three threads:
// backend  :        accept connection, recieve messages, send messages
// interfece:        pop incoming, change server state, push reply to outgoing
// sender   :        pop outgoing list, init send callback

// LOGIC//ACTIONS//
// backend:
// recieve bytes->formate msg_type->add to incomming list msg
// accept connections (init tcp client)
// send bytes

// interfece:
// pop incomming msg->perform logic->create response && change server state->push into outgoing msg

// sender:
// look up for updates in outgoing msg list
// create bytes message, create callback

// mutexes:
// 1) backend writes into incomming msg list, interfece reads
// 2) interfece writes into outgoing msg list, sender reads

// hot parts: write-read incomming list
//            write-read outgoing list

enum SERVER_STATUS : int {
    DOWN = 0,
    UP   = 1
};

enum BROADCAST : int {
    CLIENT_LOG_IN = 0,
    CLIENT_LOG_OUT = 1,
};

typedef struct server_ {
    SERVER_STATUS status;
    chat_base_t* client_base;
    FILE* history;
    ThreadSafeList<chat_message_t>* incoming_msg;
    ThreadSafeList<chat_message_t>* outgoing_msg;
    uv_loop_t* event_loop;
    chat_session_t active_sessions[MAX_SESSIONS_COUNT];
}server_t;


server_t* create_server (size_t client_num);
void      destroy_server(server_t* server);

void run_server_backend(server_t* server, const char* ip_address, size_t port);


#endif