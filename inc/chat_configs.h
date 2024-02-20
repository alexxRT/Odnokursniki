#ifndef CONFIGS_H
#define CONFIGS_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>


enum class COMMAND : uint64_t {
    LOG_IN        = 1,
    LOG_OUT       = 2,
    REGISTR       = 3,
    ONLINE        = 4,
    CHANGED_STAT  = 5,
    ON_CLOSE      = 6,
    SHUTDOWN_SEND = 7,
    UNKNOWN       = 8
};

enum class STATUS : int {
    ONLINE  = 1,
    OFFLINE = 0
};

enum class OWNER {
    CLIENT = 0,
    SERVER = 1
};

const int THREAD_NUM = 4;

enum class ERR_STAT : uint64_t{
    SUCCESS          = 0,
    CONNECTION_LOST  = 1,
    INVALID_MSG      = 2,
    NAME_TAKEN       = 3,
    INCORRECT_LOG_IN = 4,
    BAD_REQUEST      = 5,
    UNKNOWN_CLIENT   = 6,
    UNKNOWN_CMD      = 7,
    BIND_ERR         = 8,
    LISTEN_ERR       = 9,
    LOOP_EXIT_ERROR  = 10,
    CONNECT_ERR      = 11

    //....... fill further then .......
};

const int PSWD_SIZE = sizeof(uint64_t);
const int NAME_SIZE = sizeof(uint64_t);

const int MSG_SIZE = 100;

#endif