#include "client.h"



int main () {

    client_t* client = create_client();
    
    run_client_backend(client, 7000, "127.0.0.1");

    destroy_client(client);

    return 0;
}
