#include "networking.h"
#include "chat_configs.h"
#include "threads_safe.h"
#include "memory.h"
#include "messages.h"

static connect_t* connection = NULL;

void create_connection(OWNER owner) {
    connection = CALLOC(1, connect_t);
    connection -> owner = owner;
}

void destroy_connection() {
    #ifdef DEBUG_VERSION
        fprintf(stderr, "BE AWARE OF OWNER ADRESS DESTRUCTION\n");
        fprintf(stderr, "FREE() OWNER STRUCT FIRST\n");
    #endif

    FREE(connection);
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
    fprintf(stderr, "Connection closed\n");
}

void on_read(uv_stream_t* client_endpoint, ssize_t nread, const uv_buf_t* buf) {
    if (nread > 0) {
        while (nread > 0) {
            MSG_TYPE   msg_type = get_msg_type(buf->base);
            chat_message_t* msg = create_chat_message(msg_type);

            int read_size = (msg->read_message)(msg, buf->base);
            msg->client_endpoint = client_endpoint;

            if (connection->owner == OWNER::CLIENT)
                RECV(connection->client, msg);
            else 
                RECV(connection->server, msg);

            nread -= (read_size + sizeof(MSG_TYPE));
        }
    }
    else if (nread < 0) { //error happend on connection, tear up connection
        fprintf(stderr, "Error while reading, nread: %ld\n", nread);

        chat_message_t* msg = create_chat_message(MSG_TYPE::ERROR_MSG);
        msg->client_endpoint = client_endpoint;

        if (connection->owner == OWNER::CLIENT)
            SEND(connection->client, msg);
        else 
            SEND(connection->server, msg);
    }

    //nread == 0 ---> do nothing
    return;
}

void on_write(uv_write_t* write_handle, int status) {
    if (status) {
        fprintf(stderr, "Error while writing: %d\n", write_handle->error);
        fprintf(stderr, "Closing connection...\n");

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

    sockaddr addr = {};
    int namelen = 0;

    uv_tcp_getsockname(client_endpoint, &addr, &namelen);
    sockaddr_in* addr_in = (sockaddr_in*)&addr;

    fprintf(stderr, "New connection!\n");
    fprintf(stderr, "Endpoint IP:   %s\n", inet_ntoa(addr_in->sin_addr));
    fprintf(stderr, "Endpoint PORT: %d\n", ntohl(addr_in->sin_port));

    int accept_stat = uv_accept(server_endpoint, (uv_stream_t*)client_endpoint);

    if (accept_stat) {
        fprintf(stderr, "Client accepted\n");
        uv_read_start((uv_stream_t*)client_endpoint, alloc_cb, on_read);
    }
    else {
        fprintf (stderr, "Error while accepting\n");
        fprintf (stderr, "Closing connection...\n");

        uv_close((uv_handle_t*)client_endpoint, on_close_connection);
    }
}


void* start_networking_backend(void* args) {
    thread_args* backend_params = (thread_args*)args;

    create_connection(backend_params->owner);
    if (connection->owner == OWNER::CLIENT)
        connection->client = (client_t*)backend_params->owner_struct;
    else
        connection->server = (server_t*)backend_params->owner_struct;

    uv_loop_t* event_loop = uv_default_loop();

    struct sockaddr_in addr  = {};
    uv_tcp_t owner_endpoint = {};

    uv_tcp_init(event_loop, &owner_endpoint);
    uv_ip4_addr(backend_params->ip, backend_params->port, &addr);

    if (connection->owner == OWNER::SERVER) {
        int bind_stat = uv_tcp_bind(&owner_endpoint, (const struct sockaddr *)&addr, 0);

        if (bind_stat) {
            fprintf(stderr, "Bind error: %d\n", bind_stat);
            fprintf(stdin, "exit");

            destroy_connection();
            pthread_exit(NULL);
            return NULL;
        }

        int listen_stat = uv_listen((uv_stream_t *)&owner_endpoint, 1024, on_acceptance);

        if (listen_stat){
            fprintf(stderr, "Listen new connections error: %d\n", listen_stat);
            fprintf(stdin, "exit");

            destroy_connection();
            pthread_exit(NULL);
            return NULL;
        }
    }
    else {
        uv_connect_t* req = CALLOC(1, uv_connect_t);
        int connect_stat = uv_tcp_connect(req, &owner_endpoint, (const struct sockaddr *)&addr, on_connect);
    }
    if (connection->owner == OWNER::SERVER)
        connection->server->event_loop = event_loop;
    else 
        connection->client->event_loop = event_loop;

    int run_stat = uv_run(event_loop, UV_RUN_DEFAULT);
    
    if (run_stat) {
        fprintf(stderr, "Event loop error: %d\n", run_stat);
        fprintf(stdin, "exit");

        destroy_connection();
        pthread_exit(NULL);
        return NULL;
    }

    fprintf(stderr, "Server finalization\n");

    destroy_connection();
    pthread_exit(NULL);
    return NULL;
}