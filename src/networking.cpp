#include "networking.h"
#include "client.h"
#include "server.h"
#include "chat_configs.h"
#include "memory.h"

#include <thread>

static connect_t* connection = NULL;
static net_event_t write_event;

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

    chat_session_t* session = (chat_session_t*)uv_handle_get_data((uv_handle_t*)write_endpoint);
    fprintf(stderr, "[INFO] Register write event in loop; session_id=[%llu]\n", session->client_id);
    int write_stat = uv_write((uv_write_t*)write_req, write_endpoint, &write_req->buf, 1, on_write);

    // When write stat < 0, request hasn't been put in event queue, so on_write() never been called!
    if (write_stat < 0) {
        fprintf(stderr, "[ERROR] Failed register on_write event in loop; session_id=[%llu]\n", session->client_id);
        uv_close((uv_handle_t*)write_endpoint, on_close_connection);
        free_write_req(write_req);
        uv_sem_post(&async_write_event.event_sem);
    }
}

void async_close(uv_async_s* async_args) {
    uv_close(async_args->next_closing, on_close_connection);
}


void call_async_write(uv_stream_t* write_dest, buffer_t* buf_to_write) {
    chat_session_t* session = (chat_session_t*)uv_handle_get_data((uv_handle_t*)write_dest);
    fprintf(stderr, "[INFO] Pending write; session_id=[%llu]\n", session->client_id);
    uv_sem_wait(&async_write_event.event_sem);

    write_req_t* write_req = CALLOC(1, write_req_t);
    write_req->buf = uv_buf_init(CALLOC(buf_to_write->capacity, char), buf_to_write->capacity);
    memcpy(write_req->buf.base, buf_to_write->buf, buf_to_write->capacity);

    write_req->endpoint = write_dest;
    async_write_event.event.next_closing = (uv_handle_t*)write_req;

    int send_async = uv_async_send(&async_write_event.event);
    if (send_async < 0) {
        fprintf(stderr, "[FATAL] Failed send async write event; session_id=[%llu]\n", session->client_id);
        free_write_req(write_req);
        uv_sem_post(&async_write_event.event_sem);
    }
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
}

//close handle = (uv_handle_t*)(uv_stream_t type)

// Called for multiple places in code:
// - on_read failed
// - on write failed
// - force call from server operation loop
// -> mismatch data path; invalid session_id

void on_close_connection(uv_handle_t* close_handle) {
    uv_read_stop((uv_stream_t*)close_handle);

    chat_session_t* active_session = (chat_session_t*)uv_handle_get_data(close_handle);
    fprintf(stderr, "[INFO] Terminating active session; session_id=[%llu]\n", active_session->client_id);

    active_session->client_id     = 0;
    active_session->client_name   = 0;
    active_session->client_stream = NULL;
    active_session->state = CLIENT_STATE::DISCONNECTED;

    fprintf(stderr, "[INFO] Session terminated\n");
    FREE(close_handle);
}

void on_read(uv_stream_t* endpoint, ssize_t nread, const uv_buf_t* buf) {
    if (nread > 0) {
        MSG_TYPE  msg_type = get_msg_type(buf->base);
        chat_message_t msg = create_chat_message(msg_type);

        buffer_t* buffer = create_type_buffer(msg_type);
        buffer->buf  = buf->base;
        buffer->size = buffer->capacity;

        size_t read_size = (msg.read_message)(&msg, buffer);
        msg.client_endpoint = endpoint;

        if (connection->owner == OWNER::CLIENT) {
            connection->client->incoming_msg->insert_head(msg);
        }
        else {
            connection->server->incoming_msg->insert_head(msg);
        }
        destroy_type_buffer(buffer);
    }
    else if (nread < 0) { //error happend on connection, tear up connection
        chat_session_t* session = (chat_session_t*)uv_handle_get_data((uv_handle_t*)endpoint);
        fprintf(stderr, "[ERROR] Read failed: EOF; session_id=[%llu]\n", session->client_id);

        uv_close((uv_handle_t*)endpoint, on_close_connection);
    }

    //nread == 0 ---> do nothing
    return;
}

void on_write(uv_write_t* write_handle, int status) {
    chat_session_t* session = (chat_session_t*)uv_handle_get_data((uv_handle_t*)((write_req_t*)write_handle)->endpoint);
    fprintf(stderr, "[INFO:ON_WRITE] on_write invocation; session_id=[%llu]\n", session->client_id);

    if (status) {
        fprintf(stderr, "[ERROR] Write failed: pipe error; session_id=[%llu]\n", session->client_id);
        uv_close((uv_handle_t*)write_handle->send_handle, on_close_connection);
        free_write_req((write_req_t*)write_handle);
        return;
    }

    fprintf(stderr, "[INFO] Write success; session_id=[%llu]\n", session->client_id);
    free_write_req((write_req_t*)write_handle);
    uv_sem_post(&async_write_event.event_sem);
}


void on_connect(uv_connect_t* req, int status) {
    if (status < 0) {
        fprintf(stderr, "[ERROR] Failed to connect, bad status=[%d]\n", status);
        uv_close((uv_handle_t*)req->handle, on_close_connection);
        FREE(req);
        return;
    }

    connection->client->server_dest = (uv_stream_t*)req->handle;
    uint64_t client_id = hash_djb2_data(&connection->client->server_dest, sizeof(uv_stream_t*));
    connection->client->session = create_new_session(connection->client->server_dest, client_id);
    fprintf(stderr, "[INFO] New session created; session_id=[%llu]\n", connection->client->session.client_id);

    uv_handle_set_data((uv_handle_t*)req->handle, (void*)&connection->client->session);
    fprintf(stderr, "[INFO] Session attached to uv_handle; session_id=[%llu]\n", connection->client->session.client_id);

    uv_async_init(connection->client->event_loop, &async_write_event.event, async_send);
    uv_sem_init(&async_write_event.event_sem, 1U);
    // this makes possible to rise callbacks from different threads
    // in thread safe manner
    uv_async_init(connection->client->event_loop, &async_close_connection, async_close);
    uv_async_init(connection->client->event_loop, &async_stop_networking,  async_stop);

    uv_read_start(req->handle, alloc_cb, on_read);

    FREE(req);
}

void on_acceptance(uv_stream_t* server_endpoint, int status) {
    if (status < 0) {
        fprintf (stderr, "[FATAL] Connection error, bad status; status=[%d]\n", status);
        return;
    }

    uv_tcp_t* client_endpoint = CALLOC(1, uv_tcp_t);
    uv_tcp_init(server_endpoint->loop, client_endpoint);

    uint64_t client_id = hash_djb2_data((void*)(&client_endpoint), sizeof(uv_stream_t*));
    chat_session_t* session = add_new_session(connection->server->active_sessions,
                                                 (uv_stream_t*)client_endpoint,
                                                  client_id);
    fprintf(stderr, "[INFO] New session created; session_id=[%llu]\n", session->client_id);

    int accept_stat = uv_accept(server_endpoint, (uv_stream_t*)client_endpoint);
    if (accept_stat) {
        fprintf (stderr, "[FATAL] Failed to accept new connection; session_id=[%llu]\n", session->client_id);
        uv_close((uv_handle_t*)client_endpoint, on_close_connection);
        return;
    }

    uv_handle_t* session_handle = (uv_handle_t*)client_endpoint;
    uv_handle_set_data(session_handle, (void*)session);
    fprintf(stderr, "[INFO] Session attached to uv_handle; session_id=[%llu]\n", session->client_id);

    uv_read_start((uv_stream_t*)client_endpoint, alloc_cb, on_read);
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

    fprintf(stderr, "[INFO] Init libuv callbacks to invoke thread-safe\n");
    int init_send = uv_async_init(connection->server->event_loop, &async_write_event.event, async_send);
    uv_sem_init(&async_write_event.event_sem, 1U);

    int init_close = uv_async_init(connection->server->event_loop, &async_close_connection, async_close);
    int init_stop = uv_async_init(connection->server->event_loop, &async_stop_networking,  async_stop);

    if (init_send != 0 || init_close != 0 || init_stop != 0) {
        fprintf(stderr, "[FATAL] Failed register async event callbacks to invoke event_loop\n");
        return ERR_STAT::LIBUV_ERROR;
    }

    fprintf(stderr, "<<<<<<     Server start listening on port: [%lu]    >>>>>>\n", port);
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

    int run_stat = uv_run(event_loop, UV_RUN_DEFAULT);

    if (run_stat < 0) {
        fprintf(stderr, "<<<<<<     Error on event loop exit     >>>>>>\n");
        fprintf(stderr, "%s\n\n",   uv_strerror(run_stat));

        return ERR_STAT::LOOP_EXIT_ERROR;
    }

    return ERR_STAT::SUCCESS;
}

void* start_networking(void* args) {
    thread_args* backend_params = (thread_args_t*)args;

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
        fprintf(stderr, "Error Network Backend. Error Code: %llu\n", backend_stat);
    }

    destroy_connection();
    pthread_exit(NULL);
    return NULL;
}