#ifndef CONFIGS_H
#define CONFIGS_H

#include <string.h>

enum commands {
    LOG_IN,
    REGISTR,
    LOG_OUT,
    ONLINE,
    UNKNOWN = -1
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
    SUCCESS = 0,
    CONNECTION_LOST = 1,
    INVALID_MSG = 2,
    NAME_TAKEN = 3,
    INCORRECT_PSWD = 4,
    INCORRECT_USRN = 5
    //....... fill further then .......
};

const int PSWD_SIZE = 10;
const int NAME_SIZE = 10;

const int MSG_SIZE = 100;

#endif