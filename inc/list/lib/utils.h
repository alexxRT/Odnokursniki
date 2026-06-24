#pragma once

#ifdef DEBUG_VERSION //this custom assert will print error that happened and 'll exit function
#define LIST_VALIDATE( lst_ptr )                                 \
do{                                                              \
    LIST_ERR_CODE ErrCode = ListValid (lst_ptr);                 \
    if (ErrCode != LIST_ERR_CODE::SUCCESS)                       \
        {                                                        \
            PrintErr (lst_ptr, ErrCode, __LINE__, __func__);     \
            return ErrCode;                                      \
        }                                                        \
}                                                                \
while (0)                                                        \

#else
#define LIST_VALIDATE( lst_ptr ) (void*)0
#endif

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

#define CHECK_UP( lst_ptr ) lst_ptr->size == lst_ptr->capacity
#define CHECK_DOWN( lst_ptr ) lst_ptr->size < (lst_ptr->capacity) / 4 && \
lst_ptr->capacity / 2 >= lst_ptr->min_capacity

#define SAME_STATUS( stat_1, stat_2 ) (stat_1 == stat_2)
