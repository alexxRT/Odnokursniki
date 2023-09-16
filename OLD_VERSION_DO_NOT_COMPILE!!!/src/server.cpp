#include "server.h"


int main() {

    server_t* server = create_server(10);

    run_server_backend(server, "127.0.0.1", 7000);

    destroy_server(server);

    return 0;
}