#ifndef THREADS_SAFE_LIST
#define THREADS_SAFE_LIST

#include <errno.h>
#include <pthread.h>
#include "list.h"
#include "list_debug.h"
#include "chat_configs.h"
#include "networking.h"
#include <mutex>


typedef struct thread_args_ {
    OWNER owner;
    void* owner_struct;
    const char* ip_addr;
    size_t port;
}thread_args;

class ts_list
{
    public:
    ts_list(size_t num_elem){
        LIST_ERR_CODE err_code = ListInit(&list, num_elem);

        if (err_code != LIST_ERR_CODE::SUCCESS){

            #ifdef DEBUG_VERSION
                fprintf(stderr, "Bad List Init\n");
                PrintErr(&list, err_code, __LINE__, __func__);
            #endif

            list.buffer = NULL;
        }
    };

    ~ts_list() {
        LIST_ERR_CODE err_code = ListDestroy(&list);

        if (err_code != LIST_ERR_CODE::SUCCESS){

            #ifdef DEBUG_VERSION
                fprintf(stderr, "Bad List Destroy\n");
                PrintErr(&list, err_code, __LINE__, __func__);
            #endif

            list.buffer = NULL;
        }
    };

    void insert_head(list_data_t item) {
        std::scoped_lock lock(mx_list);
        LIST_ERR_CODE err_code = ListInsertHead(&list, item);

        if(err_code != LIST_ERR_CODE::SUCCESS){
            #ifdef DEBUG_VERSION
                fprintf(stderr, "Bad List Insert\n");
                PrintErr(&list, err_code, __LINE__, __func__);
            #endif
        }

        std::unique_lock<std::mutex> ul(mx_update);
        update.notify_one();
    };

    void insert(list_data_t item, size_t pos) {
        std::scoped_lock lock(mx_list);
        LIST_ERR_CODE err_code = ListInsert(&list, pos, item);

        if(err_code != LIST_ERR_CODE::SUCCESS){
            #ifdef DEBUG_VERSION
                fprintf(stderr, "Bad List Insert\n");
                PrintErr(&list, err_code, __LINE__, __func__);
            #endif
        }

        std::unique_lock<std::mutex> ul(mx_update);
        update.notify_one();
    };

    void delete_elem(size_t pos){
        std::scoped_lock lock(mx_list);
        LIST_ERR_CODE err_code = ListDelete(&list, pos);

        if(err_code != LIST_ERR_CODE::SUCCESS){
            #ifdef DEBUG_VERSION
                fprintf(stderr, "Bad List Deletet\n");
                PrintErr(&list, err_code, __LINE__, __func__);
            #endif
        }
    };

    void delete_head(){
        std::scoped_lock lock(mx_list);
        // list head position = 1
        LIST_ERR_CODE err_code = ListDelete(&list, 1);

        if(err_code != LIST_ERR_CODE::SUCCESS){
            #ifdef DEBUG_VERSION
                fprintf(stderr, "Bad List Deletet\n");
                PrintErr(&list, err_code, __LINE__, __func__);
            #endif
        }
    };

    bool empty() {
        std::scoped_lock lock(mx_list);
        return list.size == 0 ? true : false;
    };

    size_t count() {
        std::scoped_lock lock(mx_list);
        return list.size;
    };

    list_elem* get_elem(size_t pos) {
        std::scoped_lock lock(mx_list);

        size_t indx = GetElemIndex(&list, pos);

        if (indx >= 0)
            return ELEM(&list, indx);

        return NULL;
    }

    list_elem* get_head() {
        std::scoped_lock lock(mx_list);

        // head elem position = 1
        size_t indx = GetElemIndex(&list, 1);

        if (indx >= 0)
            return ELEM(&list, indx);
        
        return NULL;
    }

    void wait(){
        while (empty()){
            std::unique_lock<std::mutex> ul(mx_update);
            update.wait(ul);
        }
    }

    protected:
    std::mutex mx_list;
    my_list list;

    std::condition_variable update;
    std::mutex mx_update;
};

#endif //THREADS_SAFE_LIST