#ifndef LIST_H
#define LIST_H

#include <stdlib.h>
#include "messages.h"

typedef chat_message_t list_data_t;
#define ZERO_DATA {}

enum class NODE_STATUS : int
{
    FREE    = 1,
    ENGAGED = 2, 
    MASTER  = 3 
};

typedef struct _list_elem {
    NODE_STATUS status;
    size_t next;
    size_t prev;
    size_t index;
    list_data_t data;
} list_elem;


typedef struct _list {
    list_elem* buffer;
    size_t     free_head_id;
    size_t     size;
    size_t     capacity;
    size_t     min_capacity;
} my_list;


typedef enum ERROR_CODES
{
    SUCCESS          = 0,
    LIST_NULL        = 1,
    DATA_NULL        = 2,
    LIST_OVERFLOW    = 3,
    LIST_UNDERFLOW   = 4,
    CAPACITY_INVALID = 5,
    WRONG_SIZE       = 6,
    BROKEN_LOOP_ENGAGED = 7,
    NULL_LINK        = 8,
    HEAD_DELEATE     = 9,
    HEAD_INSERT      = 10,
    BROKEN_LOOP_FREE = 11,
    WRONG_INDX       =-2, //these error codes can be normaly returned by functions, so they have "imposble" value 
    WRONG_POS        =-1 //
} LIST_ERR_CODE;


#define ELEM( lst, indx ) ((lst)->buffer  + indx)
#define NEXT( lst, indx ) ((lst)->buffer + (((lst)->buffer + indx)->next))
#define PREV( lst, indx ) ((lst)->buffer + (((lst)->buffer + indx)->prev)) 


LIST_ERR_CODE ListInit     (my_list* list, size_t elem_num);
LIST_ERR_CODE ListDestroy  (my_list* list);


/// @brief 
/// @param list 
/// @param pos order position of the insert, if counts clockwise
/// @param data data to be placed in list node, copy is used to prevent changes from the outside
/// @return 
LIST_ERR_CODE ListInsertHead(my_list* list, list_data_t data);
LIST_ERR_CODE ListInsert    (my_list* list, size_t pos, list_data_t data);

LIST_ERR_CODE ListDelete (my_list* list, size_t pos);

LIST_ERR_CODE ListInsertIndex (my_list* list, size_t index, list_data_t data);
LIST_ERR_CODE ListDeleteIndex (my_list* list, size_t index);

LIST_ERR_CODE ListResize (my_list* list);

size_t GetElemIndex (my_list* list, size_t pos);

LIST_ERR_CODE MakeListGreatAgain (my_list* list);

#endif