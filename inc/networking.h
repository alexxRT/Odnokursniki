#ifndef NETWORKING
#define NETWORKING

#include <uv.h>
#include "messages.h"

// this thread safe variables are used to rise callbacks
// from defferent thread, where is no event loop running
static uv_async_t async_send_message;
static uv_async_t async_close_connection;
static uv_async_t async_stop_networking;

typedef struct event {
    uv_mutex_t event_mutex;
    uv_sem_t   event_sem;
    bool       running_flag;
    uv_async_t event;
} net_event_t;

static net_event_t async_write_event;

extern pthread_mutex_t sessions_mutex;

//callbacks calls, used from different threads
void call_async_write(uv_stream_t* write_dest, buffer_t* buf_to_write);
void call_async_close(uv_handle_t* close_handle);
void call_async_stop ();

typedef struct client_ client_t;
typedef struct server_ server_t;

typedef struct connection_ {
    OWNER owner;
    client_t* client = NULL;
    server_t* server = NULL;
}connect_t;

//main libuv backend function
//TO DO split to little functions
void* start_networking(void* args);

//callbacks
void on_acceptance      (uv_stream_t* server_endpoint, int status);
void on_connect         (uv_connect_t* req, int status);
void on_write           (uv_write_t* write_handle, int status);
void on_read            (uv_stream_t* client_endpoint, ssize_t nread, const uv_buf_t* buf);
void on_close_connection(uv_handle_t* close_handle);
void alloc_cb           (uv_handle_t* alloc_handle, size_t suggested_size, uv_buf_t* buf);


#endif // NETWORKING