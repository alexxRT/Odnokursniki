#ifndef THREADS_SAFE
#define THREADS_SAFE

#include <errno.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <pthread.h>
#include "list.h"
#include "chat_configs.h"
#include "networking.h"


enum class OWNER;
typedef struct args__{
    OWNER owner;
    void* owner_struct;
    const char* ip;
    size_t port;

}thread_args;





#endif //THREADS_SAFE