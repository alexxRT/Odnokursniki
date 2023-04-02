#include "message.h"
#include "client.h"
#include <stdio.h>


// void on_message_recieved(message<msg_type>& recv_msg, std::vector<std::vector<char>>& message) {

//     switch (recv_msg.header.id) {
//         case msg_type::Message: {
//             std::vector<char> tag; //tag is for GUI only

//             int tag_size = strlen("Message");
//             tag.resize(tag_size);
//             message.push_back(tag);

//             fill_message(message, recv_msg);
//             dump_message(message);
//         }
//         break;

//         case msg_type::Online: {
//             std::vector<char> tag;

//             int tag_size = strlen("Online");
//             tag.resize(tag_size);
//             message.push_back(tag);

//             fill_list(message, recv_msg);
//         }
//         break;

//         case msg_type::History: {
//             std::vector<char> tag;

//             int tag_size = strlen("History");
//             tag.resize(tag_size);
//             message.push_back(tag);

//             fill_history(message, recv_msg);
//             dump_history(message);
//         }
//         break;

//         case msg_type::StatusChanged: {

//         }
//         break;

//         default:
//             std::cout << "Unknown message type recieved " << recv_msg << "\n";
//         break;
//     }
// };


int fill_history(std::vector<std::vector<char>>& message, ::message<msg_type>& source) {
    int num_of_logs = 0;
    int log_size    = 0;
    source >> num_of_logs;

    for (int i = 0; i < num_of_logs; i += 2) {
        source >> log_size;
        std::vector<char> log(log_size, 0);
        source >> log;

        std::vector<char> user_name(MAX_NAME_SIZE, 0);
        source >> user_name;

        message.push_back(user_name);
        message.push_back(log);
    }

    return 0;
};
int dump_history(std::string path, std::vector<std::vector<char>>& message) {
    for (int i = 0; i < message.size(); i += 2) {

        std::vector<char> user_name = message[i];
        std::string file_name;
        std::string extention = ".txt";

        for (int i = 0; i < MAX_NAME_SIZE && user_name[i] != 0; i++)
            file_name.push_back(user_name[i]);

        file_name += extention;
        path      += file_name;

        std::cout << path << "\n";

        FILE* dump_file = fopen(path.c_str(), "w");

        if (dump_file == nullptr)
            return DUMP_ERROR;

        fwrite(message[i + 1].data(), sizeof(char), message[i + 1].size(), dump_file);
        fclose (dump_file);
    }

    return 0;
};

int fill_online_list(std::vector<std::vector<char>>& message, ::message<msg_type>& source) {
    int list_size = 0;
    source >> list_size;

    std::cout << "debug info list size " << list_size << "\n"; 

    for (int i = 0; i < list_size; i ++) {
        std::vector<char> client(MAX_NAME_SIZE, 0);
        source >> client;

        message.push_back(client);
    }

    return 0;
};

int fill_message(std::vector<std::vector<char>>& message, ::message<msg_type>& source) {
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

    std::vector<char> user_name = message[1];
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