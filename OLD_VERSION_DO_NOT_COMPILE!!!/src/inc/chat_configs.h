#ifndef CONFIGS_H
#define CONFIGS_H

#include <string.h>

enum cmd_code {
    LOG_IN       = 0,
    LOG_OUT      = 1,
    REGISTR      = 2,
    ONLINE       = 3,
    CHANGED_STAT = 4,
    UNKNOWN = -1
};

const char commands[] = {
    "in$",
    "out$",
    "reg$",
    "online$",
    "chstat$"
};

enum status {
    ONLINE = 1,
    OFFLINE = 0
};

const int IN_SIZE     = strlen("in$");
const int OUT_SIZE    = strlen("out$");
const int REG_SIZE    = strlen("reg$");
const int ONLINE_SIZE = strlen("online$");

enum ERR_STAT : int{
    SUCCESS          = 0,
    CONNECTION_LOST  = 1,
    INVALID_MSG      = 2,
    NAME_TAKEN       = 3,
    INCORRECT_LOG_IN = 4,
    BAD_REQUEST      = 5,
    UNKNOWN_CLIENT   = 6,
    UNKNOWN_CMD      = 7

    //....... fill further then .......
};

const int PSWD_SIZE = sizeof(uint64_t);
const int NAME_SIZE = sizeof(uint64_t);

const int MSG_SIZE = 100;

#endif