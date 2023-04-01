#pragma once

#include <memory>
#include <queue>
#include <thread>
#include <mutex>
#include <iostream>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>


//message body has fixed size     - 100 symbols
//user names also have fixed size - 10 symbols

const int MAX_NAME_SIZE = 10;
const int MAX_MSG_SIZE  = 100;

enum class msg_type : uint32_t
{
    Message, //simple message aka: to, from, message.
    History, //to refresh feed.
    Online,  //request for currently online users.
    Registr,
    SignIn,
    StatusChanged //if somebody disconnected
};

