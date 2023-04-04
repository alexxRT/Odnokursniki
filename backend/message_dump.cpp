#include <stdio.h>
#include "message.h"
#include "common_include.h"



int fill_vector_message(std::vector<std::vector<char>>& message, ::message<msg_type> source) {
    std::vector<char> message_1(MAX_NAME_SIZE, 0);
    std::vector<char> message_2(MAX_NAME_SIZE, 0);
    std::vector<char> message_3(MAX_MSG_SIZE , 0);

    source >> message_3;
    source >> message_2;
    source >> message_1;

    message.push_back(message_1);
    message.push_back(message_2);
    message.push_back(message_3);

    return 0;
};

int dump_message(std::string path, std::vector<std::vector<char>>& message, int FLAG) {
    if (message.size() != 3)
        return DUMP_ERROR;

    std::vector<char> user_name(MAX_NAME_SIZE, 0);

    if (FLAG == MSG_FROM)
        user_name = message[0];
    else 
        user_name = message[1];
    
    std::string file_name;
    std::string extention = ".txt";

    for (int i = 0; i < MAX_NAME_SIZE && user_name[i] != 0; i++)
        file_name.push_back(user_name[i]);

    file_name += extention;
    path += file_name;

    std::cout << path << "\n";

    FILE* dump_file = fopen(path.c_str(), "a");

    if (dump_file == nullptr)
        return DUMP_ERROR;

    message[2].push_back('\n');

    if (FLAG == MSG_FROM) {
        fwrite("* ", sizeof(char), strlen("* "), dump_file);
        fwrite(message[2].data(), sizeof(char), message[2].size(), dump_file);
    }
    else if (FLAG == MSG_TO) {
        fwrite("# ", sizeof(char), strlen("* "), dump_file);
        fwrite(message[2].data(), sizeof(char), message[2].size(), dump_file);
    }
    else
        return UNKNOWN_FLAG;

    fclose(dump_file);

    return 0;
};