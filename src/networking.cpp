#include "networking.h"
#include "client.h"
#include "server.h"
#include "chat_configs.h"
#include "memory.h"

#include <thread>

static connect_t* connection = NULL;
pthread_mutex_t sessions_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct write_req {
    uv_write_t   uv_write_handle;
    uv_buf_t     buf;
    chat_session_t* session;
    uint64_t session_id;
} write_req_t;

static ThreadSafeList<write_req_t*> pending_writes(10);

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
void async_send(uv_async_s*);

void async_stop(uv_async_s* async_args) {
    async_send(async_args);
    uv_walk(async_args->loop, [](uv_handle_t* handle, void*) {
        if (!uv_is_closing(handle))
            uv_close(handle, handle->type == UV_TCP ? on_close_connection : NULL);
    }, NULL);
}

void async_send(uv_async_s*) {
    write_req_t* wr;
    while (pending_writes.pop_oldest(&wr)) {
        chat_session_t* session = wr->session;
        uv_stream_t* stream = session->client_stream;
        if (session->client_id == wr->session_id && stream && !uv_is_closing((uv_handle_t*)stream)) {
            if (!wr->buf.base)
                uv_close((uv_handle_t*)stream, on_close_connection);
            else if (uv_write(&wr->uv_write_handle, stream, &wr->buf, 1, on_write) == 0)
                continue;
            else
                uv_close((uv_handle_t*)stream, on_close_connection);
        }
        if (wr->buf.base)
            free_write_req(wr);
        else
            FREE(wr);
    }
}

// Callers hold sessions_mutex while recording the destination. A disconnected
// session slot can be reused before the loop services the async notification.
static void queue_write(uv_stream_t* stream, buffer_t* buffer) {
    write_req_t* wr = CALLOC(1, write_req_t);
    wr->session = (chat_session_t*)stream->data;
    wr->session_id = wr->session->client_id;
    if (buffer) {
        wr->buf = uv_buf_init(CALLOC(buffer->size, char), buffer->size);
        memcpy(wr->buf.base, buffer->buf, buffer->size);
    }
    pending_writes.insert_head(wr);
    uv_async_send(&async_write_event.event);
}

void call_async_write(uv_stream_t* stream, buffer_t* buffer) {
    queue_write(stream, buffer);
}

void call_async_close(uv_handle_t* handle) {
    queue_write((uv_stream_t*)handle, NULL);
}

void call_async_stop() {
    uv_async_send(&async_stop_networking);
}

//-------------------------------------------LIBUV NETWORKING---------------------------------------//


//calbacks
void alloc_cb (uv_handle_t* alloc_handle, size_t suggested_size, uv_buf_t* buf) {
    chat_session_t* session = (chat_session_t*)alloc_handle->data;
    // Read one type prefix, then exactly one payload, across as many TCP reads as needed.
    char* dest = (char*)&session->incoming_type;
    size_t size = sizeof(MSG_TYPE);
    if (session->incoming_frame) {
        dest = session->incoming_frame->buf;
        size = session->incoming_frame->capacity;
    }
    *buf = uv_buf_init(dest + session->bytes_read, size - session->bytes_read);
}

//close handle = (uv_handle_t*)(uv_stream_t type)

// Called for multiple places in code:
// - on_read failed
// - on write failed
// - force call from server operation loop
// -> mismatch data path; invalid session_id

void on_close_connection(uv_handle_t* close_handle) {
    chat_session_t* session = (chat_session_t*)close_handle->data;
    if (session) {
        pthread_mutex_lock(&sessions_mutex);
        bool notify = connection->owner == OWNER::SERVER && session->state == CLIENT_STATE::LOGGED_IN;
        if (session->incoming_frame)
            destroy_type_buffer(session->incoming_frame);
        *session = {};
        if (connection->owner == OWNER::CLIENT) {
            chat_message_t roster = create_chat_message(MSG_TYPE::SYSTEM);
            roster.sys_command = COMMAND::ONLINE;
            connection->client->incoming_msg->insert_head(roster);
        }
        if (notify) {
            chat_message_t event = create_chat_message(MSG_TYPE::SYSTEM);
            event.sys_command = COMMAND::ON_CLOSE;
            connection->server->incoming_msg->insert_head(event);
        }
        pthread_mutex_unlock(&sessions_mutex);
    }
    FREE(close_handle);
}

void on_read(uv_stream_t* endpoint, ssize_t nread, const uv_buf_t*) {
    chat_session_t* session = (chat_session_t*)endpoint->data;
    if (nread < 0) {
        uv_close((uv_handle_t*)endpoint, on_close_connection);
        return;
    }
    session->bytes_read += nread;
    if (!session->incoming_frame) {
        if (session->bytes_read < sizeof(MSG_TYPE))
            return;
        if (session->incoming_type < MSG_TYPE::SYSTEM || session->incoming_type > MSG_TYPE::ERROR_MSG) {
            uv_close((uv_handle_t*)endpoint, on_close_connection);
            return;
        }
        session->incoming_frame = create_type_buffer(session->incoming_type);
        return;
    }
    buffer_t* frame = session->incoming_frame;
    if (session->bytes_read == frame->capacity) {
        frame->size = session->bytes_read;
        chat_message_t msg = create_chat_message(frame->type);
        msg.read_message(&msg, frame);
        msg.client_endpoint = endpoint;
        msg.session_id = session->client_id;
        if (connection->owner == OWNER::CLIENT)
            connection->client->incoming_msg->insert_head(msg);
        else
            connection->server->incoming_msg->insert_head(msg);
        destroy_type_buffer(frame);
        session->incoming_frame = NULL;
        session->bytes_read = 0;
    }
}

void on_write(uv_write_t* req, int status) {
    if (status < 0 && !uv_is_closing((uv_handle_t*)req->handle))
        uv_close((uv_handle_t*)req->handle, on_close_connection);
    free_write_req((write_req_t*)req);
}

void on_connect(uv_connect_t* req, int status) {
    if (status < 0) {
        fprintf(stderr, "[ERROR] Failed to connect, bad status=[%d]\n", status);
        uv_close((uv_handle_t*)req->handle, on_close_connection);
        uv_sem_post(&connection->client->backend_ready);
        FREE(req);
        return;
    }

    uv_stream_t* server_endpoint = (uv_stream_t*)req->handle;
    uint64_t client_id = hash_djb2_data(&server_endpoint, sizeof(uv_stream_t*));
    connection->client->session.client_id = client_id;
    connection->client->session.client_stream = server_endpoint;
    connection->client->session.state = CLIENT_STATE::CONNECTED;
    fprintf(stderr, "[INFO] Session state changed to connected; session_id=[%llu]\n", connection->client->session.client_id);

    uv_handle_set_data((uv_handle_t*)req->handle, (void*)&connection->client->session);
    fprintf(stderr, "[INFO] Session attached to uv_handle; session_id=[%llu]\n", connection->client->session.client_id);

    uv_read_start(req->handle, alloc_cb, on_read);
    uv_sem_post(&connection->client->backend_ready);

    FREE(req);
}

void on_acceptance(uv_stream_t* server_endpoint, int status) {
    if (status < 0) {
        fprintf (stderr, "[FATAL] Connection error, bad status; status=[%d]\n", status);
        return;
    }

    uv_tcp_t* client_endpoint = CALLOC(1, uv_tcp_t);
    uv_tcp_init(server_endpoint->loop, client_endpoint);

    static uint64_t next_session_id = 0;
    uint64_t client_id = ++next_session_id;
    pthread_mutex_lock(&sessions_mutex);
    chat_session_t* session = add_new_session(connection->server->active_sessions,
                                                 (uv_stream_t*)client_endpoint,
                                                  client_id);
    pthread_mutex_unlock(&sessions_mutex);
    if (!session) {
        uv_accept(server_endpoint, (uv_stream_t*)client_endpoint);
        uv_close((uv_handle_t*)client_endpoint, on_close_connection);
        return;
    }
    client_endpoint->data = session;
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

    int init_stop = uv_async_init(connection->server->event_loop, &async_stop_networking,  async_stop);

    if (init_send != 0 || init_stop != 0) {
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
    uv_loop_t* event_loop = CALLOC(1, uv_loop_t);
    uv_loop_init(event_loop);
    uv_tcp_t* server_endpoint = CALLOC(1, uv_tcp_t);
    uv_tcp_init(event_loop, server_endpoint);
    client->event_loop  = event_loop;
    uv_async_init(event_loop, &async_write_event.event, async_send);
    uv_async_init(event_loop, &async_stop_networking, async_stop);

    sockaddr_in addr  = {};
    uv_ip4_addr(ip, port, &addr);
    uv_connect_t* req = CALLOC(1, uv_connect_t);

    connection->client->session = create_new_session(NULL, 0);
    connection->client->session.state = CLIENT_STATE::DISCONNECTED;

    int connect_stat = uv_tcp_connect(req, server_endpoint, (const struct sockaddr *)&addr, on_connect);

    if (connect_stat) {
        fprintf(stderr, "<<<<<<     Error while connecting     >>>>>>");
        fprintf(stderr, "%s\n\n",   uv_strerror(connect_stat));

        uv_close((uv_handle_t*)server_endpoint, on_close_connection);
        FREE(req);
        uv_sem_post(&client->backend_ready);
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
        fprintf(stderr, "[INFO] Networking stopped; client backend exit\n");
    }
    else {
        connection->server = (server_t*)backend_params->owner_struct;
        backend_stat = run_networking(connection->server, backend_params->ip, backend_params->port);
        fprintf(stderr, "[INFO] Networking stopped; server backend exit\n");
    }

    if (backend_stat != ERR_STAT::SUCCESS) {
        fprintf(stderr, "Error Network Backend. Error Code: %llu\n", backend_stat);
    }

    uv_loop_close(backend_params->owner == OWNER::CLIENT ? connection->client->event_loop : connection->server->event_loop);
    destroy_connection();
    pthread_exit(NULL);
    return NULL;
}