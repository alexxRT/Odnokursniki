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

void send_online_list(server_t* server) {
    chat_message_t msg = create_chat_message(MSG_TYPE::SYSTEM);
    msg.sys_command = COMMAND::ONLINE;
    size_t position = 0;
    static_assert(MAX_SESSIONS_COUNT * NAME_SIZE <= MSG_SIZE);
    for (size_t i = 0; i < MAX_SESSIONS_COUNT; i++) {
        chat_session_t* session = server->active_sessions + i;
        if (session->state == CLIENT_STATE::LOGGED_IN) {
            base_client_t* user = get_client(server->client_base, session->client_name);
            memcpy(msg.msg_body + position, user->name, NAME_SIZE);
            position += NAME_SIZE;
        }
    }
    broadcast_message(server, &msg);
}

cmd_result_t operate_command(server_t* server, chat_session_t* session, chat_message_t* msg) {
    assert(server  && "[FATAL] server is NULL on operate_command");
    assert(session && "[FATAL] session is NULL on operate_command");
    assert(msg     && "[FATAL] msg is NULL on operate_command");

    COMMAND code = msg->sys_command;

    if (code == COMMAND::REGISTR || code == COMMAND::LOG_IN) {
        char name[NAME_SIZE + 1] = {};
        memcpy(name, msg->msg_body, NAME_SIZE);
        if (session->state != CLIENT_STATE::CONNECTED || !name[0] || !msg->msg_body[NAME_SIZE])
            return {ERR_STAT::BAD_REQUEST, code};
        if (get_current_session_by_name(server->active_sessions, server->client_base->hash_client(name)))
            return {ERR_STAT::NAME_TAKEN, code};
    }

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
            call_async_close((uv_handle_t*)session->client_stream);
            return {ERR_STAT::SUCCESS, code};
        }
        case COMMAND::ONLINE: {
            if (session->state == CLIENT_STATE::LOGGED_IN)
                send_online_list(server);
            return {ERR_STAT::SUCCESS, code};
        }
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
            if (session->state != CLIENT_STATE::LOGGED_IN)
                return ERR_STAT::BAD_LOGIN;
            base_client_t* sender = get_client(server->client_base, session->client_name);
            memcpy(msg->from, sender->name, NAME_SIZE);
            char recipient[NAME_SIZE + 1] = {};
            memcpy(recipient, msg->to, NAME_SIZE);
            // find addressant session
            uint64_t addressant_id = server->client_base->hash_client(recipient);
            chat_session_t* addressant_session = get_current_session_by_name(server->active_sessions, addressant_id);
            base_client_t* recipient_user = get_client(server->client_base, addressant_id);
            if (!recipient_user || strncmp(recipient_user->name, recipient, NAME_SIZE))
                addressant_session = NULL;
            ERR_STAT send_status = addressant_session ? ERR_STAT::SUCCESS : ERR_STAT::CLIENT_OFFLINE;

            // check if online, otherwise drop message for now
            if (send_status == ERR_STAT::CLIENT_OFFLINE) {
                fprintf(stderr, "[INFO] Client=[%llu] is offline, unreachable\n", addressant_id);
                fprintf(stderr, "[ERROR] Unable to send message to=[%.8s]; session_id=[%llu]\n", msg->to, session->client_id);
            } else {
                fprintf(stderr, "[INFO] Retrnsmitting message: from=[%.8s], to=[%.8s]\n", msg->from, msg->to);
                send_status = send_message(server, addressant_session, msg);
            }

            chat_message_t status_msg = create_chat_message(MSG_TYPE::ERROR_MSG);
            memcpy(status_msg.to, msg->to, NAME_SIZE);
            memcpy(status_msg.msg_body, msg->msg_body, MSG_SIZE);
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

            send_message(server, session, &status_msg);
            if ((cmd_stat.command_id == COMMAND::LOG_IN || cmd_stat.command_id == COMMAND::REGISTR) &&
                cmd_stat.status_id == ERR_STAT::SUCCESS) {
                session->state = CLIENT_STATE::LOGGED_IN;
                send_online_list(server);
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

        chat_message_t msg = {};
        if (!server->incoming_msg->pop_oldest(&msg))
            continue;
        pthread_mutex_lock(&sessions_mutex);
        if (!msg.session_id && msg.sys_command == COMMAND::ON_CLOSE) {
            send_online_list(server);
            pthread_mutex_unlock(&sessions_mutex);
            continue;
        }
        chat_session_t* session = get_current_session(server->active_sessions, msg.session_id);
        if (!session) {
            pthread_mutex_unlock(&sessions_mutex);
            continue;
        }
        uint64_t client_id = session->client_id;
        ERR_STAT request_stat = operate_request(server, session, &msg);
        pthread_mutex_unlock(&sessions_mutex);

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
    pthread_t   pid[2];
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

    for (int i = 1; i >= 0; i--)
        pthread_join(pid[i], NULL);
}