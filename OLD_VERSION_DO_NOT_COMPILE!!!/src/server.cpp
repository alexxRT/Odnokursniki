#include "server.h"


int main() {

    server_t* server = create_server(10);
    
    if (server)
        fprintf(stderr, "<<<<<<     Server created successfully     >>>>>>\n\n");

    run_server_backend(server, "127.0.0.1", 8123);

    destroy_server(server);

    fprintf (stderr, "\n<<<<<<     Server destroyed     >>>>>>\n\n");

    return 0;
}