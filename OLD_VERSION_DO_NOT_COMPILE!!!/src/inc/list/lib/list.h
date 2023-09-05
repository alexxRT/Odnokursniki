#ifndef LIST_H
#define LIST_H

#include <stdlib.h>
#include "messages.h"

#define NEXT(el)       (el)->next
#define PREV(el)       (el)->prev
#define FREE_HEAD(lst) (lst)->free_head

typedef chat_message_t list_data_t;

enum class NODE_STATUS
{
    FREE    = 1,
    ENGAGED = 2, 
    MASTER  = 3
};

typedef struct _list_elem
{
    struct _list_elem* next;
    struct _list_elem* prev;
    list_data_t* data;
    int index;
    NODE_STATUS status;
} list_elem;


typedef struct my_list_
{
    list_elem* buffer;
    list_elem* free_head;
    size_t size;
    size_t capacity;
    size_t min_capacity;

    void (*destructor)(list_data_t* data);
} list_;


typedef enum class ERROR_CODES
{
    SUCCESS          = 0,
    LIST_NULL        = 1,
    DATA_NULL        = 2,
    LIST_OVERFLOW    = 3,
    LIST_UNDERFLOW   = 4,
    CAPACITY_INVALID = 5,
    WRONG_SIZE       = 6,
    BROAKEN_LOOP     = 7,
    NULL_LINK        = 8,
    HEAD_DELEATE     = 9,
    WRONG_INDX       =-2, //these error codes can be normaly returned by functions, so they have "imposble" value 
    WRONG_ID         =-1 //
} LIST_ERR_CODE;


LIST_ERR_CODE list_init     (list_* list, size_t elem_num, void (*init_destructor)(list_data_t*));
LIST_ERR_CODE list_destroy  (list_* list);

LIST_ERR_CODE list_insert_right (list_* list, size_t id, list_data_t* data);
LIST_ERR_CODE list_insert_left  (list_* list, size_t id, list_data_t* data);
LIST_ERR_CODE list_delete_left  (list_* list, size_t id);
LIST_ERR_CODE list_delete_right (list_* list, size_t id);
LIST_ERR_CODE list_delete_index (list_* list, size_t id);

LIST_ERR_CODE list_insert_index (list_* list, size_t index, list_data_t* data);
LIST_ERR_CODE list_delete_index (list_* list, size_t index);

LIST_ERR_CODE list_resize (list_* list);

list_elem* get_elem (list_* list, size_t id);

LIST_ERR_CODE make_list_great_again (list_* list);

#endif