#ifndef NETWORKING
#define NETWORKING

#include "threads_safe.h"
#include "server.h"
#include "client.h"
#include <uv.h>

//TO DO 
//write forward declaration to avoid whole header include
// 



typedef struct connection_ {
    OWNER owner;
    client_t* client = NULL;
    server_t* server = NULL;

    uv_async_t finish_networking;
    uv_async_t send_message;
    uv_async_t close_connection;
}connect_t;

//main libuv backend function
//TO DO split to little functions
void* start_networking(void* args);

//sender function
void* start_sender(void* args);


//callbacks
void on_acceptance      (uv_stream_t* server_endpoint, int status);
void on_connect         (uv_connect_t* req, int status);
void on_write           (uv_write_t* write_handle, int status);
void on_read            (uv_stream_t* client_endpoint, ssize_t nread, const uv_buf_t* buf);
void on_close_connection(uv_handle_t* close_handle);
void alloc_cb           (uv_handle_t* alloc_handle, size_t suggested_size, uv_buf_t* buf);



#endif // NETWORKING