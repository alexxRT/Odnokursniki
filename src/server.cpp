#include "chat_base.h"
#include "messages.h"
#include "server.h"
#include "chat_configs.h"
#include "networking.h"
#include "sender.h"
#include <unistd.h>
#include <iostream>

//SEND MESSAGE
//1) creat char buffer for body
//2) fill body
//3) create appropriate buffer_t
//4) send

//RECIEVE MESSAGE
//1) read type
//2) create appropriate buffer_t buf
//3) read bytes from socket into buf
//4) fill msg from buf


//system commands describtion. Client <--> Server interaction

//command: in$namepassword
//command: reg$namepassword
//command: online$
//command: out$

// 0 -- incoming sem index
// 1 -- outgoing sem index
// 2 -- incoming try read index
// 3 -- outgoing try read index


//-------------------------------------------------------------------------------------------------------//


typedef struct command_result {
    ERR_STAT status_id;
    COMMAND command_id;
} cmd_result_t;

void fill_err_stat(ERR_STAT err_code, buffer_t* buf) {

    if (err_code == ERR_STAT::SUCCESS) {
        const char* err_msg = "Opperation proceded successfuly";
        fill_body(buf, (void*)err_msg, strlen(err_msg));

        return;
    }

    if (err_code == ERR_STAT::INVALID_MSG) {
        const char* err_msg = "Your client version is not supported";
        fill_body(buf, (void*)err_msg, strlen(err_msg));

        return;
    }

    if (err_code == ERR_STAT::NAME_TAKEN) {
        const char* err_msg = "User with this name already exists";
        fill_body(buf, (void*)err_msg, strlen(err_msg));

        return;
    }

    if (err_code == ERR_STAT::BAD_LOGIN) {
        const char* err_msg = "Incorrect user name or password";
        fill_body(buf, (void*)err_msg, strlen(err_msg));

        return;
    }
}

server_t* create_server(size_t client_num) {
    server_t* server = CALLOC(1, server_t);
    server->client_base = create_client_base(client_num);
    server->status = SERVER_STATUS::UP;

    for (size_t i = 0; i < MAX_SESSIONS_COUNT; i ++) {
        server->active_sessions[i].client_id = 0;
        server->active_sessions[i].client_name = 0;
        server->active_sessions[i].client_stream = NULL;
        server->active_sessions[i].state = CLIENT_STATE::DISCONNECTED;
    }

    server->incoming_msg = new ThreadSafeList<chat_message_t>(10);
    server->outgoing_msg = new ThreadSafeList<chat_message_t>(10);

    return server;
}

void destroy_server(server_t* server) {
    destoy_client_base(server->client_base);

    delete server->incoming_msg;
    delete server->outgoing_msg;

    FREE(server);
}

//---------------------------------------------INTERFACE---------------------------------------------//

void send_all_status_changed(server_t* server, base_client_t* client) {
    assert(client);
    assert(server);

    char server_name[NAME_SIZE] = {0};

    for (int i = 0; i < server->client_base->size; i ++) {
        base_client_t* client_recv = server->client_base->base + i;

        if (client_recv->status == STATUS::ONLINE) {
            chat_message_t msg = create_chat_message(MSG_TYPE::SYSTEM);
            msg.sys_command = COMMAND::ONLINE;

            strncpy(msg.to, client_recv->name, NAME_SIZE);
            strncpy(msg.from, server_name, NAME_SIZE);
            strncpy(msg.msg_body, client->name, NAME_SIZE);

            server->outgoing_msg->insert_head(msg);
        }
    }
}

void send_online_list(server_t* server, base_client_t* client) {
    assert(client);
    assert(server);

    char server_name[NAME_SIZE] = {0};

    chat_message_t msg = create_chat_message(MSG_TYPE::SYSTEM);
    msg.sys_command = COMMAND::ONLINE;

    strncpy(msg.to, client->name, NAME_SIZE);
    strncpy(msg.from, server_name, NAME_SIZE);

    for (int i = 0; i < server->client_base->size; i ++) {
        base_client_t* client_stored = server->client_base->base + i;
        size_t position = 0;

        if (client->status == STATUS::ONLINE) {
            strncpy(msg.msg_body + position, client_stored->name, NAME_SIZE);
            position += NAME_SIZE;
        }
    }

    server->outgoing_msg->insert_head(msg);
}

cmd_result_t operate_command(server_t* server, chat_session_t* session, chat_message_t* msg) {
    assert(server  && "[FATAL] server is NULL on operate_command");
    assert(session && "[FATAL] session is NULL on operate_command");
    assert(msg     && "[FATAL] msg is NULL on operate_command");

    COMMAND code = msg->sys_command;

    switch(code) {
        case COMMAND::LOG_IN: {
            ERR_STAT LOGGED_IN = log_in_client(server->client_base, session, msg->msg_body);

            fprintf(stderr, "[INFO] client logged in; session_id=[%llu]\n", session->client_id) ? LOGGED_IN == ERR_STAT::SUCCESS :
            fprintf(stderr, "[ERROR] client failed to log in; session_id=[%llu]\n", session->client_id);

            return cmd_result_t({
                .command_id = COMMAND::LOG_IN,
                .status_id  = LOGGED_IN
            });
        }
        break;

        case COMMAND::REGISTR: {
            ERR_STAT REGISTERED = registr_client(server->client_base, session, msg->msg_body);

            fprintf(stderr, "[INFO] client regitered; session_id=[%llu]\n", session->client_id) ? REGISTERED == ERR_STAT::SUCCESS :
            fprintf(stderr, "[ERROR] client failed to register; session_id=[%llu]\n", session->client_id);

            return cmd_result_t({
                .command_id = COMMAND::REGISTR,
                .status_id  = REGISTERED
            });
        }
        break;

        case COMMAND::LOG_OUT: {
            base_client_t* client = log_out_client(server->client_base, msg->msg_body);

            if (client) {
                // change_status(client);
                send_all_status_changed(server, client);

                //closing connection
                call_async_close((uv_handle_t*)client->client_stream);

                fprintf(stderr, "Client [%s] log out\n\n", client->name);
                //return ERR_STAT::SUCCESS;
            }
            fprintf(stderr, "BAD LOG OUT. Client was not found\n");
            fprintf(stderr, "Client request: %s\n\n", msg->msg_body);
        }
        break;

        case COMMAND::ONLINE: {
            base_client_t* client = get_client(server->client_base, msg->client_endpoint);
            send_online_list(server, client);
            fprintf(stderr, "Online list sent\n");
        }
        break;

        case COMMAND::ON_CLOSE: {
            fprintf (stderr, "Connection closed. Confirmed!\n");
            //return ERR_STAT::SUCCESS;
        }
        break;

        default:
            fprintf(stderr, "UNKNOWN COMMAND\n");
            fprintf(stderr, "Client full request: %s\n", msg->msg_body);
        break;
    }
    return cmd_result_t ({
        .status_id = ERR_STAT::BAD_REQUEST,
        .command_id = COMMAND::UNKNOWN
    });
}

ERR_STAT operate_request(server_t* server, chat_session_t* session, chat_message_t* msg) {
    MSG_TYPE msg_type = msg->msg_type;

    switch (msg_type) {
        case MSG_TYPE::TXT_MSG: {
            // find addressant session
            uint64_t addressant_id = server->client_base->hash_client(msg->to);
            chat_session_t* addressant_session = get_current_session_by_name(server->active_sessions, addressant_id);
            ERR_STAT send_status = addressant_session ? ERR_STAT::SUCCESS : ERR_STAT::CLIENT_OFFLINE;

            // check if online, otherwise drop message for now
            if (send_status == ERR_STAT::CLIENT_OFFLINE) {
                fprintf(stderr, "[INFO] Client=[%llu] is offline, unreachable\n", addressant_id);
                fprintf(stderr, "[ERROR] Unable to send message to=[%s]; session_id=[%llu]\n", msg->to, session->client_id);
            } else {
                fprintf(stderr, "[INFO] Retrnsmitting message: from=[%s], to=[%s]\n", msg->from, msg->to);
                send_status = send_message(server, addressant_session, msg);
            }

            chat_message_t status_msg = create_chat_message(MSG_TYPE::ERROR_MSG);
            status_msg.error_stat = send_status;
            status_msg.sys_command = COMMAND::SEND_MESSAGE;
            send_status = send_message(server, session, &status_msg);

            if (send_status != ERR_STAT::SUCCESS) {
                fprintf(stderr, "[ERROR] Error send status=[%lu]; session_id=[%llu]", send_status, session->client_id);
            }
        } break;
        case MSG_TYPE::SYSTEM: {
            cmd_result_t cmd_stat = operate_command(server, session, msg);

            // send status message clients
            chat_message_t status_msg = create_chat_message(MSG_TYPE::ERROR_MSG);
            status_msg.error_stat = cmd_stat.status_id;
            status_msg.sys_command = cmd_stat.command_id;

            switch (cmd_stat.command_id) {
                case COMMAND::LOG_IN: {
                    if (cmd_stat.status_id == ERR_STAT::SUCCESS) {
                        ERR_STAT broadcast_stat = broadcast_message(server, &status_msg);
                        fprintf(stderr, "[INFO] Broadcast all clients with new log in; session_id=[%llu]\n",
                                    session->client_id) ? broadcast_stat == ERR_STAT::SUCCESS :
                        fprintf(stderr, "[ERROR] Failed to broadcast, status=[%lu]; session_id=[%llu]\n",
                                    broadcast_stat, session->client_id);
                    } else {
                        ERR_STAT send_stat = send_message(server, session, &status_msg);
                        fprintf(stderr, "[INFO] Sent client bad log in; session_id=[%llu]\n",
                                    session->client_id) ? send_stat == ERR_STAT::SUCCESS :
                        fprintf(stderr, "[ERROR] Failed to send status, status=[%lu]; session_id=[%llu]\n",
                                    send_stat, session->client_id);
                    }
                } break;
                case COMMAND::REGISTR: {
                    ERR_STAT send_stat = send_message(server, session, &status_msg);
                    fprintf(stderr, "[INFO] Sent client register success; session_id=[%llu]\n",
                                    session->client_id) ? send_stat == ERR_STAT::SUCCESS :
                    fprintf(stderr, "[ERROR] Failed to send regiter status; session_id=[%llu]\n",
                                    session->client_id);
                }
                default:
                    break;
            }
        } break;
        case MSG_TYPE::ERROR_MSG: {
            ERR_STAT err_code = msg->error_stat;

            //accidently disconnected user
            if (err_code == ERR_STAT::CONNECTION_LOST) {
                // log out first
                if (session->state == CLIENT_STATE::LOGGED_IN) {
                    bool logged_out = change_state_session(server->active_sessions, session->client_id, CLIENT_STATE::CONNECTED);
                    if (logged_out) {
                        fprintf(stderr, "[INFO] Clinet logged out; session_id=[%llu]\n", session->client_id);
                        //send_all_logged_out(server, session);
                    } else {
                        fprintf(stderr, "[FATAL] Client failed logged out; session_id=[%llu]\n, session_state=[%u]\n",
                            session->client_id, session->state);
                        return ERR_STAT::BAD_OPERATE;
                    }
                }

                // disconnect from server
                bool disconnect = change_state_session(server->active_sessions, session->client_id, CLIENT_STATE::DISCONNECTED);
                if (disconnect) {
                    fprintf(stderr, "[INFO] Client disconnected; session id=[%llu]\n", session->client_id);
                    uv_handle_t* on_close_handle = (uv_handle_t*)session->client_stream;
                    uv_handle_set_data(on_close_handle, (void*)session);
                    call_async_close((uv_handle_t*)session->client_stream);
                }
                else {
                    fprintf(stderr, "[FATAL] Client failed disconnect; session_id=[%llu]\n", session->client_id);
                    return ERR_STAT::BAD_OPERATE;
                }
            }
            else
                fprintf(stderr, "Unexpected error code %llu\n", err_code);
        } break;

        default: {
            fprintf(stderr, "Unexpected msg_type: %llu\n", msg_type);
            fprintf(stderr, "Request ignored...\n\n");

            return ERR_STAT::BAD_REQUEST;
        }
        break;
    }

    return ERR_STAT::SUCCESS;
}

int close_all_active_connections(server_t* server) {
    //poping all messages out of incomming
    while (!server->incoming_msg->empty()) {
        server->incoming_msg->delete_head();
    }

    //closing all connections on interface finalization
    size_t closing_req = 0;
    for (int i = 0; i < server->client_base->size; i ++) {
        base_client_t* client_b = server->client_base->base + i;

        if (client_b->status == STATUS::ONLINE) {
            call_async_close((uv_handle_t*)client_b->client_stream);
            closing_req += 1;

            // does not change anything, but to underlie connection closing
            client_b->status = STATUS::OFFLINE;
        }
    }
    size_t closing_confirms = 0;

    //recieve connection close confirmation
    //check if request amount == closed confirms
    //otherwise interface won't finalize
    while (closing_confirms != closing_req) {
        server->incoming_msg->wait();

        chat_message_t msg = server->incoming_msg->get_head()->data;
        server->incoming_msg->delete_head();

        if (msg.sys_command == COMMAND::ON_CLOSE)
            closing_confirms ++;

        fprintf(stderr, "\rConnection close [%lu/%lu]", closing_confirms, closing_req);
        fflush(stdout);

        sleep(1); //just for now (beautiful output)
    }

    fprintf(stderr, "\n");

    return 0;
}

void* start_interface(void* args) {
    thread_args_t* thr_args = (thread_args_t*)args;
    server_t* server = (server_t*)(thr_args->owner_struct);

    char server_name[NAME_SIZE] = {0};

    fprintf(stderr, "<<<<<<     Server start operate incomming msgs     >>>>>>\n");
    while (server->status == SERVER_STATUS::UP) {
        //block until smth will apear to read ---> avoid useless "while" looping
        server->incoming_msg->wait();

        chat_message_t msg = server->incoming_msg->get_head()->data;
        server->incoming_msg->delete_head();

        uint64_t client_id = hash_djb2_data((void*)(&msg.client_endpoint), sizeof(uv_stream_t*));
        CLIENT_STATE session_state = get_state_session(server->active_sessions, client_id);

        if (session_state == CLIENT_STATE::UNDEFINED ||
            session_state == CLIENT_STATE::DISCONNECTED)
        {
            fprintf(stderr, "[FATAL] Drop request, session state undefined or disconnected; session_id=[%llu]\n", client_id);
            continue;
        }

        chat_session_t* session = get_current_session(server->active_sessions, client_id);
        fprintf(stderr, "[INFO] New request; session_id=[%llu]\n", session->client_id);
        ERR_STAT request_stat = operate_request(server, session, &msg);

        if (request_stat != ERR_STAT::SUCCESS)
        {
            fprintf(stderr, "[FATAL] Request operation returned not zero code; session_id=[%llu]\n", session->client_id);
        }
    }

    close_all_active_connections(server);

    fprintf(stderr, "<<<<<<     Interface finalization     >>>>>>\n");

    pthread_exit(NULL);
    return NULL;
}

//------------------------------------------------------------------------------------------------------//


void run_server_backend(server_t* server, const char* ip_address, size_t port) {
    pthread_t   pid[THREAD_NUM];
    thread_args_t args = {};

    args.owner_struct = (void*)server;
    args.owner        = OWNER::SERVER;
    args.port         = port;
    args.ip           = ip_address;

    #ifdef DEBUG_VERSION
        InitLogFile("../log_server.txt");
    #endif

    pthread_create(&pid[0], NULL, start_networking,(void*)&args);
    pthread_create(&pid[1], NULL, start_interface, (void*)&args);

    for (int i = THREAD_NUM - 1; i >= 0; i--)
        pthread_join(pid[i], NULL);
}