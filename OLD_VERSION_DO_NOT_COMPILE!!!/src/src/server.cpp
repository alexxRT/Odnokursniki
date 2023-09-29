#include "chat_base.h"
#include "messages.h"
#include "server.h"
#include "threads_safe.h"
#include "chat_configs.h"
#include "networking.h"

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
    server->alive_stat = ALIVE_STAT::ALIVE;

    server->sem_lock            = CALLOC(1, lock);
    server->sem_lock->sem_array = CALLOC(SEM_NUM, sembuf);

    server->sem_lock->sem_array->sem_num = SEM_NUM;

    size_t key    = ftok("server.cpp", 0);
    size_t sem_id = semget(key, SEM_NUM, 0777 | IPC_CREAT);

    assert(sem_id > 0 && "Invalid sem id");

    server->sem_lock->semid = sem_id;

    server->incoming_msg = list_create(10, THREAD_MODE::THREAD_SAFE, destroy_chat_message);
    server->outgoing_msg = list_create(10, THREAD_MODE::THREAD_SAFE, destroy_chat_message);

    return server;
}

void destroy_server(server_t* server) {
    destoy_client_base(server->client_base);
    list_destroy(server->incoming_msg);
    list_destroy(server->outgoing_msg);

    FREE(server->sem_lock->sem_array);
    FREE(server->sem_lock);
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
            chat_message_t* msg = create_chat_message(MSG_TYPE::SYSTEM);
            msg->to = client->name_hash;
            msg->write_message(msg, buf->buf);

            list_insert_right(server->outgoing_msg, 0, msg);
            RELEASE_OUTGOING(server);
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

    chat_message_t* msg = create_chat_message(MSG_TYPE::SYSTEM);
    msg->to = client->name_hash;
    msg->write_message(msg, buf->buf);

    list_insert_right(server->outgoing_msg, 0, msg);
    RELEASE_OUTGOING(server);

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
            fprintf(stderr, "Client full request: %s", msg->msg_body);
        break;
    } 
    return ERR_STAT::BAD_REQUEST;
}

ERR_STAT operate_request(server_t* server, chat_message_t* msg) {
    assert(msg != NULL);

    MSG_TYPE msg_type = msg->msg_type;

    switch (msg_type) {
        case MSG_TYPE::TXT_MSG:
            list_insert_right(server->outgoing_msg, 0, msg);
            RELEASE_OUTGOING(server);
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

        default: {
            fprintf(stderr, "Unexpected msg_type: %d\n", msg_type);
            fprintf(stderr, "Request ignored...\n\n");

            return ERR_STAT::BAD_REQUEST;
        }
        break;
    }

    return ERR_STAT::SUCCESS;
}

void* start_interface(void* args) {
    thread_args* thr_args = (thread_args*)args;
    server_t* server = (server_t*)(thr_args->owner_struct);

    while (ALIVE_STAT(server)) {
        chat_message_t* msg = NULL;

        //block until smth will apear to read ---> avoid useless "while" looping
        TRY_READ_INCOMING(server);

        //thread_safe
        list_delete_left(server->incoming_msg, 0, msg);

        ERR_STAT request_stat = ERR_STAT::SUCCESS;
        if (msg)
            request_stat = operate_request(server, msg);

        if (request_stat != ERR_STAT::SUCCESS) {
            chat_message_t* err_msg = create_chat_message(MSG_TYPE::ERROR_MSG);
            err_msg->error_stat = request_stat;
            
            list_insert_right(server->outgoing_msg, 0, msg);
            RELEASE_OUTGOING(server);
        }
    }

    fprintf(stderr, "<<<<<<     Interface finalization     >>>>>>\n");

    pthread_exit(NULL);
    return NULL;
}

//------------------------------------------------------------------------------------------------------//


void run_server_backend(server_t* server, const char* ip_address, size_t port) {
    pthread_t   pid[THREAD_NUM];
    thread_args args = {};

    args.ip   = ip_address;
    args.port = port;
    args.owner_struct = (void*)server;
    args.owner = OWNER::SERVER;

    pthread_create(&pid[0], NULL, start_networking,(void*)&args);
    pthread_create(&pid[1], NULL, start_interface, (void*)&args);
    pthread_create(&pid[2], NULL, start_sender,    (void*)&args);
    pthread_create(&pid[3], NULL, governer,        (void*)&args);

    for (int i = THREAD_NUM - 1; i >= 0; i--)
        pthread_join(pid[i], NULL);

}
