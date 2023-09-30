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

#define ALIVE_STAT(owner) static_cast<int>(owner->alive_stat)
#define FATAL_ERROR_OCCURED() fprintf(stdin, "exit")

void* governer(void* args);

enum class ALIVE_STAT : size_t{
    ALIVE = 1,
    DEAD = 0
};

typedef struct lock_ {
    sembuf* sem_array;
    size_t   semid;
}lock;

enum class OWNER;
typedef struct args__{
    OWNER owner;
    void* owner_struct;
    const char* ip;
    size_t port;

}thread_args;

#define TRY_READ_INCOMING( owner )                                            \
do{                                                                    \
    sembuf sem_buf = {};                                               \
    sem_buf.sem_op = -1;                                              \
    sem_buf.sem_flg = 0;                                             \
    sem_buf.sem_num = 0;                                                \
                                                                           \
    int stat_ = semop(owner->sem_lock->semid, &sem_buf, 1);                         \
    if (stat_)                                                                    \
        perror("read incomming error");                        \
                                                                       \
}while(0)

#define TRY_READ_OUTGOING( owner )                                                    \
do{                                                                                 \
    sembuf sem_buf = {};                                               \
    sem_buf.sem_op = -1;                                              \
    sem_buf.sem_flg = 0;                                             \
    sem_buf.sem_num = 1;                                                \
                                                                           \
    int stat_ = semop(owner->sem_lock->semid, &sem_buf, 1);                         \
    if (stat_)                                                                  \
        perror("read outgoing error");                                  \
                                                                                      \
}while(0)

#define RELEASE_INCOMING( owner )                                                \
do{                                                                    \
    sembuf sem_buf = {};                                               \
    sem_buf.sem_op  = 1;                                              \
    sem_buf.sem_flg = 0;                                             \
    sem_buf.sem_num = 0;                                                \
                                                                           \
    int stat_ = semop(owner->sem_lock->semid, &sem_buf, 1);                   \
    if (stat_)                                                               \
        perror("incoming release error");                                     \
                                                                       \
}while(0)

#define RELEASE_OUTGOING( owner )                                                \
do{                                                                    \
    sembuf sem_buf = {};                                               \
    sem_buf.sem_op  = 1;                                              \
    sem_buf.sem_flg = 0;                                             \
    sem_buf.sem_num = 1;                                                \
                                                                           \
    int stat_ = semop(owner->sem_lock->semid, &sem_buf, 1);                         \
    if (stat_)                                                               \
        perror("outgoing release error");                                             \
}while(0)

#define THREADS_TERMINATE( owner )                                    \
do {                                                           \
    owner->alive_stat = ALIVE_STAT::DEAD;                     \
                                                               \
    RELEASE_OUTGOING(owner);                                           \
    RELEASE_INCOMING(owner);                                           \
                                                                   \
    call_async_stop();                                              \
                                                               \
    return NULL;                                               \
}while(0)   




#endif //THREADS_SAFE