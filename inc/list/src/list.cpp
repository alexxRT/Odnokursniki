#include "memory.h"
#include "list.h"
#include "list_debug.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <iostream>

static const int HEAD_ID = 0;

#define CHECK_UP( lst_ptr ) lst_ptr->size == lst_ptr->capacity
#define CHECK_DOWN( lst_ptr ) lst_ptr->size < (lst_ptr->capacity) / 4 && \
lst_ptr->capacity / 2 >= lst_ptr->min_capacity


#define SAME_STATUS( stat_1, stat_2 ) (stat_1 == stat_2)


LIST_ERR_CODE InsertFreeList (my_list* list, size_t elem);
size_t        TakeFreeList   (my_list* list);

//------------------------------------------------------INIT/DESTROY---------------------------------------------------//

LIST_ERR_CODE ListInit (my_list* list, size_t elem_num) {
    assert (list != NULL);

    list->buffer = CALLOC (elem_num + 1, list_elem);
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

LIST_ERR_CODE ListDestroy (my_list* list)
{
    LIST_VALIDATE (list);

    list->capacity  = 0;
    list->size      = 0;
    list->free_head_id = 0;

    FREE (list->buffer);

    return SUCCESS;
}

//---------------------------------------------------------------------------------------------------------------------//

size_t GetElemIndex (my_list* list, size_t pos) { //Gives elem by his order number
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

//------------------------------------------INSERT/DELETE TO/FROM LOGIC ELEM'S POSITION--------------------------------//

int InsertRight (my_list* list, size_t elem, size_t pos);
int DeleteRight (my_list* list, size_t pos);
int InsertLeft  (my_list* list, size_t elem, size_t pos);
int DeleteLeft  (my_list* list, size_t pos);

// head ---> node_1 ---> node_2 ---> ....
//  0          1           2         ....
//inserting with position 0 everytime will couse inserting in tail

LIST_ERR_CODE ListInsertHead(my_list* list, list_data_t data) {
    LIST_VALIDATE (list);

    ListResize (list);

    size_t new_to_add = TakeFreeList (list);
    ELEM(list, new_to_add)->status = NODE_STATUS::ENGAGED;
    list->size ++;

    ELEM(list, new_to_add)->data = data;

    InsertRight(list, new_to_add, HEAD_ID);

    // printf ("\nHEAD INSERT\n");
    // printf ("new to add val: %lu\n", new_to_add);
    // printf ("next elem index in head: %lu\n", ELEM(list, HEAD_ID)->next);

    LIST_VALIDATE(list);

    return LIST_ERR_CODE::SUCCESS;
}

LIST_ERR_CODE ListInsert (my_list* list, size_t pos, list_data_t data) {
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


LIST_ERR_CODE ListDelete (my_list* list, size_t pos) {
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


int InsertRight (my_list* list, size_t elem, size_t pos) {
    size_t old_elem            = ELEM(list, pos)->next;
    ELEM(list, pos )->next     = elem;
    ELEM(list, elem)->prev     = ELEM(list, pos)->index;
    ELEM(list, elem)->next     = old_elem;
    ELEM(list, old_elem)->prev = elem;

    return SUCCESS;
}

int DeleteRight (my_list* list, size_t elem) {
    size_t del_elem = ELEM(list, elem)->next;
    ELEM(list, elem)->next = ELEM(list, del_elem)->next;
    NEXT(list, del_elem)->prev = elem;

    ELEM(list, del_elem)->prev = 0;
    ELEM(list, del_elem)->next = 0;

    return SUCCESS;
}

int InsertLeft (my_list* list, size_t elem, size_t pos) {
    size_t old_elem            = ELEM(list, pos)->prev;
    ELEM(list, pos )->prev     = elem;
    ELEM(list, elem)->next     = ELEM(list, pos)->index;
    ELEM(list, elem)->prev     = old_elem;
    ELEM(list, old_elem)->next = elem;

    return SUCCESS;
}

int DeleteLeft (my_list* list, size_t elem) {
    size_t del_elem = ELEM(list, elem)->prev;
    ELEM(list, elem)->prev = ELEM(list, del_elem)->prev;
    PREV(list, del_elem)->next = elem;

    ELEM(list, del_elem)->prev = 0;
    ELEM(list, del_elem)->next = 0;

    return SUCCESS;
}


size_t TakeFreeList (my_list* list) {
    assert (list != NULL);

    list_elem* head = ELEM(list, list->free_head_id);

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


LIST_ERR_CODE InsertFreeList (my_list* list, size_t elem) {

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

//--------------------------------------------------------------------------------------------------------//




//----------------------------------------INSERT/DELETE BY ELEM'S INDEX-----------------------------------//

size_t DeleteFreeHead    (my_list* list);
size_t DeleteIndex       (my_list* list, size_t indx);
size_t TakeFreeList_indx (my_list* list, size_t indx);
LIST_ERR_CODE ResizeUp (my_list* list);


LIST_ERR_CODE ListInsertIndex(my_list* list, size_t index, list_data_t data) {
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
        list_data_t old_data = ELEM(list, index)->data;

        ELEM(list, index)->data = data;

        ELEM(list, new_to_add)->status = NODE_STATUS::ENGAGED;
        ELEM(list, new_to_add)->data = old_data;

        InsertRight(list, new_to_add, HEAD_ID);
    }

    list->size ++;

    LIST_VALIDATE (list);

    return SUCCESS;
}



LIST_ERR_CODE ListDeleteIndex (my_list* list, size_t index) {
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

size_t DeleteFreeHead (my_list* list) {
    size_t old_head = list->free_head_id;
    size_t new_head = ELEM(list, old_head)->next;

    ELEM(list, new_head)->prev = ELEM(list, old_head)->prev;
    PREV(list, old_head)->next = new_head;
    list->free_head_id         = new_head;

    ELEM(list, old_head)->next = 0;
    ELEM(list, old_head)->prev = 0;

    return SUCCESS;
}

size_t DeleteIndex (my_list* list, size_t indx) {
    PREV(list, indx)->next = ELEM(list, indx)->next;
    NEXT(list, indx)->prev = ELEM(list, indx)->prev;

    ELEM(list, indx)->next = 0;
    ELEM(list, indx)->prev = 0;

    return SUCCESS;
}

size_t TakeFreeList_indx (my_list* list, size_t indx) {
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

//------------------------------------------------------------------------------------------------//


//---------------------------------------------LINIRIAZATION---------------------------------------//

LIST_ERR_CODE MakeListGreatAgain (my_list* list) {
    LIST_VALIDATE (list);

    my_list new_list = {};
    ListInit(&new_list, list->capacity);

    size_t elem = ELEM(list, HEAD_ID)->next;

    for (size_t i = 1; i <= list->size; i ++) {
        list_data_t elem_data = ELEM(list, elem)->data;

        LIST_ERR_CODE err_code = ListInsertIndex(&new_list, i, elem_data);

        elem = ELEM(list, elem)->next;
    }

    FREE(list->buffer);

    list->buffer       = new_list.buffer;
    list->free_head_id = new_list.free_head_id;

    // fprintf(stderr, "LINIRIZE FINISHED\n");
    // fprintf(stderr, "List size after LINIRIZE: %lu\n", list->size);
    
    LIST_VALIDATE (list);

    return SUCCESS;
}

//--------------------------------------------------------------------------------------------------//



//------------------------------------------RESIZE FUNCTIONS-----------------------------------------//

LIST_ERR_CODE ResizeUp (my_list* list) {
    LIST_VALIDATE (list);

    // InitGraphDumpFile("list_before_realloc.gv");
    // ListGraphDump(list);
    // DestroyGraphDumpFile();

    size_t old_capacity = list->capacity;
    
    size_t new_list_capacity = 2*(old_capacity + !old_capacity);
    list_elem* new_list = (list_elem*)realloc((void*)list->buffer, (new_list_capacity + 1)*sizeof(list_elem));

    if (!new_list)
        return LIST_ERR_CODE::DATA_NULL;

    list->buffer   = new_list;
    list->capacity = new_list_capacity;

    // InitGraphDumpFile("list_after_realloc.gv");
    // ListGraphDump(list);
    // DestroyGraphDumpFile();

    // printf("NEW LIST CAPACITY: %lu\n", new_list_capacity);
    // printf("OLD CAPACITY: %lu\n", old_capacity);
    // printf("LIST SIZE AFTER REALLOC: %lu\n", list->size);

    for (size_t i = old_capacity + 1; i <= new_list_capacity; i ++) {
        list->buffer[i].index = i;
        LIST_ERR_CODE insert_err = InsertFreeList(list, i);

        if (insert_err != LIST_ERR_CODE::SUCCESS)
            PrintErr(list, insert_err, __LINE__, __func__);
    }

    // InitGraphDumpFile("list_resize.gv");
    // ListGraphDump(list);
    // DestroyGraphDumpFile();

    LIST_VALIDATE (list);

    return SUCCESS;
}

LIST_ERR_CODE ResizeDown (my_list* list) {
    LIST_VALIDATE (list);

    size_t new_list_capacity = list->capacity / 2 ;
    list_elem* new_list = (list_elem*)realloc((void*)list->buffer, (new_list_capacity + 1) * sizeof(list_elem));

    if (!new_list)
        return LIST_ERR_CODE::DATA_NULL;

    list->buffer   = new_list;
    list->capacity = new_list_capacity;

    // printf ("\n\n---------------------STARTING INITING NEW FREE LIST------------------------\n");
    // printf("list size: %lu\n", list->size);
    // printf("list cacity: %lu\n", list->capacity);
    // printf("list free head id: %lu\n", list->free_head_id);
    // printf("-------------------------------------------------------------------------------\n\n");

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

LIST_ERR_CODE ListResize (my_list* list) {
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
//----------------------------------------------------------------------------------------------------------------//
