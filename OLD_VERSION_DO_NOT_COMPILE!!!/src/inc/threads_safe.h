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


#define LOCK_INCOMING()                                                \
do {                                                                   \
    server->sem_lock->sem_array->sem_op  = -1;                         \
    server->sem_lock->sem_array->sem_flg = 0;                          \
                                                                       \
    semop(0, server->sem_lock->sem_array, server->sem_lock->semid);    \
                                                                       \
} while(0)

#define LOCK_OUTGOING()                                                \
do {                                                                   \
    server->sem_lock->sem_array->sem_op  = -1;                         \
    server->sem_lock->sem_array->sem_flg = 0;                          \
                                                                       \
    semop(1, server->sem_lock->sem_array, server->sem_lock->semid);    \
                                                                       \
} while(0)

#define UNLOCK_INCOMING()                                              \
do {                                                                   \
    server->sem_lock->sem_array->sem_op  = 1;                          \
    server->sem_lock->sem_array->sem_flg = 0;                          \
                                                                       \
    semop(0, server->sem_lock->sem_array, server->sem_lock->semid);    \
                                                                       \
}while(0)

#define UNLOCK_OUTGOING()                                              \
do {                                                                   \
    server->sem_lock->sem_array->sem_op  = 1;                          \
    server->sem_lock->sem_array->sem_flg = 0;                          \
                                                                       \
    semop(1, server->sem_lock->sem_array, server->sem_lock->semid);    \
                                                                       \
}while(0)

#define TRY_READ_INCOMING()                                            \
do{                                                                    \
    server->sem_lock->sem_array->sem_op  = -1;                         \
    server->sem_lock->sem_array->sem_flg =  0;                         \
                                                                       \
    semop(2, server->sem_lock->sem_array, server->sem_lock->semid);    \
                                                                       \
}while(0)

#define TRY_READ_OUTGOING()                                            \
do{                                                                    \
    server->sem_lock->sem_array->sem_op  = -1;                         \
    server->sem_lock->sem_array->sem_flg =  0;                         \
                                                                       \
    semop(3, server->sem_lock->sem_array, server->sem_lock->semid);    \
                                                                       \
}while(0)

#define PUSH_INCOMING()                                                \
do{                                                                    \
    server->sem_lock->sem_array->sem_op  = 1;                          \
    server->sem_lock->sem_array->sem_flg = 0;                          \
                                                                       \
    semop(2, server->sem_lock->sem_array, server->sem_lock->semid);    \
                                                                       \
}while(0)

#define PUSH_OUTGOING()                                                \
do{                                                                    \
    server->sem_lock->sem_array->sem_op  = 1;                          \
    server->sem_lock->sem_array->sem_flg = 0;                          \
                                                                       \
    semop(3, server->sem_lock->sem_array, server->sem_lock->semid);    \
                                                                       \
}while(0)

#define THREADS_TERMINATE()                                    \
do {                                                           \
    server->alive_stat = ALIVE_STAT::DEAD;                     \
                                                               \
    PUSH_OUTGOING();                                           \
    PUSH_INCOMING();                                           \
                                                               \
    if (uv_loop_alive(server->event_loop)) {                   \
        uv_stop(server->event_loop);                           \
        uv_loop_close(server->event_loop);                     \
    }                                                          \
                                                               \
    return NULL;                                               \
}while(0)