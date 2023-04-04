#pragma once

#include "common_include.h"
#include "thread_safe_queues.h"
#include "message.h"
#include "connection.h"
#include <assert.h>
#include <filesystem>
#include "message_dump.h"



#define CHECK_NAME(name)                        \
do {                                            \
    if (strlen(name.c_str()) > MAX_NAME_SIZE)   \
        return NAME_SIZE_EXCEED;                \
}                                               \
while(0)

#define CHECK_PASS(password)                        \
do {                                                \
    if (strlen(password.c_str()) >= MAX_PASS_SIZE)  \
        return PASS_SIZE_EXCEED;                    \
}                                                   \
while(0)

#define CHECK_MSG(msg_str)                          \
do {                                                \
    if (strlen(msg_str.c_str()) >= MAX_MSG_SIZE)   \
        return PASS_SIZE_EXCEED;                    \
}                                                   \
while(0)

int  fill_vector_message(std::vector<std::vector<char>>& message, ::message<msg_type>  source);
int  fill_history       (std::vector<std::vector<char>>& message, ::message<msg_type>& source);
int  fill_online_list   (std::vector<std::vector<char>>& message, ::message<msg_type>& source);
int  dump_history       (std::string path, std::vector<std::vector<char>>& message);
int  dump_message       (std::string path, std::vector<std::vector<char>>& message, int FLAG);
//void on_message_recieved(message<msg_type>& recv_msg, std::vector<std::vector<char>>& message);


class client {

public:
    client(const std::string& host, const uint16_t port) : socket(context){
        //on construct, we want client to be imediately connected
        boost::asio::ip::tcp::resolver resolver(context);
        boost::asio::ip::tcp::resolver::results_type endpoint = resolver.resolve(host, std::to_string(port));

        server_connection = std::make_unique<connection<msg_type>> (connection<msg_type>::owner::client, \
        context,                                                                                         \
        boost::asio::ip::tcp::socket(context),                                                           \
        deq_messages_in);

        server_connection->connect_to_server(endpoint);
        thr_context = std::thread([this]() { context.run(); });
    };

    virtual ~client(){
        //std::filesystem::remove_all(cache_files_path.c_str());

        if (is_connected()){
            server_connection->disconnect();
        }
        
        context.stop();

        if (thr_context.joinable())
            thr_context.join();

        server_connection.release();
    };


    bool is_connected(){
        if (server_connection)
            return server_connection->is_connected();
        else 
            return false;
    };

    int send_message(std::string to, std::string to_send) {
        CHECK_NAME(to);
        CHECK_MSG (to_send);

        message<msg_type> msg;
        msg.header.id = msg_type::Message;

        std::vector<char> to_send_user(MAX_NAME_SIZE, 0);
        std::vector<char> to_send_msg (MAX_MSG_SIZE , 0);

        std::memcpy (to_send_user.data(), to.c_str(), to.size());
        std::memcpy (to_send_msg.data() , to_send.c_str(), to_send.size());

        msg << user_name;
        msg << to_send_user;
        msg << to_send_msg;

        std::cout << "msg size " << msg << "\n";

        server_connection->send(msg);

        std::vector<std::vector<char>> message;
        fill_vector_message(message, msg);
        dump_message(cache_files_path, message, MSG_TO);
        
        std::cout << "Msg sent!\n";

        return 0;
    }

    void see_online() {
        message<msg_type> msg;
        msg.header.id = msg_type::Online;

        server_connection->send(msg);
        std::cout << "Online request sent!\n";
    }

    void see_history() {
        message<msg_type> msg;
        msg.header.id = msg_type::History;

        server_connection->send(msg);
        std::cout << "History request sent!\n";
    }

    std::vector<std::vector<char>> recv_messages() {
        if (!deq_messages_in.empty()) { 
            auto msg = deq_messages_in.pop_front();

            std::vector<std::vector<char>> message; //to fill
            //on_message_recieved(msg.msg, message);

            return message;
        }
        else {
            std::vector<std::vector<char>> empty_message(0);
            return empty_message;
        }
    };

    int start_odnokursniki(std::string name, std::string password, int FLAG) {
        CHECK_NAME(name);
        CHECK_PASS(password);

        user_name.resize(MAX_NAME_SIZE, 0);
        std::memcpy(user_name.data(), name.c_str(), user_name.size());

        std::vector<char> user_password(MAX_PASS_SIZE, 0);
        std::memcpy(user_password.data(), password.c_str(), password.size());

        std::cout << "user name " << user_name.data() << "\n";

        message<msg_type> msg;

        if (FLAG == REGISTER)
            msg.header.id = msg_type::Registr;
        else if (FLAG == SIGN_IN)
            msg.header.id = msg_type::SignIn;
        else 
            return UNKNOWN_FLAG;

        msg << user_name;
        msg << user_password;

        server_connection->send(msg);

        return wait_for_approve();
    };

        int wait_for_approve() {

        //wait for server reply
        //if its too much time, i should say that smth went wrong
        int efforts = 0;
        while (deq_messages_in.empty() && efforts < 5) {
            sleep(EFFORT_TIME);

            efforts ++;
        };

        if (deq_messages_in.empty())
            return TRY_ENTER_AGAIN; //i would had rather re-entered the app :))

        message<msg_type> last_message = deq_messages_in.pop_back().msg;
        
        switch (last_message.header.id) {
            case msg_type::EnterSuccess:
                std::cout << "Entered odnokursniki!!!\n";
                return ENTER_SUCCESS;

            case msg_type::EnterBad:
                return BAD_ENTER;

            default: 
                return UNKNOWN_FLAG; //aka smth went wrong, please repeate your input
        }

        return UNKNOWN_FLAG;
    }

    int init_cache_directory() {
        for (int i = 0; i < MAX_NAME_SIZE && user_name[i] != 0; i ++)
            cache_files_path.push_back(user_name[i]);

        cache_files_path.push_back('/');

        int on_dir_create = mkdir(cache_files_path.c_str(), S_IRWXU);

        if (on_dir_create != 0)
            return UNKNOWN_FLAG;

        return 0;
    }

protected:
    boost::asio::io_context               context;
    std::thread                           thr_context;  //thread is needed for asio to perform its tasks
    
    boost::asio::ip::tcp::socket          socket;
    std::unique_ptr<connection<msg_type>> server_connection;

private:
    ts_queue<owned_message<msg_type>>     deq_messages_in;
    std::string cache_files_path = "../";
    std::vector<char>  user_name;
};
