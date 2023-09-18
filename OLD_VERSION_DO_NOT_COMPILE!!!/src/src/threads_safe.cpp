#include "threads_safe.h"
#include "chat_configs.h"
#include "server.h"
#include "client.h"

//simply finish server application on command "exit"
void* governer(void* args) {
    char command[NAME_SIZE];

    fflush(stdin);
    fscanf(stdin, "%s", command);

    thread_args* thr_args = (thread_args*)args;

    if (!strncmp("exit", command, strlen("exit"))) {
        if (thr_args->owner == OWNER::SERVER) {
            server_t* server = (server_t*)thr_args->owner_struct;
            THREADS_TERMINATE(server);
        }
        else if (thr_args->owner == OWNER::CLIENT){
            client_t* client = (client_t*)thr_args->owner_struct;
            THREADS_TERMINATE(client);
        }
        else 
            fprintf (stderr, "Unknown owner: %d\n", thr_args->owner);
    }
    else {
        fprintf (stderr, "<<<<<<     Governer terminated ACCIDENTLY     >>>>>>\n");
        fprintf (stderr, "!!!Non of synchro on exit was performed!!!\n\n");
    }
    
    return NULL;
}