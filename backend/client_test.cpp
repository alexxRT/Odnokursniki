#include "client.h"





int main () {
    client client("127.0.0.1", 7123);

    sleep(2);

    client.start_odnokursniki("nikita", "2897403", SIGN_IN);
    client.send_message("aleksey", "hello, how are u?");
    //client.send_message("aleksey", "how are you, aleksey?");

    sleep(2);

    //client.init_cache_directory();


    // std::string name = "vanya";
    // std::string message = "hello how are you?";

  
    // client.send_message("petya", "good");
    // client.send_message("vasya", "thanks for asking");


    return 0;
}