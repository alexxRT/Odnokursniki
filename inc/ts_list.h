#ifndef THREADS_SAFE_LIST_MESSAGES
#define THREADS_SAFE_LIST_MESSAGES

#include <errno.h>
#include <pthread.h>
#include <mutex>
#include <condition_variable>

#include "messages.h"
#define USER_DTYPE chat_message_t
#include "list.h"
#include "list_debug.h"

template <typename ListDataT>
class ThreadSafeList {
    public:
    ThreadSafeList(size_t num_elem){
        LIST_ERR_CODE err_code = ListInit(&list, num_elem);

        if (err_code != LIST_ERR_CODE::SUCCESS){

            #ifdef DEBUG_VERSION
                fprintf(stderr, "Bad List Init\n");
                PrintErr(&list, err_code, __LINE__, __func__);
            #endif

            list.buffer = NULL;
        }
    };

    ~ThreadSafeList() {
        LIST_ERR_CODE err_code = ListDestroy(&list);

        if (err_code != LIST_ERR_CODE::SUCCESS){

            #ifdef DEBUG_VERSION
                fprintf(stderr, "Bad List Destroy\n");
                PrintErr(&list, err_code, __LINE__, __func__);
            #endif

            list.buffer = NULL;
        }
    };

    void insert_head(ListDataT item) {
        std::scoped_lock lock(mx_list);
        LIST_ERR_CODE err_code = ListInsertHead(&list, item);

        if(err_code != LIST_ERR_CODE::SUCCESS){
            #ifdef DEBUG_VERSION
                fprintf(stderr, "Bad List Insert\n");
                PrintErr(&list, err_code, __LINE__, __func__);
            #endif
        }

        update.notify_one();
    };

    void insert(ListDataT item, size_t pos) {
        std::scoped_lock lock(mx_list);
        LIST_ERR_CODE err_code = ListInsert(&list, pos, item);

        if(err_code != LIST_ERR_CODE::SUCCESS){
            #ifdef DEBUG_VERSION
                fprintf(stderr, "Bad List Insert\n");
                PrintErr(&list, err_code, __LINE__, __func__);
            #endif
        }

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

    list_elem_t<ListDataT>* get_elem(size_t pos) {
        std::scoped_lock lock(mx_list);

        size_t indx = GetElemIndex(&list, pos);

        if (indx >= 0)
            return ELEM(&list, indx);

        return NULL;
    }

    list_elem_t<ListDataT>* get_head() {
        std::scoped_lock lock(mx_list);

        // head elem position = 1
        size_t indx = GetElemIndex(&list, 1);

        if (indx >= 0)
            return ELEM(&list, indx);

        return NULL;
    }

    bool pop_oldest(ListDataT* item) {
        std::scoped_lock lock(mx_list);
        if (!list.size)
            return false;
        size_t tail = list.buffer[HEAD_ID].prev;
        *item = list.buffer[tail].data;
        return ListDeleteIndex(&list, tail) == LIST_ERR_CODE::SUCCESS;
    }

    bool wait_for(unsigned milliseconds) {
        std::unique_lock<std::mutex> lock(mx_list);
        return update.wait_for(lock, std::chrono::milliseconds(milliseconds), [this] { return list.size != 0; });
    }

    void wait() {
        std::unique_lock<std::mutex> lock(mx_list);
        update.wait(lock, [this] { return list.size != 0; });
    }

    protected:
    std::mutex mx_list;
    list_t<ListDataT> list;

    std::condition_variable update;
};

#endif //THREADS_SAFE_LIST_MESSAGES