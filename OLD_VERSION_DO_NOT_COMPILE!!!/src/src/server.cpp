#include "chat_base.h"
#include "messages.h"
#include "server.h"
#include "threads_safe.h"
#include "chat_configs.h"
#include "networking.h"
#include "list_debug.h"
#include <unistd.h>

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

CMD_CODE get_command (char cmd_body[]) {
    size_t offset = sizeof(MSG_TYPE) + NAME_SIZE;

    if (!strncmp(cmd_body + offset, "in$", IN_SIZE)) {
        return CMD_CODE::LOG_IN;
    }

    if(!strncmp(cmd_body + offset, "online$", ONLINE_SIZE)) {
        return CMD_CODE::ONLINE;
    }

    if (!strncmp(cmd_body + offset, "out$", OUT_SIZE)) {
        return CMD_CODE::LOG_OUT;
    }

    if (!strncmp(cmd_body + offset, "reg$", REG_SIZE)) {
        return CMD_CODE::REGISTR;
    }

    if (!strncmp(cmd_body + offset, "chstat$", STAT_SIZE)) {
        return CMD_CODE::CHANGED_STAT;
    }

    return CMD_CODE::UNKNOWN;
}

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

    if (err_code == ERR_STAT::INCORRECT_LOG_IN) {
        const char* err_msg = "Incorrect user name or password";
        fill_body(buf, (void*)err_msg, strlen(err_msg));

        return;
    }
}

server_t* create_server(size_t client_num) {
    server_t* server = CALLOC(1, server_t);
    server->client_base = create_client_base(client_num);
    server->status = SERVER_STATUS::UP;

    server->incoming_msg = new ts_list(10);
    server->outgoing_msg = new ts_list(10);

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
    assert(client != NULL);

    buffer_t* buf = create_buffer(MSG_TYPE::SYSTEM);
    fill_server_message_buffer(buf, 0, CMD_CODE::CHANGED_STAT);

    fill_body(buf, (void*)client->name, NAME_SIZE);

    for (int i = 0; i < server->client_base->size; i ++) {
        base_client_t* client = server->client_base->base + i;

        if (client->status == STATUS::ONLINE) {
            chat_message_t msg = create_chat_message(MSG_TYPE::SYSTEM);
            msg.to = client->name_hash;
            msg.read_message(&msg, buf->buf);

            server->outgoing_msg->insert_head(msg);
        }
    }
    destroy_type_buffer(buf);
}

void send_online_list(server_t* server, base_client_t* client) {
    assert(client != NULL);

    buffer_t* buf = create_buffer(MSG_TYPE::SYSTEM);
    fill_server_message_buffer(buf, 0, CMD_CODE::ONLINE);

    for (int i = 0; i < server->client_base->size; i ++) {
        base_client_t* client = server->client_base->base + i;
        if (client->status == STATUS::ONLINE)
            fill_body(buf, (void*)client->name, NAME_SIZE);
    }

    chat_message_t msg = create_chat_message(MSG_TYPE::SYSTEM);
    msg.to = client->name_hash;
    msg.read_message(&msg, buf->buf);

    server->outgoing_msg->insert_head(msg);

    destroy_type_buffer(buf);
}

ERR_STAT operate_command(server_t* server, CMD_CODE code, chat_message_t* msg) {
    assert(msg != NULL);

    switch(code) {
        case CMD_CODE::LOG_IN: {
            base_client_t* client = log_in_client(server->client_base, msg->msg_body + IN_SIZE);

            if (client) {
                send_all_status_changed(server, client);
                change_status(client);

                fprintf(stderr, "New log in, client: [%s]\n", client->name);
                return ERR_STAT::SUCCESS;
            }

            fprintf(stderr, "BAD LOG IN\n");
            fprintf(stderr, "Client request %s\n\n", msg->msg_body);
            return ERR_STAT::INCORRECT_LOG_IN;
        }
        break;

        case CMD_CODE::LOG_OUT: {
            base_client_t* client = log_out_client(server->client_base, msg->msg_body + OUT_SIZE);

            if (client) {
                change_status(client);
                send_all_status_changed(server, client);

                //closing connection
                call_async_close((uv_handle_t*)client->client_stream);

                fprintf(stderr, "Client [%s] log out\n\n", client->name);
                return ERR_STAT::SUCCESS;
            }
            fprintf(stderr, "BAD LOG OUT. Client was not found\n");
            fprintf(stderr, "Client request: %s\n\n", msg->msg_body);
        }
        break;

        case CMD_CODE::REGISTR: {
            base_client_t* client = registr_client(server->client_base, msg->client_endpoint, msg->msg_body + REG_SIZE);

            if (client) {
                send_all_status_changed(server, client);
                change_status(client);

                fprintf(stderr, "Client [%s] have just registred!\n\n", client->name);
                return ERR_STAT::SUCCESS;
            }
            fprintf(stderr, "BAD REGISTRATION\n");
            fprintf(stderr, "Client request: %s\n\n", msg->msg_body);

            return ERR_STAT::NAME_TAKEN;
        }
        break;

        case CMD_CODE::ONLINE: {
            base_client_t* client = get_client(server->client_base, msg->client_endpoint);
            send_online_list(server, client);
            fprintf(stderr, "Online list sent\n");
        }
        break;

        default:
            fprintf(stderr, "UNKNOWN COMMAND\n");
            fprintf(stderr, "Client full request: %s\n", msg->msg_body);
        break;
    } 
    return ERR_STAT::BAD_REQUEST;
}

ERR_STAT operate_request(server_t* server, chat_message_t* msg) {
    MSG_TYPE msg_type = msg->msg_type;

    switch (msg_type) {
        case MSG_TYPE::TXT_MSG:
            server->outgoing_msg->insert_head(*msg);
        break;

        case MSG_TYPE::SYSTEM: {
            CMD_CODE code     = get_command(msg->msg_body);
            ERR_STAT cmd_stat = operate_command(server, code, msg);

            if (cmd_stat != ERR_STAT::SUCCESS)
                return cmd_stat;
        }
        break;

        case MSG_TYPE::ERROR_MSG: {
            ERR_STAT err_code = msg->error_stat;

            //accidently disconnected user
            if (err_code == ERR_STAT::CONNECTION_LOST) {
                uv_stream_t* client_endpoint = msg->client_endpoint;
                base_client_t* client = get_client(server->client_base, client_endpoint);

                if (client) {
                    change_status(client);
                    send_all_status_changed(server, client);

                    //closing connection
                    call_async_close((uv_handle_t*)client_endpoint);
                }
            }
            else 
                fprintf(stderr, "Unexpected error code %d\n", err_code);
        }
        break;

        case MSG_TYPE::ON_CLOSE: {
            fprintf (stderr, "Connection closed. Confirmed!\n");
        }
        break;

        default: {
            fprintf(stderr, "Unexpected msg_type: %d\n", msg_type);
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

        if (msg.msg_type == MSG_TYPE::ON_CLOSE)
            closing_confirms ++;

        fprintf(stderr, "\rConnection close [%lu/%lu]", closing_confirms, closing_req);
        fflush(stdout);

        sleep(1); //just for now (beautiful output)
    }

    fprintf(stderr, "\n");

    return 0;
}

void* start_interface(void* args) {
    thread_args* thr_args = (thread_args*)args;
    server_t* server = (server_t*)(thr_args->owner_struct);

    while (server->status == SERVER_STATUS::UP) {
        //block until smth will apear to read ---> avoid useless "while" looping
        server->incoming_msg->wait();

        
        chat_message_t msg = server->incoming_msg->get_head()->data;
        server->incoming_msg->delete_head();

        ERR_STAT request_stat = operate_request(server, &msg);

        if (request_stat != ERR_STAT::SUCCESS) {
            fprintf (stderr, "Bad Result On Request Operation\n");

            // if message was recieved from client, reply bad request.
            if (msg.from != 0){
                chat_message_t err_msg = create_chat_message(MSG_TYPE::ERROR_MSG);
                err_msg.error_stat     = request_stat;
                err_msg.to             = err_msg.from;
            
                server->outgoing_msg->insert_head(err_msg);
            }
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
    thread_args args = {};

    args.ip_addr      = ip_address;
    args.port         = port;
    args.owner_struct = (void*)server;
    args.owner        = OWNER::SERVER;

    InitLogFile("../log_server.txt");

    pthread_create(&pid[0], NULL, start_networking,(void*)&args);
    pthread_create(&pid[1], NULL, start_interface, (void*)&args);
    pthread_create(&pid[2], NULL, start_sender,    (void*)&args);

    for (int i = THREAD_NUM - 1; i >= 0; i--)
        pthread_join(pid[i], NULL);
}