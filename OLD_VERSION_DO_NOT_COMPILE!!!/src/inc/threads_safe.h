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

static pthread_mutex_t lock_incoming;
static pthread_mutex_t lock_outgoing;

int  init_incoming_lock();
int  init_outgoing_lock();
void destroy_incoming_lock();
void destroy_outgoing_lock();

#define LOCK_INCOMING( owner )                                                \
do {                                                                   \
    pthread_mutex_lock(&lock_incoming);                                 \
                                                                       \
} while(0)

#define LOCK_OUTGOING( owner )                                                \
do {                                                                   \
    pthread_mutex_lock(&lock_outgoing);                                 \
} while(0)

#define UNLOCK_INCOMING( owner )                                              \
do {                                                                   \
    pthread_mutex_unlock(&lock_incoming);                                   \
}while(0)

#define UNLOCK_OUTGOING( owner )                                              \
do {                                                                   \
    pthread_mutex_unlock(&lock_outgoing);                                 \
}while(0)

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
  /*  if (uv_loop_alive(owner->event_loop)) {                   \
                                                             \
        uv_stop(owner->event_loop);                           \
        uv_loop_close(owner->event_loop);                     \
    }                      */                                    \
                                                               \
    return NULL;                                               \
}while(0)

#define INSERT_OUTGOING(owner, msg)                                  \
do{                                                             \
    LOCK_OUTGOING(owner);                                            \
        list_insert_right(owner->outgoing_msg, 0, msg);        \
        RELEASE_OUTGOING(owner);                                        \
    UNLOCK_OUTGOING(owner);                                          \
                                                                \
}while(0)                                                       \

#define INSERT_INCOMMING(owner, msg)                                          \
do{                                                               \
    LOCK_INCOMING(owner);                                              \
        list_insert_right(owner->incoming_msg, 0, msg);          \
        RELEASE_INCOMING(owner);                                          \
    UNLOCK_INCOMING(owner);                                            \
}while(0)    

#define POP_OUTGOING(owner, msg)                                      \
do {                                                                  \
    LOCK_OUTGOING(owner);                                             \
        if (owner->outgoing_msg->size) {                              \
            msg = PREV(owner->outgoing_msg->buffer)->data;           \
            list_delete_left(owner->outgoing_msg, 0);                \
        }                                                             \
    UNLOCK_OUTGOING(owner);                                           \
}                                                                      \
while(0)                                                              \

#define POP_INCOMMING(owner, msg)                                     \
do {                                                                  \
    LOCK_INCOMING(owner);                                             \
        if (owner->outgoing_msg->size) {                              \
            msg = PREV(owner->outgoing_msg->buffer)->data;           \
            list_delete_left(owner->outgoing_msg, 0);                \
        }                                                             \
    UNLOCK_INCOMING(owner);                                           \
}                                                                      \
while(0)                                                              \



#endif //THREADS_SAFE