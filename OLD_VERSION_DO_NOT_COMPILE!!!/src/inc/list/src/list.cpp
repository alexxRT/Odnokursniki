#include "memory.h"
#include "list.h"
#include "list_debug.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>


#define CHECK_UP( lst_ptr ) lst_ptr->size == lst_ptr->capacity

#define CHECK_DOWN( lst_ptr ) lst_ptr->size < (lst_ptr->capacity) / 4 && \
lst_ptr->capacity / 2 >= lst_ptr->min_capacity

int        insert_free_list (list_* list, list_elem* elem);
list_elem* take_free_list   (list_* list);

//------------------------------------------------------INIT/DESTROY---------------------------------------------------//

LIST_ERR_CODE list_init (list_* list, size_t elem_num, void (*init_destructor)(list_data_t*) = NULL)
{
    assert (list != NULL);

    list->buffer    = CALLOC(elem_num + 1, list_elem);
    list->free_head = list->buffer;
    list->capacity  = elem_num;
    list->min_capacity = elem_num;
    list->size     = 0;

    list->buffer[0].status = NODE_STATUS::MASTER;
    list->buffer[0].next = list->buffer;
    list->buffer[0].prev = list->buffer;

    for (size_t i = 1; i <= elem_num; i++)
    {
        list->buffer[i].index = i;
        list->buffer[i].data = NULL;
        insert_free_list (list, list->buffer + i);
    }

    //destructor for list elems
    list->destructor = init_destructor;

    LIST_VALIDATE (list);

    return LIST_ERR_CODE::SUCCESS;
}

LIST_ERR_CODE list_destroy (list_* list)
{
    LIST_VALIDATE (list);

    list->capacity  = 0;
    list->size      = 0;
    list->free_head = NULL;

    if (list->destructor)
        for (int i = 0; i < list->size; i ++)
            list->destructor(list->buffer[i].data);

    FREE(list->buffer);

    return LIST_ERR_CODE::SUCCESS;
}

//---------------------------------------------------------------------------------------------------------------------//

list_elem* get_elem (list_* list, size_t id) //Gives elem by his order number
{
    list_elem* to_get = list->buffer;

    for (size_t i = 0; i < id; i ++)
    {
        to_get = to_get->next;
    }

    return to_get;
}


//------------------------------------------INSERT/DELETE TO/FROM LOGIC ELEM'S POSITION--------------------------------//

int insert_right (list_elem* dest, list_elem* elem);
int delete_right (list_elem* dest);
int insert_left (list_elem* dest, list_elem* elem);
int delete_left (list_elem* dest);


LIST_ERR_CODE list_insert_right (list_* list, size_t id, list_data_t* data)
{
    LIST_VALIDATE (list);

    list_resize (list);

    list_elem* new_to_add = take_free_list (list);
    new_to_add->status = NODE_STATUS::ENGAGED;
    list->size ++;

    new_to_add->data = data;

    list_elem* dest = get_elem (list, id);
    insert_right (dest, new_to_add);

    LIST_VALIDATE (list);

    return LIST_ERR_CODE::SUCCESS;
}


LIST_ERR_CODE list_insert_left (list_* list, size_t id, list_data_t* data)
{
    LIST_VALIDATE (list);

    list_resize (list);

    list_elem* new_to_add = take_free_list (list);
    new_to_add->status = NODE_STATUS::ENGAGED;
    list->size ++;

    new_to_add->data = data;

    list_elem* dest = get_elem (list, id);
    insert_left (dest, new_to_add);

    LIST_VALIDATE (list);

    return LIST_ERR_CODE::SUCCESS;
}

LIST_ERR_CODE list_delete (list_* list, size_t id)
{
    LIST_VALIDATE (list);

    if (list->capacity == 0)
        return LIST_ERR_CODE::LIST_UNDERFLOW;
    
    list_elem* del_elem = get_elem (list, id); 

    if (del_elem == list->buffer)
        return LIST_ERR_CODE::HEAD_DELEATE;

    delete_right (del_elem->prev);
    list->size --;

    insert_free_list (list, del_elem);
    list_resize(list);

    LIST_VALIDATE (list);

    return LIST_ERR_CODE::SUCCESS;
}



int insert_right (list_elem* dest, list_elem* elem)
{
    list_elem* old_elem = NEXT(dest);

    NEXT(dest)     = elem;
    PREV(elem)     = dest;
    NEXT(elem)     = old_elem;
    PREV(old_elem) = elem;

    return static_cast<int>(LIST_ERR_CODE::SUCCESS);
}

int delete_right (list_elem* dest)
{
    list_elem* del_elem = NEXT(dest);
    NEXT(dest) = NEXT(del_elem);
    del_elem->next->prev = dest;

    PREV(del_elem) = NULL;
    NEXT(del_elem) = NULL;

    return static_cast<int>(LIST_ERR_CODE::SUCCESS);
}

int insert_left (list_elem* dest, list_elem* elem)
{
    list_elem* old_elem = dest->prev;

    PREV(dest) = elem;
    NEXT(elem) = dest;
    PREV(elem) = old_elem;
    NEXT(old_elem) = elem;

    return static_cast<int>(LIST_ERR_CODE::SUCCESS);
}

int delete_left (list_elem* dest)
{
    list_elem* del_elem = PREV(dest);
    PREV(dest) = PREV(del_elem);
    del_elem->prev->next = dest;

    NEXT(del_elem) = NULL;
    PREV(del_elem) = NULL;

    return static_cast<int>(LIST_ERR_CODE::SUCCESS);
}


list_elem* take_free_list (list_* list)
{
    assert (list            != NULL);
    assert (FREE_HEAD(list) != NULL);

    list_elem* head = FREE_HEAD(list);

    if (NEXT(head) == head) //only head left
    {
        FREE_HEAD(list) = NULL;
        NEXT(head) = NULL;
        PREV(head)= NULL;

        return head;
    }
    else 
    {
        list_elem* pop_elem = NEXT(head);
        delete_right (head);

        return pop_elem;
    }

    return NULL;
}


int insert_free_list (list_* list, list_elem* elem)
{
    assert (list != NULL);
    assert (elem != NULL);

    elem->status = NODE_STATUS::FREE;
    elem->data   = NULL;

    if (FREE_HEAD(list) == NULL) // no free elems
    {
        FREE_HEAD(list) = elem;   //initing free head
        NEXT(elem) = elem;        //cycling the list
        PREV(elem) = elem;
    }
    else
        insert_right (list->free_head, elem);

    return static_cast<int>(LIST_ERR_CODE::SUCCESS);
}

//--------------------------------------------------------------------------------------------------------//




//----------------------------------------INSERT/DELETE BY ELEM'S INDEX-----------------------------------//

int delete_free_head (list_* list);
int delete_index (list_* list, int indx);
list_elem* take_free_list_index (list_* list, int indx);


LIST_ERR_CODE list_insert_index (list_* list, size_t index, list_data_t* data)
{
    LIST_VALIDATE (list);

    if (index > list->capacity)
        return LIST_ERR_CODE::WRONG_INDX;

    list_resize (list);

    list_elem* new_to_add = take_free_list_index (list, index);
    new_to_add->status = NODE_STATUS::ENGAGED;
    list->size ++;

    new_to_add->data = data;

    insert_left (list->buffer, new_to_add);

    LIST_VALIDATE (list);

    return LIST_ERR_CODE::SUCCESS;
}

LIST_ERR_CODE list_delete_index (list_* list, size_t index)
{
    LIST_VALIDATE (list);
    
    if (index > list->capacity ||
        list->buffer[index].status == NODE_STATUS::FREE)
        return LIST_ERR_CODE::WRONG_INDX;

    if (list->capacity == 0)
        return LIST_ERR_CODE::LIST_UNDERFLOW;

    if (index == 0)
        return LIST_ERR_CODE::HEAD_DELEATE;
    
    list_elem* del_elem = list->buffer + index; 

    delete_right (del_elem->prev);
    list->size --;

    insert_free_list (list, del_elem);
    list_resize(list);

    LIST_VALIDATE (list);

    return LIST_ERR_CODE::SUCCESS;
}

LIST_ERR_CODE list_delete_right(list_* list, size_t index) {
    LIST_VALIDATE (list);
    
    if (index > list->capacity ||
        list->buffer[index].status == NODE_STATUS::FREE)
        return LIST_ERR_CODE::WRONG_INDX;

    if (list->capacity == 0)
        return LIST_ERR_CODE::LIST_UNDERFLOW;
    
    list_elem* del_elem = NEXT(list->buffer + index);

    if (del_elem == list->buffer)
        return LIST_ERR_CODE::HEAD_DELEATE; 

    delete_right (del_elem->prev);
    list->size --;

    insert_free_list (list, del_elem);
    list_resize(list);

    LIST_VALIDATE (list);

    return LIST_ERR_CODE::SUCCESS;

}

LIST_ERR_CODE list_delete_left(list_* list, size_t index) {
    LIST_VALIDATE (list);
    
    if (index > list->capacity ||
        list->buffer[index].status == NODE_STATUS::FREE)
        return LIST_ERR_CODE::WRONG_INDX;

    if (list->capacity == 0)
        return LIST_ERR_CODE::LIST_UNDERFLOW;
    
    list_elem* del_elem = PREV(list->buffer + index);

    if (del_elem == list->buffer)
        return LIST_ERR_CODE::HEAD_DELEATE; 

    delete_right (del_elem->prev);
    list->size --;

    insert_free_list (list, del_elem);
    list_resize(list);

    LIST_VALIDATE (list);

    return LIST_ERR_CODE::SUCCESS;

}

int delete_free_head (list_* list)
{
    list_elem* old_head = FREE_HEAD(list);
    list_elem* new_head = NEXT(FREE_HEAD(list));

    PREV(new_head) = PREV(old_head);
    NEXT(PREV(old_head)) = new_head;
    FREE_HEAD(list) = new_head;

    NEXT(old_head) = NULL;
    PREV(old_head) = NULL;

    return static_cast<int>(LIST_ERR_CODE::SUCCESS);
}

int delete_index (list_* list, int indx)
{
    list_elem* elem = list->buffer + indx;

    NEXT(PREV(elem)) = NEXT(elem);
    PREV(NEXT(elem)) = PREV(elem);

    NEXT(elem) = NULL;
    PREV(elem) = NULL;

    return static_cast<int>(LIST_ERR_CODE::SUCCESS);
}

list_elem* take_free_list_index (list_* list, int indx)
{
    list_elem* elem = list->buffer + indx;
    assert (elem->status == NODE_STATUS::FREE);

    if (NEXT(elem) == elem) //only head left
    {
        FREE_HEAD(list) = NULL;
        NEXT(elem) = NULL;
        PREV(elem) = NULL;

        return elem;
    }

    else if (elem == FREE_HEAD(list))
        delete_free_head (list);

    else 
        delete_index (list, indx);

    return elem;
}

//------------------------------------------------------------------------------------------------//


//---------------------------------------------LINIRIAZATION---------------------------------------//

LIST_ERR_CODE make_list_great_again (list_* list)
{
    LIST_VALIDATE (list);

    list_ new_list = {};
    list_init (&new_list, list->capacity, list->destructor);

    int indx = 1;
    list_elem* head = list->buffer;
    list_elem* elem = NEXT(head);

    while (elem != head)
    {
        list_data_t* data = elem->data;
        list_insert_index (&new_list, indx, data);

        elem = NEXT(elem);
        indx ++;
    }

    list_elem* old_buffer = list->buffer;
    list->buffer = new_list.buffer;
    list->free_head = new_list.free_head;

    if (list->destructor)
        for (int i = 0; i < list->size; i ++)
            list->destructor(list->buffer[i].data);

    FREE (old_buffer);

    LIST_VALIDATE (list);

    return LIST_ERR_CODE::SUCCESS;
}

//--------------------------------------------------------------------------------------------------//



//---------------------------------------------------------------------------------------------------//



//------------------------------------------RESIZE FUNCTIONS-----------------------------------------//

LIST_ERR_CODE resize_up (list_* list)
{
    LIST_VALIDATE (list);

    list_ new_list = {};
    list_init (&new_list, 2*list->capacity, list->destructor);

    for (size_t i = 1; i <= list->capacity; i++)
    {
        list_elem* elem = list->buffer + i;
        list_data_t* data = elem->data;

        list_insert_index (&new_list, i, data);
    }

    list->capacity = new_list.capacity;
    list_elem* old_buffer = list->buffer;
    list->buffer = new_list.buffer;
    list->free_head = new_list.free_head;

    if (list->destructor)
        for (int i = 0; i < list->size; i ++)
            list->destructor(old_buffer[i].data);
    
    FREE (old_buffer);

    LIST_VALIDATE (list);

    return LIST_ERR_CODE::SUCCESS;
}

LIST_ERR_CODE resize_down (list_* list)
{
    LIST_VALIDATE (list);

    list_ new_list = {};
    list_init (&new_list, list->capacity / 2, list->destructor);

    for (size_t i = 1; i <= list->size; i ++) 
    {
        list_elem* elem = list->buffer + i;
        list_data_t* data = elem->data;

        list_insert_index (&new_list, i, data);
    }

    list->capacity = new_list.capacity;
    list_elem* old_buffer = list->buffer;
    list->buffer = new_list.buffer;
    list->free_head = new_list.free_head;

    if (list->destructor)
        for (int i = 0; i < list->size; i ++)
            list->destructor(old_buffer[i].data);
        
    FREE (old_buffer);

    LIST_VALIDATE (list);

    return LIST_ERR_CODE::SUCCESS;
}

LIST_ERR_CODE list_resize (list_* list)
{
    LIST_VALIDATE (list);

    if (CHECK_UP(list))
    {
        make_list_great_again (list);
        resize_up (list);
    }
    else if (CHECK_DOWN(list))
    {
        make_list_great_again (list);
        resize_down (list);
    }

    LIST_VALIDATE (list);

    return LIST_ERR_CODE::SUCCESS;
}

//----------------------------------------------------------------------------------------------------------------//
