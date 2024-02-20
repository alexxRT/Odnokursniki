#include "networking.h"
#include "client.h"
#include "server.h"
#include "chat_configs.h"
#include "threads_safe.h"
#include "memory.h"

static connect_t* connection = NULL;

typedef struct write_req {
    uv_write_t   uv_write_handle;
    uv_buf_t     buf;
    uv_stream_t* endpoint;
} write_req_t;

void free_write_req(write_req_t* wr) {
    FREE(wr->buf.base);
    FREE(wr);
}

void create_connection(OWNER owner) {
    connection = CALLOC(1, connect_t);
    connection -> owner = owner;

    return;
}

void destroy_connection() {
    FREE(connection);
}

//needs refactor and testing

// thread safe callback calls
void async_stop(uv_async_s* async_args) {
    if (uv_loop_alive(async_args->loop)) {
        uv_stop(async_args->loop);
        uv_loop_close(async_args->loop);
    }
}

void async_send(uv_async_s* async_args) {
    write_req_t*  write_req = (write_req_t*)async_args->next_closing;
    uv_stream_t*  write_endpoint = write_req->endpoint;

    //fprintf(stderr, "write endpoint before write: %p\n", write_endpoint);
    int write_stat = uv_write((uv_write_t*)write_req, write_endpoint, &write_req->buf, 1, on_write);

    // When write stat < 0, request hasn't been put in event queue, so on_write() never been called!
    if (write_stat < 0) {
        fprintf(stderr, "Error while initing writing request\n");

        uv_close((uv_handle_t*)write_endpoint, on_close_connection);
        free_write_req(write_req);
    }

    fprintf(stderr, "Msg Write Success\n");
}

void async_close(uv_async_s* async_args) {
    uv_close(async_args->next_closing, on_close_connection);
}


void call_async_write(uv_stream_t* write_dest, buffer_t* buf_to_write) {
    write_req_t* write_req = CALLOC(1, write_req_t);

    write_req->buf = uv_buf_init(CALLOC(buf_to_write->capacity, char), buf_to_write->capacity);
    memcpy(write_req->buf.base, buf_to_write->buf, buf_to_write->capacity);

    write_req->endpoint = write_dest;

    async_send_message.next_closing = (uv_handle_t*)write_req;

    uv_async_send(&async_send_message);
}

void call_async_close(uv_handle_t* close_handle) {
    async_close_connection.next_closing = close_handle;

    uv_async_send(&async_close_connection);
}

void call_async_stop() {
    uv_async_send(&async_stop_networking);
}

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

    chat_message_t close_confirm_msg = create_chat_message(MSG_TYPE::SYSTEM);
    close_confirm_msg.sys_command = COMMAND::ON_CLOSE;

    if (connection->owner == OWNER::SERVER)
        connection->server->incoming_msg->insert_head(close_confirm_msg);
    else if (connection->owner == OWNER::CLIENT)
        connection->client->incoming_msg->insert_head(close_confirm_msg);

    fprintf(stderr, "<<<<<<     Connection closed     >>>>>>\n");
}

void on_read(uv_stream_t* endpoint, ssize_t nread, const uv_buf_t* buf) {
    if (nread > 0) {
        MSG_TYPE   msg_type = get_msg_type(buf->base);
        chat_message_t msg = create_chat_message(msg_type);

        buffer_t* buffer = create_type_buffer(msg_type); 
        buffer->buf  = buf->base;
        buffer->size = buffer->capacity;

        // fprintf(stderr, "%lu msg type recieved!\n", msg_type);
        // fprintf(stderr, "recieved %lu bytes\n", buf->len);
        // fprintf(stderr, "buffer capacity %lu\n", buffer->capacity);

        size_t read_size = (msg.read_message)(&msg, buffer);
        msg.client_endpoint = endpoint;

        if (connection->owner == OWNER::CLIENT) {
            connection->client->incoming_msg->insert_head(msg);
        }
        else {
            connection->server->incoming_msg->insert_head(msg);
        }

        destroy_type_buffer(buffer);

        #ifdef DEBUG_VERSION
            fprintf(stderr, "Num bytes read from socket %lu\n", nread);
        #endif
    }
    else if (nread < 0) { //error happend on connection, tear up connection
        fprintf(stderr, "<<<<<<     Error while reading     >>>>>>\n");
        fprintf(stderr, "%s\n\n",   uv_strerror(nread));

        chat_message_t msg = create_chat_message(MSG_TYPE::ERROR_MSG);
        msg.error_stat = ERR_STAT::CONNECTION_LOST;
        msg.client_endpoint = endpoint;

        uv_close((uv_handle_t*)endpoint, on_close_connection);
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

    // from libuv src code
    // if (req->error == 0) {
    //     if (req->bufs != req->bufsml)
    //         free(req->bufs);
    //     req->bufs = NULL;
    // }
    // and fundomental one:
    //  req->bufs = req->bufsml;
    //  if (nbufs > ARRAY_SIZE(req->bufsml))
    //     req->bufs = malloc(nbufs * sizeof(bufs[0]));
    // So NO FREE() can be performed on bufs!!!

    free_write_req((write_req_t*)write_handle);

    //fprintf(stderr, "on_write() was performed\n");
}


void on_connect(uv_connect_t* req, int status) {
    struct sockaddr_in addr = {};
    int namelen = 0;

    uv_tcp_getpeername((uv_tcp_t*)req->handle, (struct sockaddr*)&addr, &namelen);

    if (status < 0) {
        #ifdef DEBUG_VERSION
            fprintf(stderr, "Connection failed\n");
            fprintf(stderr, "Server IP:   %s\n", inet_ntoa(addr.sin_addr));
        #endif

        uv_close((uv_handle_t*)req->handle, on_close_connection);

        FREE(req);
    }
    else {
        #ifdef DEBUG_VERSION
            fprintf(stderr, "Connection success\n");
            fprintf(stderr, "Server IP:   %s\n", inet_ntoa(addr.sin_addr));
        #endif

        // this makes possible to rise callbacks from different threads 
        // in thread safe manner
        uv_async_init(connection->client->event_loop, &async_close_connection, async_close);
        uv_async_init(connection->client->event_loop, &async_stop_networking,  async_stop);
        uv_async_init(connection->client->event_loop, &async_send_message,     async_send);

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

    if (!accept_stat) {
        #ifdef DEBUG_VERSION
            struct sockaddr_in addr = {0};
            int namelen = 0;
            uv_tcp_getpeername(client_endpoint, (struct sockaddr*)&addr, &namelen);

            fprintf(stderr, "New connection!\n");
            fprintf(stderr, "IP address:   %s\n", inet_ntoa(addr.sin_addr));
        #endif

        // this makes possible to rise callbacks from different threads 
        // in thread safe manner
        uv_async_init(connection->server->event_loop, &async_close_connection, async_close);
        uv_async_init(connection->server->event_loop, &async_stop_networking,  async_stop);
        uv_async_init(connection->server->event_loop, &async_send_message,     async_send);

        uv_read_start((uv_stream_t*)client_endpoint, alloc_cb, on_read);
    }
    else {
        fprintf (stderr, "Error On Accept, %s\n", uv_strerror(accept_stat));
        uv_close((uv_handle_t*)client_endpoint, on_close_connection);
    }
}


ERR_STAT run_networking(server_t* server, const char* ip, size_t port) {
    uv_loop_t* event_loop = uv_default_loop();
    server->event_loop = event_loop;

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

    int run_stat = uv_run(event_loop, UV_RUN_DEFAULT);
    
    if (run_stat < 0) {
        fprintf(stderr, "<<<<<<     Error on event loop exit     >>>>>>\n");
        fprintf(stderr, "%s\n\n",   uv_strerror(run_stat));

        return ERR_STAT::LOOP_EXIT_ERROR;
    }

    return ERR_STAT::SUCCESS;
}

ERR_STAT run_networking(client_t* client, const char* ip, size_t port) {
    uv_loop_t* event_loop = uv_default_loop();
    uv_tcp_t* server_endpoint = CALLOC(1, uv_tcp_t);
    uv_tcp_init(event_loop, server_endpoint);
    client->event_loop  = event_loop;

    sockaddr_in addr  = {};
    uv_ip4_addr(ip, port, &addr);
    uv_connect_t* req = CALLOC(1, uv_connect_t);

    int connect_stat = uv_tcp_connect(req, server_endpoint, (const struct sockaddr *)&addr, on_connect);

    if (connect_stat) {
        fprintf(stderr, "<<<<<<     Error while connecting     >>>>>>");
        fprintf(stderr, "%s\n\n",   uv_strerror(connect_stat));

        return ERR_STAT::CONNECT_ERR;
    }

    client->server_dest = (uv_stream_t*)server_endpoint;

    int run_stat = uv_run(event_loop, UV_RUN_DEFAULT);

    if (run_stat < 0) {
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
        backend_stat = run_networking(connection->client, backend_params->ip_addr, backend_params->port);
    }
    else {
        connection->server = (server_t*)backend_params->owner_struct;
        backend_stat = run_networking(connection->server, backend_params->ip_addr, backend_params->port);
    }

    fprintf(stderr, "<<<<<<     Networking finalization     >>>>>>\n");

    if (backend_stat != ERR_STAT::SUCCESS) {
        fprintf(stderr, "Error Network Backend. Error Code: %llu\n", backend_stat);
    }

    destroy_connection();
    pthread_exit(NULL);
    return NULL;
}