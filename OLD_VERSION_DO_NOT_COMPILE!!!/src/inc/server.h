#ifndef SERVER_H
#define SERVER_H

#include "chat_base.h"
#include "chat_configs.h"
#include "list.h"
#include "uv.h"
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <fcntl.h>

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

typedef struct sembuf sembuf_;

typedef struct lock_ {
    sembuf_* sem_array;
    size_t   semid;
}lock;


enum class ALIVE_STAT : size_t{
    ALIVE = 1,
    DEAD = 0
};

typedef struct server_ {
    ALIVE_STAT alive_stat;
    chat_base_t* client_base;
    list_* incoming_msg; 
    list_* outgoing_msg;
    lock* sem_lock;
    uv_loop_t* event_loop;
    
}chat_server;




#endif