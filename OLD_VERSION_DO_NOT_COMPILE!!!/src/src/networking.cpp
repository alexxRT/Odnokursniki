#include "networking.h"
#include "chat_configs.h"
#include "memory.h"
#include "messages.h"

static connect_t* connection = NULL;

void create_connection(OWNER owner) {
    connection = CALLOC(1, connect_t);
    connection -> owner = owner;

    return;
}

void destroy_connection() {
    #ifdef DEBUG_VERSION
        fprintf(stderr, "BE AWARE OF OWNER ADRESS DESTRUCTION\n");
        fprintf(stderr, "FREE() OWNER STRUCT FIRST\n");
    #endif

    FREE(connection);
}

//needs refactor and testing
// void async_finish_networking(uv_async_s* async_args) {

//     uv_stream_t* closing_endpoint = (uv_stream_t*)(async_args->data);

//     if (uv_loop_alive(async_args->loop)) {
//         uv_read_stop(closing_endpoint);
//         uv_stop(async_args->loop);
//         uv_loop_close(async_args->loop);
//     }
// }

// void async_send_message(uv_async_s* async_args) {
//     uv_buf_t buf_to_send = {};
//     size_t   msg_size = 0;
//     size_t   offset = 0;

//     uv_stream_t* write_endpoint = (uv_stream_t*)(async_args->data);
//     offset += sizeof(uv_stream_t*);

//     memcpy(&msg_size, async_args->data + offset, sizeof(size_t));
//     offset += sizeof(size_t);

//     const char msg[msg_size] = "";
//     memcpy(msg, async_args->data + offset, msg_size);

//     buf_to_send.base = msg;
//     buf_to_send.len  = msg_size;

//     uv_write_t* write_req = CALLOC(1, uv_write_t);
//     uv_write(write_req, write_endpoint, &buf_to_send, 1, on_write); 
// }

// void async_close_connection(uv_async_s* async_args) {
//     uv_handle_t* close_handle = (uv_handle_t*)async_args->data;

//     uv_close(close_handle, on_close_connection);
// }

//-------------------------------------------LIBUV NETWORKING---------------------------------------//


//calbacks
void alloc_cb (uv_handle_t* alloc_handle, size_t suggested_size, uv_buf_t* buf) {
    if (buf->len == 0) {
        buf->base = CALLOC(suggested_size, char);
        buf->len  = suggested_size;
    }
    else {
        buf->base = REALLOC(buf->base, suggested_size, char);
        buf->len  = suggested_size; 
    }

    uv_handle_set_data(alloc_handle, buf->base);
}

//close handle = (uv_handle_t*)(uv_stream_t type)
void on_close_connection(uv_handle_t* close_handle) {
    uv_read_stop((uv_stream_t*)close_handle);
    FREE(close_handle);
    fprintf(stderr, "<<<<<<     Connection closed     >>>>>>\n");
}

void on_read(uv_stream_t* endpoint, ssize_t nread, const uv_buf_t* buf) {
    if (nread > 0) {
        while (nread > 0) {
            MSG_TYPE   msg_type = get_msg_type(buf->base);
            chat_message_t* msg = create_chat_message(msg_type);

            int read_size = (msg->read_message)(msg, buf->base);
            msg->client_endpoint = endpoint;

            if (connection->owner == OWNER::CLIENT) {
                list_insert_right(connection->client->incoming_msg, 0, msg);
                RELEASE_INCOMING(connection->client);
            }
            else {
                list_insert_right(connection->server->incoming_msg, 0, msg);
                RELEASE_INCOMING(connection->server);
            }

            nread -= (read_size + sizeof(MSG_TYPE));
        }
    }
    else if (nread < 0) { //error happend on connection, tear up connection
        fprintf(stderr, "<<<<<<     Error while reading     >>>>>>\n");
        fprintf(stderr, "%s\n\n",   uv_strerror(nread));

        chat_message_t* msg = create_chat_message(MSG_TYPE::ERROR_MSG);
        msg->error_stat = ERR_STAT::CONNECTION_LOST;
        msg->client_endpoint = endpoint;
    
        if (connection->owner == OWNER::CLIENT) {
            list_insert_right(connection->client->outgoing_msg, 0, msg);
            RELEASE_OUTGOING(connection->client);
        }
        else {
            list_insert_right(connection->server->outgoing_msg, 0, msg);
            RELEASE_OUTGOING(connection->client);
        } 
    }

    //nread == 0 ---> do nothing
    return;
}

void on_write(uv_write_t* write_handle, int status) {
    if (status) {
        fprintf(stderr, "<<<<<<     Error while writing     >>>>>>\n");
        fprintf(stderr, "%s\n\n",   uv_strerror(status));

        uv_close((uv_handle_t*)write_handle->send_handle, on_close_connection);
    }

    FREE(write_handle);
}


void on_connect(uv_connect_t* req, int status) {
    sockaddr addr = {};
    int namelen = 0;

    uv_tcp_getsockname((uv_tcp_t*)req->handle, &addr, &namelen);
    sockaddr_in* addr_in = (sockaddr_in*)&addr;

    if (status < 0) {
        fprintf(stderr, "Connection failed\n");
        fprintf(stderr, "Endpoint IP:   %s\n", inet_ntoa(addr_in->sin_addr));
        fprintf(stderr, "Endpoint PORT: %d\n", ntohl(addr_in->sin_port));

        FREE(req);
    }
    else {
        fprintf(stderr, "Connection success\n");
        fprintf(stderr, "Endpoint IP:   %s\n", inet_ntoa(addr_in->sin_addr));
        fprintf(stderr, "Endpoint PORT: %d\n", ntohl(addr_in->sin_port));

        uv_read_start(req->handle, alloc_cb, on_read);

        FREE(req);
    }
}

void on_acceptance(uv_stream_t* server_endpoint, int status) {
    if (status < 0) {
        fprintf (stderr, "Connection error\n");
        return;
    }

    uv_tcp_t* client_endpoint = CALLOC(1, uv_tcp_t);
    uv_tcp_init(server_endpoint->loop, client_endpoint);

    int accept_stat = uv_accept(server_endpoint, (uv_stream_t*)client_endpoint);

    if (accept_stat) {
        sockaddr addr = {};
        int namelen = 0;

        uv_tcp_getpeername(client_endpoint, &addr, &namelen);
        sockaddr_in* addr_in = (sockaddr_in*)&addr;

        fprintf(stderr, "New connection!\n");
        fprintf(stderr, "Endpoint IP:   %s\n", inet_ntoa(addr_in->sin_addr));
        fprintf(stderr, "Endpoint PORT: %d\n", ntohl(addr_in->sin_port));

        uv_read_start((uv_stream_t*)client_endpoint, alloc_cb, on_read);
    }
    else {
        fprintf (stderr, "Error while accepting\n");
        fprintf (stderr, "Closing connection...\n");

        uv_close((uv_handle_t*)client_endpoint, on_close_connection);
    }
}


ERR_STAT run_networking(server_t* server, const char* ip, size_t port) {
    uv_loop_t* event_loop = uv_default_loop();

    sockaddr_in addr  = {};
    uv_tcp_t owner_endpoint = {};

    uv_tcp_init(event_loop, &owner_endpoint);
    uv_ip4_addr(ip, port, &addr);

    int bind_stat = uv_tcp_bind(&owner_endpoint, (const struct sockaddr *)&addr, 0);

    if (bind_stat) {
        fprintf(stderr, "<<<<<     Error on binding     >>>>>>\n");
        fprintf(stderr, "%s\n\n",  uv_strerror(bind_stat));

        return ERR_STAT::BIND_ERR;
    }

    int listen_stat = uv_listen((uv_stream_t *)&owner_endpoint, 1024, on_acceptance);

    if (listen_stat){
        fprintf(stderr, "<<<<<<     Error on litening     >>>>>>\n");
        fprintf(stderr, "%s\n\n",   uv_strerror(listen_stat));

        return ERR_STAT::LISTEN_ERR;
    }

    server->event_loop = event_loop;

    // uv_async_init(event_loop, &connection->close_connection,  async_connection_close);
    // uv_async_init(event_loop, &connection->finish_networking, async_finish_networking);
    // uv_async_init(event_loop, &connection->send_message,      async_send_message);

    int run_stat = 0;//uv_run(event_loop, UV_RUN_DEFAULT);
    
    if (run_stat) {
        fprintf(stderr, "<<<<<<     Error on event loop exit     >>>>>>\n");
        fprintf(stderr, "%s\n\n",   uv_strerror(run_stat));

        return ERR_STAT::LOOP_EXIT_ERROR;
    }

    return ERR_STAT::SUCCESS;
}

ERR_STAT run_networking(client_t* client, const char* ip, size_t port) {
    uv_loop_t* event_loop = uv_default_loop();

    sockaddr_in addr  = {};
    uv_tcp_t owner_endpoint = {};

    uv_tcp_init(event_loop, &owner_endpoint);
    uv_ip4_addr(ip, port, &addr);

    uv_connect_t* req = CALLOC(1, uv_connect_t);
    int connect_stat = uv_tcp_connect(req, &owner_endpoint, (const struct sockaddr *)&addr, on_connect);

    if (connect_stat) {
        fprintf(stderr, "<<<<<<     Error while connecting     >>>>>>");
        fprintf(stderr, "%s\n\n",   uv_strerror(connect_stat));

        return ERR_STAT::CONNECT_ERR;
    }

    client->event_loop = event_loop;

    // uv_async_init(event_loop, &connection->close_connection,  async_connection_close);
    // uv_async_init(event_loop, &connection->finish_networking, async_finish_networking);
    // uv_async_init(event_loop, &connection->send_message,      async_send_message);

    int run_stat = 0; //uv_run(event_loop, UV_RUN_DEFAULT);

    if (run_stat) {
        fprintf(stderr, "<<<<<<     Error on event loop exit     >>>>>>\n");
        fprintf(stderr, "%s\n\n",   uv_strerror(run_stat));

        return ERR_STAT::LOOP_EXIT_ERROR;
    }

    return ERR_STAT::SUCCESS;
}

void* start_networking(void* args) {
    thread_args* backend_params = (thread_args*)args;

    create_connection(backend_params->owner);

    ERR_STAT backend_stat = ERR_STAT::SUCCESS;

    if (connection->owner == OWNER::CLIENT) {
        connection->client = (client_t*)backend_params->owner_struct;
        backend_stat = run_networking(connection->client, backend_params->ip, backend_params->port);
    }
    else {
        connection->server = (server_t*)backend_params->owner_struct;
        backend_stat = run_networking(connection->server, backend_params->ip, backend_params->port);
    }

    fprintf(stderr, "<<<<<<     Networking finalization     >>>>>>\n");

    if (backend_stat != ERR_STAT::SUCCESS) {
        FATAL_ERROR_OCCURED();

        destroy_connection();
        pthread_exit(NULL);
        return NULL;
    }

    destroy_connection();
    pthread_exit(NULL);
    return NULL;
}


//------------------------------------------------------SENDER------------------------------------------//

ERR_STAT send_message(server_t* server, chat_message_t* msg) {

    base_client_t* client = get_client(server->client_base, msg->to);
    if (client) {
        buffer_t* buf = create_buffer(msg->msg_type);
        msg->write_message(msg, buf->buf);

        //on write request//
        uv_write_t* write_req = CALLOC(1, uv_write_t);
        int write_stat = uv_write(write_req, client->client_stream, (uv_buf_t*)buf, 1, on_write);

        if (write_stat) {
            fprintf(stderr, "<<<<<<     Error while writing     >>>>>>\n");
            fprintf(stderr, "%s\n\n",   uv_strerror(write_stat));

            //See error handling TO DO below:
        }

        destroy_type_buffer(buf);
        return ERR_STAT::SUCCESS;
    }


    return ERR_STAT::UNKNOWN_CLIENT; 
}


ERR_STAT send_message(client_t* client, chat_message_t* msg) {
    buffer_t* buf = create_buffer(msg->msg_type);
    msg->write_message(msg, buf->buf);

    //on write request//
    uv_write_t* write_req = CALLOC(1, uv_write_t);
    int write_stat = uv_write(write_req, client->server_dest, (uv_buf_t*)buf, 1, on_write);

    destroy_type_buffer(buf);

    if (write_stat) {
        fprintf(stderr, "<<<<<<     Error while wrinting     >>>>>>\n");
        fprintf(stderr, "%s\n\n",   uv_strerror(write_stat));

        //for now only dump error occured
        //TO DO: extend error handling

        //return ERR_STAT::CONNECTION_LOST;
    }

    return ERR_STAT::SUCCESS;
}


void run_sender(server_t* server) {
    while (ALIVE_STAT(server)) {
        chat_message_t* msg = NULL;

        TRY_READ_OUTGOING(server); //block until recieve something
        
        //thread safe
        list_delete_left(server->outgoing_msg, 0, msg);

        
        ERR_STAT send_stat = ERR_STAT::SUCCESS;
        if (msg)
            send_stat = send_message(server, msg);

        if (send_stat != ERR_STAT::SUCCESS) {
            chat_message_t* err_msg = create_chat_message(MSG_TYPE::ERROR_MSG);
            err_msg->error_stat = send_stat;
            
            list_insert_right(server->incoming_msg, 0, err_msg);
        }
    }
} 

void run_sender(client_t* client) {
    while (ALIVE_STAT(client)) {
        chat_message_t* msg = NULL;

        TRY_READ_OUTGOING(client); //block until recieve something
        
        //thread safe
        list_delete_left(client->outgoing_msg, 0, msg);
        
        ERR_STAT send_stat = ERR_STAT::SUCCESS;
        if (msg)
            send_stat = send_message(client, msg);

        if (send_stat != ERR_STAT::SUCCESS) {
            chat_message_t* err_msg = create_chat_message(MSG_TYPE::ERROR_MSG);
            err_msg->error_stat = send_stat;

            list_insert_right(client->incoming_msg, 0, err_msg);
            RELEASE_INCOMING(client);
        }
    }

}

void* start_sender(void* args) {
    thread_args* thr_args = (thread_args*)args;
    
    if (thr_args->owner == OWNER::SERVER) {
        server_t* server = (server_t*)thr_args->owner_struct;
        run_sender(server);
    }
    else { 
        client_t* client = (client_t*)thr_args->owner_struct;
        run_sender(client);
    }

    fprintf(stderr, "<<<<<<     Sender finalization     >>>>>>\n");

    pthread_exit(NULL);
    return NULL;
}

//--------------------------------------------------------------------------------------------------------//