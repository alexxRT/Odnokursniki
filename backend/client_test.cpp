#include "client.h"





int main () {
    client client("127.0.0.1", 8373);

    client.start_odnokursniki("aleksey", "2897403", REGISTER);
    client.init_cache_directory();

    std::string name = "vanya";
    std::string message = "hello how are you?";

    client.send_message(name, message);
    client.send_message("petya", "good");
    client.send_message("vasya", "thanks for asking");


    return 0;
}