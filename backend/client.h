#pragma once

#include "common_include.h"
#include "thread_safe_queues.h"
#include "message.h"
#include "connection.h"
#include "assert.h"


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

    bool registr(std::vector<char>& user_name, std::vector<char>& user_password) {return true;};

    bool sign_in(std::vector<char>& user_name, std::vector<char>& user_password) {return true;};

    void send_message(std::vector<char>& to, std::vector<char>& to_send) {
        message<msg_type> msg;
        msg.header.id = msg_type::Message;

        msg << name;
        msg << to;
        msg << to_send;

        server_connection->send(msg);
        std::cout << "Msg sent!\n";
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

        if (!deq_messages_in.empty()) { // check it for "extra" safety :)
            auto msg = deq_messages_in.pop_front();

            std::vector<std::vector<char>> message;
            on_message(msg.msg, message);

            return message;
        }
        else {
            assert(false && "pop from empty deq\n");
        }
    };

    void on_message(message<msg_type>& recv_msg, std::vector<std::vector<char>>& message) {

        switch (recv_msg.header.id) {
            case msg_type::Message: {
                std::vector<char> tag; //tag is for GUI only

                int tag_size = strlen("Message");
                tag.resize(tag_size);
                message.push_back(tag);

                fill_message(message, recv_msg);
                dump_message(message);
            }
            break;

            case msg_type::Online: {
                std::vector<char> tag;

                int tag_size = strlen("Online");
                tag.resize(tag_size);
                message.push_back(tag);

                fill_list(message, recv_msg);
            }
            break;

            case msg_type::History: {
                std::vector<char> tag;

                int tag_size = strlen("History");
                tag.resize(tag_size);
                message.push_back(tag);

                fill_history(message, recv_msg);
                dump_history(message);
            }
            break;

            case msg_type::Registr:{

            }
            break;

            case msg_type::SignIn: {

            }
            break;

            case msg_type::StatusChanged: {

            }
            break;

            default:
                std::cout << "Unknown message type recieved " << recv_msg << "\n";
            break;
        }
    };

    void fill_message(std::vector<std::vector<char>>& message, ::message<msg_type>& source) {};
    void fill_history(std::vector<std::vector<char>>& message, ::message<msg_type>& source) {};
    void fill_list   (std::vector<std::vector<char>>& message, ::message<msg_type>& source) {};
    void dump_history(std::vector<std::vector<char>>& message) {};
    void dump_message(std::vector<std::vector<char>>& message) {};


protected:
    boost::asio::io_context               context;
    std::thread                           thr_context;  //thread is needed for asio to perform its tasks
    
    boost::asio::ip::tcp::socket          socket;
    std::unique_ptr<connection<msg_type>> server_connection;

private:
    ts_queue<owned_message<msg_type>>     deq_messages_in;
    std::vector<char> name;
};
