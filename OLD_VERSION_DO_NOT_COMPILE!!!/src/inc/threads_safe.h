#ifndef THREADS_SAFE
#define THREADS_SAFE


#include <unistd.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <fcntl.h>


typedef struct sembuf sembuf_;

typedef struct lock_ {
    sembuf_* sem_array;
    size_t   semid;
}lock;

enum class OWNER;
typedef struct args__{
    OWNER owner;
    void* owner_struct;
    const char* ip;
    size_t port;

}thread_args;

#define LOCK_INCOMING( owner )                                                \
do {                                                                   \
    owner->sem_lock->sem_array->sem_op  = -1;                         \
    owner->sem_lock->sem_array->sem_flg = 0;                          \
                                                                       \
    semop(0, owner->sem_lock->sem_array, owner->sem_lock->semid);    \
                                                                       \
} while(0)

#define LOCK_OUTGOING( owner )                                                \
do {                                                                   \
    owner->sem_lock->sem_array->sem_op  = -1;                         \
    owner->sem_lock->sem_array->sem_flg = 0;                          \
                                                                       \
    semop(1, owner->sem_lock->sem_array, owner->sem_lock->semid);    \
                                                                       \
} while(0)

#define UNLOCK_INCOMING( owner )                                              \
do {                                                                   \
    owner->sem_lock->sem_array->sem_op  = 1;                          \
    owner->sem_lock->sem_array->sem_flg = 0;                          \
                                                                       \
    semop(0, owner->sem_lock->sem_array, owner->sem_lock->semid);    \
                                                                       \
}while(0)

#define UNLOCK_OUTGOING( owner )                                              \
do {                                                                   \
    owner->sem_lock->sem_array->sem_op  = 1;                          \
    owner->sem_lock->sem_array->sem_flg = 0;                          \
                                                                       \
    semop(1, owner->sem_lock->sem_array, owner->sem_lock->semid);    \
                                                                       \
}while(0)

#define TRY_READ_INCOMING( owner )                                            \
do{                                                                    \
    owner->sem_lock->sem_array->sem_op  = -1;                         \
    owner->sem_lock->sem_array->sem_flg =  0;                         \
                                                                       \
    semop(2, owner->sem_lock->sem_array, owner->sem_lock->semid);    \
                                                                       \
}while(0)

#define TRY_READ_OUTGOING( owner )                                            \
do{                                                                    \
    owner->sem_lock->sem_array->sem_op  = -1;                         \
    owner->sem_lock->sem_array->sem_flg =  0;                         \
                                                                       \
    semop(3, owner->sem_lock->sem_array, owner->sem_lock->semid);    \
                                                                       \
}while(0)

#define PUSH_INCOMING( owner )                                                \
do{                                                                    \
    owner->sem_lock->sem_array->sem_op  = 1;                          \
    owner->sem_lock->sem_array->sem_flg = 0;                          \
                                                                       \
    semop(2, owner->sem_lock->sem_array, owner->sem_lock->semid);    \
                                                                       \
}while(0)

#define PUSH_OUTGOING( owner )                                                \
do{                                                                    \
    owner->sem_lock->sem_array->sem_op  = 1;                          \
    owner->sem_lock->sem_array->sem_flg = 0;                          \
                                                                       \
    semop(3, owner->sem_lock->sem_array, owner->sem_lock->semid);    \
                                                                       \
}while(0)

#define THREADS_TERMINATE( owner )                                    \
do {                                                           \
    owner->alive_stat = ALIVE_STAT::DEAD;                     \
                                                               \
    PUSH_OUTGOING(owner);                                           \
    PUSH_INCOMING(owner);                                           \
                                                               \
    if (uv_loop_alive(owner->event_loop)) {                   \
        uv_stop(owner->event_loop);                           \
        uv_loop_close(owner->event_loop);                     \
    }                                                          \
                                                               \
    return NULL;                                               \
}while(0)

#define SEND(owner, msg)                                               \
do{                                                             \
    LOCK_OUTGOING(owner);                                            \
        list_insert_right(owner->outgoing_msg, 0, msg);        \
        PUSH_OUTGOING(owner);                                        \
    UNLOCK_OUTGOING(owner);                                          \
                                                                \
}while(0)                                                       \

#define RECV(owner, msg)                                          \
do{                                                               \
    LOCK_INCOMING(owner);                                              \
        list_insert_right(owner->incoming_msg, 0, msg);          \
        PUSH_INCOMING(owner);                                          \
    UNLOCK_INCOMING(owner);                                            \
}while(0)    

#endif //THREADS_SAFE