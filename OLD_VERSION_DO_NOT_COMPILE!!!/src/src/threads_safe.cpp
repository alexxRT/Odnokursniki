#include "threads_safe.h"
#include "chat_configs.h"
#include "server.h"
#include "client.h"
#include <iostream>
#include <vector>

FILE* THREAD_DUMP = NULL; //fopen("../threads_dump.txt", "a");
const int THR_NAME_SIZE = 10;
const int MAX_THR_NUM   = 10;

enum class UPD_TYPE {
    MUTEX = 1,
    SEMOP = 2,
    THREAD = 3, 
    CREATE_APPROVE = 4
};

enum class ON_UPDATE_ERR {
    SUCCESS         = 0,
    MUTEX_NO_EXIST  = 1, 
    SEMOP_NO_EXIST  = 2,
    THREAD_NO_EXIST = 3,
    THREAD_INIT     = 4,
    THREAD_FAILED   = 5
};

const char* get_status (int status) {
    if (status)
        return "running";
    else 
        return "finished";
}

const char* err_msg(ON_UPDATE_ERR update_err){
    switch (update_err){
        case ON_UPDATE_ERR::MUTEX_NO_EXIST:
            return "No such mutex exists";
        case ON_UPDATE_ERR::SEMOP_NO_EXIST:
            return "No such semop exists";
        case ON_UPDATE_ERR::THREAD_NO_EXIST:
            return "No such thread exist";
        
        default:
            return "Unknown error code";
    }

    return NULL;
}


typedef void* (*thread_func_t)(void*);

typedef struct sem_ {
    size_t      locked_stat;
    size_t      sem_id;
    size_t      lock_line;
    const char* file;
    pthread_t   locked_thread;
    char        locked_thread_name[10];
}sem_t_;

typedef struct mutex_ {
    size_t           locked_stat;
    pthread_mutex_t* mutex;
    size_t           lock_line;
    const char*      file;
    pthread_t        locked_thread;
    char             locked_thread_name[10];
}mut_t;



typedef struct update_sem_ {
    size_t      locked_stat;
    size_t      sem_id;
    size_t      lock_line;
    const char* file;
    pthread_t   locked_thread;
    char        locked_thread_name[10];
}update_sem_t;


typedef struct safe_thread {
    thread_func_t thread_func;
    pthread_t     thread_id;
    size_t        status; // indicate if thread is running or not
    char thread_name[10];

    // contains all mutexes and sems info 
    // that are used in thread
    std::vector<sem_t_> sems;
    std::vector<mut_t> muts;

    // on init, user can specify in which order 
    // threads should be started and finished
    // if no, threads are inited independently
    size_t        start_order;
    size_t        finish_order;
}mthread_t;


typedef mut_t update_mutex_t;
typedef sem_t_ update_semop_t;
typedef mthread_t update_thread_t;

typedef struct update {
    UPD_TYPE type;
    update_mutex_t  mutex_update;
    update_semop_t  semop_update;
    update_thread_t thread_update;
}update_t;

typedef struct queue_ {
    std::mutex            mutex;
    std::vector<update_t> queue;
}ts_queue;

typedef struct thread_master_ {
    std::vector<mthread_t> threads;
    
    std::mutex              update_mutex;
    std::condition_variable update_wait;

    ts_queue* update_queue;
}thread_master_t;

void dump_mutex(mut_t* mutex) {
    assert(mutex != NULL);

    fprintf(THREAD_DUMP, "\tThread locked name [%s]\n", mutex->locked_thread_name);
    fprintf(THREAD_DUMP, "\tMutex file: [%s]\n",        mutex->file);
    fprintf(THREAD_DUMP, "\tMutex line: [%lu]\n",       mutex->lock_line);

    if (mutex->locked_stat)
        fprintf(THREAD_DUMP, "\tMutex LOCKED\n");
    else 
        fprintf(THREAD_DUMP, "\tMutex UNLOCKED\n");

    return;
}

void dump_sem(sem_t_* sem) {
    assert(sem != NULL);

    fprintf(THREAD_DUMP, "\n\tThread locked name [%s]\n", sem->locked_thread_name);
    fprintf(THREAD_DUMP, "\tSemop id: [%lu\n]", sem->sem_id);
    fprintf(THREAD_DUMP, "\tSemop file: [%s]\n",  sem->file);
    fprintf(THREAD_DUMP, "\tSemop line: [%lu]\n", sem->lock_line);

    if (sem->locked_stat)
        fprintf(THREAD_DUMP, "\tSemop LOCKED\n\n");
    else 
        fprintf(THREAD_DUMP, "\tSemop UNLOCKED\n");

    return;
}

void dump_threads (thread_master_t* master) {
    assert(master != NULL);

    for (int i = 0; i < master->threads.size(); i++) {
        mthread_t* thread = &master->threads[i];
        fprintf(THREAD_DUMP, "THREAD [%s]\n", thread->thread_name);
        fprintf(THREAD_DUMP, "STATUS [%s]\n", get_status(thread->status));

        fprintf(THREAD_DUMP, "\n--------------------MUTEX--------------\n");
        for (int i = 0; i < thread->muts.size(); i ++)
            dump_mutex(&thread->muts[i]);
        fprintf(THREAD_DUMP, "\n----------------------------------------\n");

        fprintf(THREAD_DUMP, "\n--------------------SEMOP--------------\n");
        for (int i = 0; i < thread->sems.size(); i ++)
            dump_sem(&thread->sems[i]);
        fprintf(THREAD_DUMP, "\n----------------------------------------\n");

        fprintf(THREAD_DUMP, "\n");
    }
}

mthread_t* get_thread(thread_master_t* master, pthread_t pthread_id) {
    for (int i = 0; i < master->threads.size(); i ++) {
        if (master->threads[i].thread_id == pthread_id)
            return &master->threads[i];
    }

    return NULL;
}

mthread_t* get_thread(thread_master_t* master, char* thread_name) {
    for (int i = 0; i < master->threads.size(); i ++) {
        if (!strncmp(master->threads[i].thread_name, thread_name, strlen(thread_name)))
            return &master->threads[i];
    }

    return NULL;
}

mut_t* get_mutex(mthread_t* thread, pthread_mutex_t* mutex) {
    for (int i = 0; i < thread->muts.size(); i ++) {
        if (thread->muts[i].mutex == mutex)
            return &thread->muts[i];
    }

    return NULL;
}

sem_t_* get_semop(mthread_t* thread, size_t sem_id) {
    for (int i = 0; i < thread->sems.size(); i ++) {
        if (thread->sems[i].sem_id == sem_id)
            return &thread->sems[i];
    }

    return NULL;
}

ON_UPDATE_ERR update_mutex(thread_master_t* master, update_mutex_t update) {
    mthread_t* thread = get_thread(master, update.locked_thread);
    if (!thread)
        return ON_UPDATE_ERR::THREAD_NO_EXIST;

    mut_t* mutex  = get_mutex (thread, update.mutex);
    if (!mutex)
        return ON_UPDATE_ERR::MUTEX_NO_EXIST;

    (*mutex) = update;

    return ON_UPDATE_ERR::SUCCESS;
}

ON_UPDATE_ERR update_semop(thread_master_t* master, update_semop_t update) {
    mthread_t* thread = get_thread(master, update.locked_thread);
    if (!thread)
        return ON_UPDATE_ERR::THREAD_NO_EXIST;

    sem_t_* semop  = get_semop (thread, update.sem_id);
    if (!semop)
        return ON_UPDATE_ERR::SEMOP_NO_EXIST;

    (*semop) = update;

    return ON_UPDATE_ERR::SUCCESS;
}

ON_UPDATE_ERR update_thread(thread_master_t* master, update_thread_t update) {
    mthread_t* thread = get_thread(master, update.thread_name);
    if (!thread)
        return ON_UPDATE_ERR::THREAD_NO_EXIST;

    (*thread) = update;

    return ON_UPDATE_ERR::SUCCESS;
}

void wait_update(thread_master_t* master) {
    while (master->update_queue->queue.empty()) {
        std::unique_lock<std::mutex> ul(master->update_mutex);
        master->update_wait.wait(ul);
    }
}

ON_UPDATE_ERR update(thread_master_t* master) {
    update_t update = {};
    wait_update(master);

    master->update_queue->mutex.lock();
        update = master->update_queue->queue.back();
        if (update.type != UPD_TYPE::CREATE_APPROVE)
            master->update_queue->queue.pop_back();
    master->update_queue->mutex.unlock();

    ON_UPDATE_ERR err = ON_UPDATE_ERR::SUCCESS;

    switch (update.type) {
        case UPD_TYPE::MUTEX:
            err = update_mutex(master, update.mutex_update);
            break;
        
        case UPD_TYPE::SEMOP:
            err = update_semop(master, update.semop_update);
            break;

        case UPD_TYPE::THREAD:
            err = update_thread(master, update.thread_update);
            break;

        default:
            break;
    }

    return err;
}

ON_UPDATE_ERR wait_init(thread_master_t* master) {
    update_t update = {};
    size_t   is_init = 0;

    while (!is_init) {
        wait_update(master);

        master->update_queue->mutex.lock();
            for (int i = 0; i < master->update_queue->queue.size(); i ++) {
                update = master->update_queue->queue[i];

                if (update.type == UPD_TYPE::CREATE_APPROVE) {
                    master->update_queue->queue.pop_back();
                    is_init = 1;

                    break;
                }
            }
        master->update_queue->mutex.unlock();
    }
    
    return ON_UPDATE_ERR::SUCCESS;
}

void send_update(ts_queue *update_queue, update_t update) {
    update_queue->mutex.lock();
        update_queue->queue.push_back(update);
    update_queue->mutex.unlock();
}

void add_mutex(mthread_t* thread, pthread_mutex_t* mutex) {
    mut_t mutex_to_add = {};

    mutex_to_add.mutex = mutex;
    mutex_to_add.locked_stat = 0;

    thread->muts.push_back(mutex_to_add);
}

void add_semop(mthread_t* thread, size_t sem_id) {
    sem_t_ semop_to_add = {};

    semop_to_add.sem_id = sem_id;
    semop_to_add.locked_stat = 0;

    thread->sems.push_back(semop_to_add);
}

void add_thread(thread_master_t* master, char* name, thread_func_t func,  size_t start_order  = -1,  \
                                                                          size_t finish_order = -1) {
    mthread_t thread = {};

    size_t name_sz = std::min((int)strlen(name), THR_NAME_SIZE);
    memcpy(thread.thread_name, name , name_sz);

    thread.thread_id = 0;

    thread.thread_func = func;
    thread.status = 0;

    thread.start_order  = start_order;
    thread.finish_order = finish_order;

    master->threads.push_back(thread);

    return;
}

thread_master_t* create_master() {
    thread_master_t* master = new thread_master_t;

    return master;
}

void destroy_master(thread_master_t* master){
    delete master;

    return;
}

bool compare_threads(mthread_t thr1, mthread_t thr2) {
    return (thr1.start_order < thr2.start_order);
}


void run_threads(thread_master_t* master) {
    assert(!master);

    std::sort(master->threads.begin(), master->threads.end(), compare_threads);

    for (int i = 0; i < master->threads.size(); i ++) {
        pthread_t thread_id = 0;
        pthread_create(&thread_id, NULL, master->threads[i].thread_func, NULL);

        //if start order was specified, wait until success creation 
        if (master->threads[i].start_order <= MAX_THR_NUM)
            ON_UPDATE_ERR err = wait_init(master);

        fprintf(stderr, "Thread [%s\n] successfully initilized\n", master->threads[i].thread_name);
    }
}

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