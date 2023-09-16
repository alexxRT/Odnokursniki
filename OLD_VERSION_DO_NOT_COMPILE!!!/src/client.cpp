#include "client.h"



int main () {

    client_t* client = create_client();
    if (client)
        fprintf(stderr, "<<<<<<     Client created successfully     >>>>>>\n\n");

    run_client_backend(client, 7000, "127.0.0.1");

    destroy_client(client);

    fprintf(stderr, "\n<<<<<<     Client destroyed     >>>>>>\n\n");
    return 0;
}
