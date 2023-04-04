#pragma once

#include "common_include.h"
#include "thread_safe_queues.h"
#include "connection.h"
#include "message.h"
#include <assert.h>
#include <filesystem>
#include "message_dump.h"

//TO DO:: create small lib that will nicely travel through files in derectory

struct client_info {
    std::shared_ptr<connection<msg_type>> client_connection;
    uint64_t    password_hash;
    std::string dump_path;
};

class server {

public:
    server(uint16_t port)
            : asio_acceptor(context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) {

        wait_for_client_connection();

        thread_context = std::thread([this]() { context.run(); });
        mkdir(dump_path_root.c_str(), S_IRWXU); //intit dirrectory for messsage dumping

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

    void send_online_list (std::shared_ptr<connection<msg_type>> client) {
        message<msg_type> msg;
        msg.header.id = msg_type::Online;
        std::unordered_map<std::shared_ptr<connection<msg_type>>, std::vector<char>>::iterator it = online.begin();

        int online_size = 0;

        while (it != online.end()) {
            msg << it->second;

            online_size ++;
            it++;
        }

        msg << online_size;
        client->send(msg);
    };

    void on_registr(std::shared_ptr<connection<msg_type>> client, message<msg_type>& msg) {
        std::vector<char> name    (MAX_NAME_SIZE, 0);
        std::vector<char> password(MAX_PASS_SIZE, 0);

        msg >> password;
        msg >> name;

        std::string user_name = "";
        for (int i = 0; i < MAX_NAME_SIZE && name[i] != 0; i ++) //copying the name into string
            user_name.push_back(name[i]);

        if (authorized.find(user_name) != authorized.end()) {  //user already exists with this name
            message<msg_type> reply;
            reply.header.id = msg_type::EnterBad;

            client->send(reply);
        } 
        else {
            uint64_t hash = 1;//std::hash<vector<char>>(password); //does not work on vectors!!!!!!!!

            struct client_info user = {};
            user.password_hash = hash;
            user.client_connection = client;
            user.dump_path = dump_path_root + user_name + "/";

            mkdir(user.dump_path.c_str(), S_IRWXU);

            authorized[user_name] = user;

            message<msg_type> reply;
            reply.header.id = msg_type::EnterSuccess;

            client->send(reply);

            online[client] = name;
            std::cout << "Client with name " << user_name << " successfully REGISTERED\n";
        }
    };


    void on_sign_in(std::shared_ptr<connection<msg_type>> client, message<msg_type>& msg) {
        std::vector<char> name    (MAX_NAME_SIZE, 0);
        std::vector<char> password(MAX_PASS_SIZE, 0);

        msg >> password;
        msg >> name;

        std::string user_name = "";
        for (int i = 0; i < MAX_NAME_SIZE && name[i] != 0; i ++) //copying the name into string
            user_name.push_back(name[i]);

        if (authorized.find(user_name) == authorized.end()) { //could't find the user with this name
            message<msg_type> reply;
            reply.header.id = msg_type::EnterBad;

            client->send(reply);
        }
        else {
            struct client_info user = {};
            user = authorized[user_name];
            uint64_t entered_pass = 1; //hash_func(password);

            if (entered_pass != user.password_hash) {
                message<msg_type> reply;
                reply.header.id = msg_type::EnterBad;

                client->send(reply);
            }
            else {
                message<msg_type> reply;
                reply.header.id = msg_type::EnterSuccess;

                client->send(reply);

                online[client] = name;
                std::cout << "Client with name " << user_name << " successfully SIGNED IN\n";
            }
        }
    };

    void send_message(message<msg_type>& msg) {
        std::vector<std::vector<char>> message;
        fill_vector_message(message, msg);

        struct client_info to_info   = {};
        struct client_info from_info = {};

        std::string str_to;
        std::string str_from;

        for (int i = 0; i < MAX_NAME_SIZE && message[0][i] != 0; i ++)
            str_to.push_back(message[0][i]);
        
        for (int i = 0; i < MAX_NAME_SIZE && message[1][i] != 0; i ++)
            str_from.push_back(message[1][i]);

        from_info = authorized[str_from];
        to_info   = authorized[str_to];

        //dump the message to both users' histories
        std::cout << from_info.dump_path.c_str() << "\n";
        std::cout << to_info.dump_path.c_str()   << "\n";

        dump_message(from_info.dump_path, message, MSG_FROM);
        dump_message(to_info.dump_path,   message, MSG_TO);

        if (to_info.client_connection->is_connected())
            to_info.client_connection->send(msg); //finally redirect the message
    };



private:
    ts_queue<owned_message<msg_type>>                 deq_message_in;  // deq for incomming requests 

    std::deque<std::shared_ptr<connection<msg_type>>>   deq_connections;             // deq for not authorized users
    std::unordered_map<std::string, struct client_info> authorized;                  // all autorized clients 
    std::unordered_map<std::shared_ptr<connection<msg_type>>, std::vector<char>> online; // only online users

    boost::asio::io_context context;
    std::thread             thread_context;

    boost::asio::ip::tcp::acceptor asio_acceptor; //this class will get users sockets

    std::string dump_path_root = "../server_cache/";


    uint32_t id_counter = 10000;
};