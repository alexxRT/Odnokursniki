#include "threads_safe.h"
#include "chat_configs.h"
#include "server.h"
#include "client.h"
#include <iostream>
#include <vector>
#include <stdlib.h>
#include <thread.h>

FILE* THREAD_DUMP = NULL; //fopen("../threads_dump.txt", "a");
const int THR_NAME_SIZE = 10;
const int MAX_THR_NUM   = 10;

enum class LOCKED_STAT {
    LOCKED   = 1,
    UNLOCKED = 0
};

#define LOCK_STAT( mutex )              \
do {                                    \
    if (mutex->try_lock()) {            \
        mutex->unlock();                \
        return LOCKED_STAT::UNLOCKED;   \
    }                                   \
                                        \
    return LOCKED_STAT::LOCKED;         \
                                        \
}while(0)                               \

#define RUNNING_THREAD_EXISTS( master ) master->num_running_threads

enum class UPD {
    CREATE_APPROVE   = 1, 
    READY_TO_JOIN    = 2,
    DUMP_THREADS     = 3,
    ALLOW_JOIN       = 4,
    EXIT_ALL_THREADS = 5,
};

enum class ON_UPDATE_ERR {
    SUCCESS         = 0,
    JOIN_FAILED     = 1,
    THREAD_NO_EXIST = 3,
    THREAD_INIT     = 4,
    THREAD_FAILED   = 5,
    NO_THREAD_EXIST = 6,
    FINISH_UPDATE   = 7 
};

enum class THREAD_STATUS {
    RUNNING         = 0,
    LOCKED          = 1,
    FINISHED        = 2,
};

const char* get_status (THREAD_STATUS status) {
    if (status == THREAD_STATUS::RUNNING)
        return "running";
    else if (status == THREAD_STATUS::FINISHED)
        return "finished";
    else if (status == THREAD_STATUS::LOCKED)
        return "locked";
    else 
        return "unknown stat";
}

const char* err_msg(ON_UPDATE_ERR update_err){
    switch (update_err){
        case ON_UPDATE_ERR::MUTEX_NO_EXIST:
            return "No such mutex exists";
        case ON_UPDATE_ERR::THREAD_NO_EXIST:
            return "No such thread exist";
        
        default:
            return "Unknown error code";
    }

    return NULL;
}

typedef struct notification_ {
    UPD type;
    pthread_t thread_id;
}notify_t;

typedef struct queue_ {
    //to lock queue while accessing
    std::mutex            mutex;
    std::vector<notify_t> queue;

    //to avoid busy loop while waiting update
    std::mutex              update_mutex;
    std::condition_variable update_var;
}ts_queue;

typedef void* (*thread_func_t)(void*);

typedef struct condition_var_ {
    LOCKED_STAT             locked_stat;
    std::condition_variable var;
    std::mutex              var_mutex;
    size_t                  lock_line;
    const char*             file;
}cond_var_t;

typedef struct safe_thread {
    thread_func_t thread_func;
    pthread_t     thread_id;
    THREAD_STATUS status;      // indicate if thread is running or not
    size_t interrupted;

    // this queue is used to send
    // updates to master thread
    ts_queue*     notify_queue;

    // this queue is used to recieve
    // updates from master thread
    ts_queue*     recieve_notify;
    
    // is used to pass arguments to the thread on start
    void* thread_args;

    // each thread has wait part
    // its needed to interrupt thread
    cond_var_t* blocking_var;

    // this mutex is used to update 
    // thread structure in ts way
    std::mutex update_self;

    // on init, user can specify in which order 
    // threads should be started and finished
    // if no, threads are inited independently
    size_t        start_order;
    size_t        finish_order;

    //simple char string for convinient identification
    char          thread_name[10];
}mthread_t;


typedef struct thread_master_ {
    std::vector<mthread_t> threads;
    size_t num_running_threads;

    ts_queue* update_queue;
}thread_master_t;


bool compare_threads(mthread_t thr1, mthread_t thr2) {
    return (thr1.start_order < thr2.start_order);
}


//------------------------------------------DUMP FUNCTIONS--------------------------------------//
void dump_blocking_var(cond_var_t* cv) {
    assert(cv != NULL);

    fprintf(THREAD_DUMP, "\tThread locked name [%s]\n", cv->locked_thread_name);
    fprintf(THREAD_DUMP, "\tCondition variable file: [%s]\n",        cv->file);
    fprintf(THREAD_DUMP, "\tCondition variable line: [%lu]\n",       cv->lock_line);

    if (cv->locked_stat)
        fprintf(THREAD_DUMP, "\tCondition vavriable LOCKED\n");
    else 
        fprintf(THREAD_DUMP, "\tCondition variable UNLOCKED\n");

    return;
}


void dump_threads (thread_master_t* master) {
    assert(master != NULL);

    for (int i = 0; i < master->threads.size(); i++) {
        mthread_t* thread = &master->threads[i];
        fprintf(THREAD_DUMP, "THREAD [%s]\n", thread->thread_name);
        fprintf(THREAD_DUMP, "STATUS [%s]\n", get_status(thread->status));

        fprintf(THREAD_DUMP, "\n--------------------CONDITION VARIABLE--------------\n");
            dump_blocking_var(thread->blocking_var);
        fprintf(THREAD_DUMP, "\n----------------------------------------------------\n");

        fprintf(THREAD_DUMP, "\n");
    }
}
//-----------------------------------------------------------------------------------------------------//



//---------------------------------------------------CREATE FUNCTIONS---------------------------------------------------//


thread_master_t* create_master() {
    thread_master_t* master = new thread_master_t;
    master->num_running_threads = 0;

    return master;
}

void destroy_master(thread_master_t* master){
    delete master;

    return;
}

void create_blocking_var(thread_t* thread) {
    std::scoped_lock(thread->update_self);
        thread->blocking_var = new cond_var_t;
        thread->blocking_var->lock_stat = LOCKED_STAT::UNLOCKED;

    return;
}

void create_thread(thread_master_t* master, char* name, thread_func_t func, void* args, size_t start_order  = -1,  \
                                                                                     size_t finish_order = -1) {
    mthread_t thread = {};

    size_t name_sz = std::min((int)strlen(name), THR_NAME_SIZE);
    memcpy(thread.thread_name, name , name_sz);

    thread.thread_args = args;
    thread.thread_id = 0;

    thread.thread_func = func;
    thread.status = 0;

    thread.start_order  = start_order;
    thread.finish_order = finish_order;

    master->threads.push_back(thread);

    return;
}

//--------------------------------------------------------------------------------------------------------------------//



//--------------------------------------------------SYNCHRO METHODS---------------------------------------------------//


ON_UPDATE_ERR wait_init(thread_master_t* master) {
    assert(master);

    size_t init_stat = 0;
    notify_t notify = {};

    std::unique_lock<std::mutex> lock (master->update_queue->mutex); 
        std::vector<notify_t>::iterator it = master->update_queue->queue.begin();
    lock.unlock();

    while (!init_stat) {
        wait_update(master);

        lock.lock();
            notify_t notify = master->update_queue->queue.back();

            for (int i = 0; i < master->threads.size(); i ++) {
                notify = master->update_queue->queue[i];

                if (notify.type == UPD::CREATE_APPROVE) {
                    std::advance(it, i);
                    master->update_queue->queue.erase(it);
                    init_stat = 1;

                    break;
                }
            }
        lock.unlock();
    }
}

ON_UPDATE_ERR wait_stop(thread_master_t* master, pthread_t thread_id) {
    assert(master);

    size_t stop_stat = 0;
    notify_t notify = {};

    std::unique_lock<std::mutex> lock (master->update_queue->mutex); 
        std::vector<notify_t>::iterator it = master->update_queue->queue.begin();
    lock.unlock();

    while (!stop_stat) {
        wait_update(master);

        lock.lock();
            notify_t notify = master->update_queue->queue.back();

            for (int i = 0; i < master->threads.size(); i ++) {
                notify = master->update_queue->queue[i];

                if (notify.type == UPD::READY_TO_JOIN && notify.thread_id == thread_id) {
                    std::advance(it, i);
                    master->update_queue->queue.erase(it);
                    stop_stat = 1;

                    break;
                }
            }
        lock.unlock();
    }

    return ON_UPDATE_ERR::SUCCESS;
}


ON_UPDATE_ERR join_thread(thread_master_t* master, pthread_t thread_id) {
    assert(master);

    notify_t  notify = {.type = UPD::ALLOW_JOIN};
    thread_t* thread = get_thread(master, thread_id);

    {
        std::scoped_lock<std::mutex> thr_lock(thread->update_self);
        
            std::scoped_lock<std::mutex> lock(thread->recieve_notify->mutex);
            thread->recieve_notify->queue.push_back(notify);

            std::unique_lock<std::mutex> cv_lock(thread->recieve_notify->update_var);
            thread->recieve_notify->update_var.notify_one();
    }

    int join_err = pthread_join(thread_id, NULL);

    if (join_err) {
        std::cout << "Join returned with error: " << join_err << "\n";
        return ON_UPDATE_ERR::JOIN_FAILED;
    }

    std::cout << "Thread " << thread->thread_name << " joined successfully!\n";
    master->num_running_threads --;

    return ON_UPDATE_ERR::SUCCESS;
}

ON_UPDATE_ERR interrupt_thread(thread_master_t* master, pthread_t thread_id) {
    assert(master);

    thread_t* thread = get_thread(master, thread_id);
    if (!thread) {
        std::cout << "No thread exist!\n";
        return ON_UPDATE_ERR::NO_THREAD_EXIST;
    }

    {
        std::scoped_lock<std::mutex> lock (thread->update_self);
        thread->interrupted = 1;

        std::unique_lock<std::mutex> cv_lock(thread->blocking_var->var_mutex);
        thread->blocking_var->var.notify_one();
    }

    return ON_UPDATE_ERR::SUCCESS;

}

void run_threads(thread_master_t* master) {
    assert(master);

    std::sort(master->threads.begin(), master->threads.end(), compare_threads);

    for (int i = 0; i < master->threads.size(); i ++) {
        pthread_t thread_id = 0;
        pthread_create(&thread_id, NULL, master->threads[i].thread_func, master->threads[i].thread_args);

        //if start order was specified, wait until success creation 
        if (master->threads[i].start_order <= MAX_THR_NUM)
            ON_UPDATE_ERR err = wait_init(master);

        master->num_running_threads += 1;
        std::cout << "Thread " << master->threads[i].thread_name << " initilized!\n";
    }
}

void join_threads(thread_master_t* master) {
    assert(master);

    std::sort(master->threads.begin(), master->threads.end(), compare_threads);

    for (int i = 0; i < master->threads.size(); i ++) {
        thread_t* thread = master->thread[i];
        pthread_t thread_id = 0;
        
        std::unique_lock<std::mutex> thr_lock(thread->update_self);
            thread_id = thread->thread_id;
        thr_lock.unlock();

        // now thread released
        interrupt_thread(master, thread_id);

        // wait until thread will finish execution
        // and be ready to join
        wait_stop(master, thread_id);


        // now it can be safely joined
        join_thread(master, thread_id);
    }
}

//--------------------------------------------------------------------------------------------------//



//----------------------------------------NOTIFY API/HANDLING---------------------------------------//


void wait_update(thread_master_t* master) {
    while (master->update_queue->queue.empty()) {
        std::unique_lock<std::mutex> ul(master->update_mutex);
        master->update_wait.wait(ul);
    }
}

ON_UPDATE_ERR update(thread_master_t* master) {
    notify_t update = {};

    {
        std::scoped_lock<std::mutex> lock(master->update_queue->mutex);
            update_t update = master->update_queue->queue.back();
            master->update_queue->queue.pop_back();
    }

    ON_UPDATE_ERR err = ON_UPDATE_ERR::SUCCESS;

    switch (update.type) {
        case UPD::READY_TO_JOIN:
            // if thread finished itself
            err = join_thread(master, update.thread_id);
            break;
        case UPD::DUMP_THREADS:
            err = dump_threads(master);
            break;
        case UPD::EXIT_ALL_THREADS:
            std::cout << "Terminating threads ...\n";
            err = ON_UPDATE_ERR::FINISH_UPDATE;
            break;
        default:
            std::cout << "Bad update type: " << update.type << "\n";
            break;
    }

    return err;
}


void push_notify(ts_queue *update_queue, notify_t update) {
    std::scoped_lock<std::mutex> lock(update_queue->mutex);
        update_queue->queue.push_back(update);

        std::unique_lock<std::mutex> unique(update_queue->update_var);
        update_queue->update_var.notify_one();
}

//--------------------------------------------------------------------------------------------------------------------//



void* master_thread(void* mstr) {
    assert(mstr);
    thread_master_t* master = (thread_master_t*)mstr;

    run_threads(master);

    std::cout << "All threads are running!\n";

    while (RUNNING_THREAD_EXISTS(master)) {
        ON_UPDATE_ERR err = ON_UPDATE_ERR::SUCCESS;

        wait_update(master);
        err = update(master);

        if (err == ON_UPDATE_ERR::FINISH_UPDATE)
            break;
    }

    join_threads(master);

    std::cout << "All threads are joined!\n";

    pthread_exit(NULL);
    return NULL;

}