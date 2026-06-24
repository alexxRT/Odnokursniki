#ifndef LIST_H
#define LIST_H

#include <stdlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <iostream>
#include "utils.h"

#define ZERO_DATA {}

enum class NODE_STATUS : int
{
    FREE    = 1,
    ENGAGED = 2,
    MASTER  = 3
};

template <typename T>
struct list_elem_t {
    NODE_STATUS status;
    size_t next;
    size_t prev;
    size_t index;
    T data;
};

template <typename ListDataT>
struct list_t {
    list_elem_t<ListDataT>* buffer;
    size_t     free_head_id;
    size_t     size;
    size_t     capacity;
    size_t     min_capacity;
};

const int HEAD_ID = 0;

// ----------------------------------- Interface Declaration --------------------------------- //
template <typename ListDataT>
LIST_ERR_CODE ListInit(list_t<ListDataT>* list, size_t elem_num);
template <typename ListDataT>
LIST_ERR_CODE ListDestroy(list_t<ListDataT>* list);
template <typename ListDataT>
LIST_ERR_CODE ListInsertHead(list_t<ListDataT>* list, ListDataT data);
template <typename ListDataT>
LIST_ERR_CODE ListInsert(list_t<ListDataT>* list, size_t pos, ListDataT data);
template <typename ListDataT>
LIST_ERR_CODE ListDelete(list_t<ListDataT>* list, size_t pos);
template <typename ListDataT>
LIST_ERR_CODE ListInsertIndex(list_t<ListDataT>* list, size_t index, ListDataT data);
template <typename ListDataT>
LIST_ERR_CODE ListDeleteIndex(list_t<ListDataT>* list, size_t index);
template <typename ListDataT>
LIST_ERR_CODE ListResize(list_t<ListDataT>* list);
template <typename ListDataT>
LIST_ERR_CODE ResizeUp(list_t<ListDataT>* list);
template <typename ListDataT>
LIST_ERR_CODE ResizeDown(list_t<ListDataT>* list);
template <typename ListDataT>
size_t GetElemIndex (list_t<ListDataT>* list, size_t pos);

// -------------------------------------------------------------------------------------------------//





template <typename ListDataT>
LIST_ERR_CODE ListInit(list_t<ListDataT>* list, size_t elem_num) {
    assert (list != NULL);

    list->buffer = CALLOC (elem_num + 1, list_elem_t<ListDataT>);
    list->free_head_id = HEAD_ID;
    list->capacity  = elem_num;
    list->min_capacity = elem_num;
    list->size     = 0;

    list->buffer[0].status = NODE_STATUS::MASTER;
    list->buffer[0].next  = HEAD_ID;
    list->buffer[0].prev  = HEAD_ID;
    list->buffer[0].index = HEAD_ID;

    // initing free list
    for (size_t i = 1; i <= elem_num; i++) {
        list->buffer[i].index = i;
        LIST_ERR_CODE err_code = InsertFreeList (list, i);

        //bad insert on init//
        if (err_code != LIST_ERR_CODE::SUCCESS)
            return err_code;
    }

    LIST_VALIDATE (list);

    return SUCCESS;
}


template <typename ListDataT>
LIST_ERR_CODE ListDestroy(list_t<ListDataT>* list) {
    LIST_VALIDATE (list);

    list->capacity  = 0;
    list->size      = 0;
    list->free_head_id = 0;

    FREE (list->buffer);

    return SUCCESS;
}


// head ---> node_1 ---> node_2 ---> ....
//  0          1           2         ....
//inserting with position 0 everytime will couse inserting in tail
template <typename ListDataT>
LIST_ERR_CODE ListInsertHead(list_t<ListDataT>* list, ListDataT data) {
    LIST_VALIDATE (list);

    ListResize (list);

    size_t new_to_add = TakeFreeList (list);
    ELEM(list, new_to_add)->status = NODE_STATUS::ENGAGED;
    list->size ++;

    ELEM(list, new_to_add)->data = data;

    InsertRight(list, new_to_add, HEAD_ID);

    LIST_VALIDATE(list);

    return LIST_ERR_CODE::SUCCESS;
}

template <typename ListDataT>
LIST_ERR_CODE ListInsert(list_t<ListDataT>* list, size_t pos, ListDataT data) {
    LIST_VALIDATE (list);

    ListResize (list);

    size_t new_to_add = TakeFreeList (list);
    ELEM(list, new_to_add)->status = NODE_STATUS::ENGAGED;
    ELEM(list, new_to_add)->data = data;

    list->size ++;

    size_t dest = GetElemIndex (list, pos);
    LIST_ERR_CODE err_code = (LIST_ERR_CODE)dest;

    if (err_code == LIST_ERR_CODE::WRONG_INDX)
        return err_code;

    size_t rel_dest    = ELEM(list, dest)->prev;
    InsertRight (list, new_to_add, rel_dest);

    LIST_VALIDATE (list);

    return LIST_ERR_CODE::SUCCESS;
}

template <typename ListDataT>
LIST_ERR_CODE ListDelete(list_t<ListDataT>* list, size_t pos) {
    LIST_VALIDATE (list);

    if (list->capacity == 0)
        return LIST_UNDERFLOW;

    size_t del_elem = GetElemIndex (list, pos);
    LIST_ERR_CODE err_code = (LIST_ERR_CODE)del_elem;

    if (err_code == LIST_ERR_CODE::WRONG_POS)
        return err_code;

    if (del_elem == HEAD_ID)
        return LIST_ERR_CODE::HEAD_DELEATE;

    size_t rel_pos = ELEM(list, del_elem)->prev;

    DeleteRight(list, rel_pos);
    list->size --;

    InsertFreeList (list, del_elem);
    ListResize(list);

    LIST_VALIDATE (list);

    return SUCCESS;
}

template <typename ListDataT>
LIST_ERR_CODE ListInsertIndex(list_t<ListDataT>* list, size_t index, ListDataT data) {
    LIST_VALIDATE (list);

    //Head index can not be changed
    if (index == HEAD_ID)
        return HEAD_INSERT;

    //array index overflow
    if (index > list->capacity)
        return WRONG_INDX;

    ListResize(list);

    size_t new_to_add = 0;
    if (ELEM(list, index)->status == NODE_STATUS::FREE) {
        new_to_add = TakeFreeList_indx(list, index);

        ELEM(list, new_to_add)->status = NODE_STATUS::ENGAGED;
        ELEM(list, new_to_add)->data   = data;

        InsertLeft(list, new_to_add, HEAD_ID);
    }
    else if (ELEM(list, index)->status == NODE_STATUS::ENGAGED){
        new_to_add = TakeFreeList (list);
        ListDataT old_data = ELEM(list, index)->data;

        ELEM(list, index)->data = data;

        ELEM(list, new_to_add)->status = NODE_STATUS::ENGAGED;
        ELEM(list, new_to_add)->data = old_data;

        InsertRight(list, new_to_add, HEAD_ID);
    }

    list->size ++;

    LIST_VALIDATE (list);

    return SUCCESS;
}

template <typename ListDataT>
LIST_ERR_CODE ListDeleteIndex(list_t<ListDataT>* list, size_t index) {
    LIST_VALIDATE (list);

    if (index > list->capacity ||
        list->buffer[index].status == NODE_STATUS::FREE)
        return WRONG_INDX;

    if (list->capacity == 0)
        return LIST_UNDERFLOW;

    if (index == HEAD_ID)
        return HEAD_DELEATE;

    size_t rel_pos  = ELEM(list, index)->prev;
    size_t del_elem = ELEM(list, index)->index;

    DeleteRight (list, rel_pos);
    list->size --;

    InsertFreeList (list, del_elem);
    ListResize(list);

    LIST_VALIDATE (list);

    return SUCCESS;
}

template <typename ListDataT>
LIST_ERR_CODE ListResize (list_t<ListDataT>* list) {
    LIST_VALIDATE (list);

    if (CHECK_UP(list)) {
        ResizeUp (list);
    }
    else if (CHECK_DOWN(list)) {
        MakeListGreatAgain(list);
        ResizeDown (list);
    }

    LIST_VALIDATE (list);

    return SUCCESS;
}

template <typename ListDataT>
size_t GetElemIndex (list_t<ListDataT>* list, size_t pos) { //Gives elem by his order number
    size_t to_get = 0;
    size_t i = 0;

    for (; i < pos; i ++)
        to_get = NEXT(list, to_get)->index;

    //if element position is out of list size
    //we do not want to go round the list and looking for elem
    if (i > list->size)
        return -1;

    return to_get;
}

template <typename ListDataT>
LIST_ERR_CODE MakeListGreatAgain (list_t<ListDataT>* list) {
    LIST_VALIDATE (list);

    list_t<ListDataT> new_list = {};
    ListInit(&new_list, list->capacity);

    size_t elem = ELEM(list, HEAD_ID)->next;

    for (size_t i = 1; i <= list->size; i ++) {
        ListDataT elem_data = ELEM(list, elem)->data;

        LIST_ERR_CODE err_code = ListInsertIndex(&new_list, i, elem_data);

        elem = ELEM(list, elem)->next;
    }

    FREE(list->buffer);

    list->buffer       = new_list.buffer;
    list->free_head_id = new_list.free_head_id;

    LIST_VALIDATE (list);

    return SUCCESS;
}

template <typename ListDataT>
int InsertRight(list_t<ListDataT>* list, size_t elem, size_t pos) {
    size_t old_elem            = ELEM(list, pos)->next;
    ELEM(list, pos )->next     = elem;
    ELEM(list, elem)->prev     = ELEM(list, pos)->index;
    ELEM(list, elem)->next     = old_elem;
    ELEM(list, old_elem)->prev = elem;

    return SUCCESS;
}

template <typename ListDataT>
int DeleteRight (list_t<ListDataT>* list, size_t elem) {
    size_t del_elem = ELEM(list, elem)->next;
    ELEM(list, elem)->next = ELEM(list, del_elem)->next;
    NEXT(list, del_elem)->prev = elem;

    ELEM(list, del_elem)->prev = 0;
    ELEM(list, del_elem)->next = 0;

    return SUCCESS;
}

template <typename ListDataT>
int InsertLeft (list_t<ListDataT>* list, size_t elem, size_t pos) {
    size_t old_elem            = ELEM(list, pos)->prev;
    ELEM(list, pos )->prev     = elem;
    ELEM(list, elem)->next     = ELEM(list, pos)->index;
    ELEM(list, elem)->prev     = old_elem;
    ELEM(list, old_elem)->next = elem;

    return SUCCESS;
}

template <typename ListDataT>
int DeleteLeft (list_t<ListDataT>* list, size_t elem) {
    size_t del_elem = ELEM(list, elem)->prev;
    ELEM(list, elem)->prev = ELEM(list, del_elem)->prev;
    PREV(list, del_elem)->next = elem;

    ELEM(list, del_elem)->prev = 0;
    ELEM(list, del_elem)->next = 0;

    return SUCCESS;
}

template <typename ListDataT>
LIST_ERR_CODE InsertFreeList(list_t<ListDataT>* list, size_t elem) {

    ELEM(list, elem)->status = NODE_STATUS::FREE;
    ELEM(list, elem)->data = ZERO_DATA;

    // no free elems
    if (list->free_head_id == HEAD_ID) {
        list->free_head_id     = ELEM(list, elem)->index;        //initing free head
        ELEM(list, elem)->next = ELEM(list, elem)->index;        //cycling the list
        ELEM(list, elem)->prev = ELEM(list, elem)->index;
    }
    else
        InsertLeft (list, elem, list->free_head_id);

    return LIST_ERR_CODE::SUCCESS;
}

template <typename ListDataT>
size_t TakeFreeList (list_t<ListDataT>* list) {
    assert (list != NULL);

    list_elem_t<ListDataT>* head = ELEM(list, list->free_head_id);

    //only head left
    if (head->next == list->free_head_id) {
        list->free_head_id = HEAD_ID;
        head->next = 0;
        head->prev = 0;

        return head->index;
    }

    size_t pop_elem = head->next;
    DeleteRight (list, head->index);

    return pop_elem;
}


template <typename ListDataT>
size_t DeleteFreeHead (list_t<ListDataT>* list) {
    size_t old_head = list->free_head_id;
    size_t new_head = ELEM(list, old_head)->next;

    ELEM(list, new_head)->prev = ELEM(list, old_head)->prev;
    PREV(list, old_head)->next = new_head;
    list->free_head_id         = new_head;

    ELEM(list, old_head)->next = 0;
    ELEM(list, old_head)->prev = 0;

    return SUCCESS;
}


template <typename ListDataT>
size_t DeleteIndex (list_t<ListDataT>* list, size_t indx) {
    PREV(list, indx)->next = ELEM(list, indx)->next;
    NEXT(list, indx)->prev = ELEM(list, indx)->prev;

    ELEM(list, indx)->next = 0;
    ELEM(list, indx)->prev = 0;

    return SUCCESS;
}


template <typename ListDataT>
size_t TakeFreeList_indx (list_t<ListDataT>* list, size_t indx) {
    size_t elem = indx;

    //only head left
    if (ELEM(list, elem)->next == elem) {
        list->free_head_id = HEAD_ID;
        ELEM(list, elem)->next = 0;
        ELEM(list, elem)->prev = 0;

        return elem;
    }
    else if (elem == list->free_head_id) {
        DeleteFreeHead (list);
    }
    else
        DeleteIndex (list, indx);

    return elem;
}

template <typename ListDataT>
LIST_ERR_CODE ResizeUp (list_t<ListDataT>* list) {
    LIST_VALIDATE (list);

    // InitGraphDumpFile("list_before_realloc.gv");
    // ListGraphDump(list);
    // DestroyGraphDumpFile();

    size_t old_capacity = list->capacity;

    size_t new_list_capacity = 2*(old_capacity + !old_capacity);
    list_elem_t<ListDataT>* new_list = (list_elem_t<ListDataT>*)realloc((void*)list->buffer,
                (new_list_capacity + 1)*sizeof(list_elem_t<ListDataT>));

    if (!new_list)
        return LIST_ERR_CODE::DATA_NULL;

    list->buffer   = new_list;
    list->capacity = new_list_capacity;

    for (size_t i = old_capacity + 1; i <= new_list_capacity; i ++) {
        list->buffer[i].index = i;
        LIST_ERR_CODE insert_err = InsertFreeList(list, i);

        if (insert_err != LIST_ERR_CODE::SUCCESS)
            PrintErr(list, insert_err, __LINE__, __func__);
    }

    LIST_VALIDATE (list);

    return SUCCESS;
}

template <typename ListDataT>
LIST_ERR_CODE ResizeDown (list_t<ListDataT>* list) {
    LIST_VALIDATE (list);

    size_t new_list_capacity = list->capacity / 2 ;
    list_elem_t<ListDataT>* new_list = (list_elem_t<ListDataT>*)realloc((void*)list->buffer,
            (new_list_capacity + 1) * sizeof(list_elem_t<ListDataT>));

    if (!new_list)
        return LIST_ERR_CODE::DATA_NULL;

    list->buffer   = new_list;
    list->capacity = new_list_capacity;

    list->free_head_id = HEAD_ID;

    for (size_t i = list->size + 1; i <= new_list_capacity; i ++) {
        list->buffer[i].index = i;
        LIST_ERR_CODE insert_err = InsertFreeList(list, i);

        if (insert_err != LIST_ERR_CODE::SUCCESS)
            PrintErr(list, insert_err, __LINE__, __func__);
    }

    LIST_VALIDATE (list);

    return SUCCESS;
}

#endif
