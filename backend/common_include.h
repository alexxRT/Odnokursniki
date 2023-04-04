#pragma once

#include <memory>
#include <queue>
#include <thread>
#include <mutex>
#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>


//message body has fixed size     - 100 symbols
//user names also have fixed size - 10 symbols

const int MAX_NAME_SIZE = 16;
const int MAX_PASS_SIZE = 32;
const int MAX_MSG_SIZE  = 128;

const int REGISTER = 1;
const int SIGN_IN  = 2;
const int MSG_FROM = 3;
const int MSG_TO   = 4;
const int EFFORT_TIME = 1;
const int MAX_EFFORTS = 5;

enum ERROR_LIST : uint32_t {
    ENTER_SUCCESS    = 0,
    BAD_ENTER        = 1,
    NAME_SIZE_EXCEED = 2,
    PASS_SIZE_EXCEED = 3,
    UNKNOWN_FLAG     = 4,
    TRY_ENTER_AGAIN  = 5,
    DUMP_ERROR       = 7
};

enum class msg_type : uint32_t
{
    Message, //simple message aka: to, from, message.
    History, //to refresh feed.
    Online,  //request for currently online users.
    Registr,
    SignIn,
    StatusChanged, //if somebody disconnected
    EnterSuccess,
    EnterBad
};

