#pragma once

#include "common_include.h"
#include "thread_safe_queues.h"
#include "connection.h"
#include "message.h"
#include <assert.h>
#include <filesystem>

//TO DO:: create small lib that will nicely travel through files in derectory

class server {

public:
    server(uint16_t port)
            : asio_acceptor(context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) {

        wait_for_client_connection();

        thread_context = std::thread([this]() { context.run(); });
        std::cout << "[SERVER] started\n";
    };

    virtual ~server(){
        finish();
    };
        
    bool finish(){
        context.stop();

        if (thread_context.joinable())
            thread_context.join();

        std::cout << "[SERVER] Stopped!\n";

        return true;
    };

    void wait_for_client_connection(){
        asio_acceptor.async_accept(
            [this](std::error_code error, boost::asio::ip::tcp::socket socket){ 
                    std::cout << "New Connect " << socket.remote_endpoint() << "\n"; //this method return ip of connection

                    std::shared_ptr<connection<msg_type>> new_connection =  //create new object - new connection
                            std::make_shared<connection<msg_type>>(connection<msg_type>::owner::server, 
                            context,
                            std::move(socket), 
                            deq_message_in);

                    if(on_client_connect(new_connection)){
                        deq_connections.push_back(std::move(new_connection));
                        deq_connections.back()->connect_to_client(id_counter++); // here i start readheaders

                        std::cout << "[" << deq_connections.back()->get_id() << "] Connection Approved\n";
                    } //establish connection
                    else
                        std::cout << "Connection Denied\n";

                //start to try connect new user
                wait_for_client_connection();
            }
            );
    };

    void update(){
        deq_message_in.wait(); // this lock is used to prevent useless while looping

        while (!deq_message_in.empty()) { //tacking all messages from the deq
            auto msg = deq_message_in.pop_front();
            on_message(msg.remote, msg.msg);
        }
    }; 

    //checker that will deside weather to connect user or not 
    bool on_client_connect(std::shared_ptr<connection<msg_type>> client){
        return true;
    };

    //called when client disconnect, this thing will detect did anyone disconnect
    bool on_client_disconnect(std::shared_ptr<connection<msg_type>> client){
        return false;
    };

    bool on_message(std::shared_ptr<connection<msg_type>> client, message<msg_type>& msg){
        switch (msg.header.id){

            case msg_type::History:
                send_user_history(client, msg);
            break;

            case msg_type::Message: 
                send_message(msg); //simply redirect it
            break;

            case msg_type::Online:
                send_online_list(client);
            break;

            case msg_type::Registr:
                on_registr(client, msg);
            break;

            case msg_type::SignIn:
                on_sign_in(client, msg);
            break;

            default:
                std::cout << "Unknown message type " << msg << "\n";
                return false;
            break;
        }

        return true;
    };

    void all_status_changed() {};

    void send_user_history (std::shared_ptr<connection<msg_type>> client, message<msg_type>& msg) {};
    void on_registr(std::shared_ptr<connection<msg_type>> client, message<msg_type>& msg) {};
    void on_sign_in(std::shared_ptr<connection<msg_type>> client, message<msg_type>& msg) {};
    void send_online_list (std::shared_ptr<connection<msg_type>> client) {};
    void send_message(message<msg_type>& msg) {};



private:
    ts_queue<owned_message<msg_type>>                 deq_message_in;
    std::deque<std::shared_ptr<connection<msg_type>>> deq_connections;

    boost::asio::io_context context;
    std::thread             thread_context;

    boost::asio::ip::tcp::acceptor asio_acceptor; //this class will get users sockets

    uint32_t id_counter = 10000;
};