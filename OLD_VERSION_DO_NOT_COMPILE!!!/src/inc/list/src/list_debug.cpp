#include "memory.h"
#include "list.h"
#include "list_debug.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>


///----------------------------------------------------FILES FOR DEBUG PRINT-------------------------------------------//

static FILE* dump_file  = NULL; 
static FILE* graph_file = NULL;
static FILE* log_file   = NULL; 


void init_log_file (const char* const log_path)
{
    log_file = fopen (log_path, "a");
    assert (log_file != NULL);
}

void destroy_log_file ()
{
    fflush (log_file);
    fclose (log_file);
    log_file = NULL;
}

void init_dump_file (const char* const dump_path)
{
    dump_file = fopen (dump_path, "a");
    assert (dump_file != NULL);
}

void destroy_dump_file ()
{
    fflush (dump_file);
    fclose (dump_file);
    dump_file = NULL;
}

void init_graph_dump_file (const char* const graph_dump_path)
{
    graph_file = fopen (graph_dump_path, "w");
    assert (graph_file != NULL);
}

void destroy_graph_dump_file ()
{
    fflush (graph_file);
    fclose (graph_file);
    graph_file = NULL;
}

//-----------------------------------------------------------------------------------------------------------------------------//



//------------------------------------------------------LOGING ERRORS----------------------------------------------------------//

void print_err (list_* list, LIST_ERR_CODE ErrCode, const int line, const char* func)
{
    assert (log_file != NULL && "Please init log file");

    switch (ErrCode)
    {
        case LIST_ERR_CODE::LIST_NULL:
            fprintf (log_file, "LIST is a NULL ptr ");
            break;
        case LIST_ERR_CODE::DATA_NULL:
            fprintf (log_file, "LIST [%p] has NULL buffer ptr ", list);
            break;
        case LIST_ERR_CODE::LIST_OVERFLOW:
            fprintf (log_file, "LIST [%p] overflowed, size - [%lu], capacity - [%lu] ", list, list->size, list->capacity);
            break;
        case LIST_ERR_CODE::LIST_UNDERFLOW:
            fprintf (log_file, "LIST [%p] underflowed, size - [%lu], capacity - [%lu] ",list, list->size, list->capacity);
            break;
        case LIST_ERR_CODE::CAPACITY_INVALID:
            fprintf (log_file, "LIST [%p] capacity equal zero - [%lu] ", list, list->capacity);
            break;
        case LIST_ERR_CODE::WRONG_SIZE:
            fprintf (log_file, "LIST [%p] has incorrect size - [%lu] ", list, list->size);
            break;
        case LIST_ERR_CODE::BROAKEN_LOOP:
            fprintf (log_file, "LIST [%p] the loop is broaken ", list);
            break;
        case LIST_ERR_CODE::NULL_LINK:
            fprintf (log_file, "LIST [%p] one of elems has invalid NULL link ", list);
            break;
        case LIST_ERR_CODE::WRONG_ID:
            fprintf (log_file, "LIST [%p] Elem's ID you use is invalid, please check your list ", list);
            break;
        case LIST_ERR_CODE::WRONG_INDX:
            fprintf (log_file, "LIST [%p] Elem's INDEX is invalid, please check your list ", list);
            break;
        case LIST_ERR_CODE::HEAD_DELEATE:
            fprintf (log_file, "LIST [%p] Trying to delete HEAD! ", list);
            break;
        case LIST_ERR_CODE::SUCCESS:
            fprintf (log_file, "LIST [%p] is consistant!\n\n", list);
            break;
        default:
        {
            fprintf (log_file, "Unexpected error code is %d\n", ErrCode);
            assert (0 && "!!Unexpected error code in ErrPrint function!!\n");
        }
    }

    if (ErrCode != LIST_ERR_CODE::SUCCESS)
        fprintf (log_file, "error happend in function [%s()] and on the line [%d]\n", func, line);

}

//------------------------------------------------------------------------------------------------------------------------//



//---------------------------------------------------VALIDATION FUNCTIONS-------------------------------------------------//

LIST_ERR_CODE list_fields_valid (list_* list);
LIST_ERR_CODE engaged_list_valid (list_* list);
LIST_ERR_CODE free_list_valid (list_* list);


LIST_ERR_CODE list_valid (list_* list)
{
    LIST_ERR_CODE ErrCode = LIST_ERR_CODE::SUCCESS;

    ErrCode = list_fields_valid  (list);
    if (ErrCode != LIST_ERR_CODE::SUCCESS)
        return ErrCode;

    ErrCode = engaged_list_valid (list);
    if (ErrCode != LIST_ERR_CODE::SUCCESS) 
      return ErrCode;

    ErrCode = free_list_valid    (list);
    if (ErrCode != LIST_ERR_CODE::SUCCESS) 
        return ErrCode;


    return ErrCode;
}

LIST_ERR_CODE list_fields_valid (list_* list)
{
    if (!list)                       return LIST_ERR_CODE::LIST_NULL;
    if (!list->buffer)               return LIST_ERR_CODE::DATA_NULL;
    if (list->size > list->capacity) return LIST_ERR_CODE::LIST_OVERFLOW;
    if (list->capacity <= 0)         return LIST_ERR_CODE::CAPACITY_INVALID; // can not create list for 0 elems, its not resizable otherwise

    return LIST_ERR_CODE::SUCCESS;
}

LIST_ERR_CODE engaged_list_valid (list_* list) //check if loop is safe and sound, no empty links
{
    int num_elems = list->size;
    int counter = 0;

    for (size_t i = 0; i <= list->capacity; i ++)
    {
        if (list->buffer[i].status == NODE_STATUS::ENGAGED)
            counter ++;
    }

    if (num_elems != counter)
        return LIST_ERR_CODE::WRONG_SIZE;
    
    list_elem* head = list->buffer;
    list_elem* elem = head;

    // it goes cloсkwise
    for (int i = 0; i <= num_elems; i ++)
    {
        if (!NEXT(elem) || !PREV(elem))
            return LIST_ERR_CODE::NULL_LINK;
        elem = NEXT(elem);
    }

    if (elem != head)
        return LIST_ERR_CODE::BROAKEN_LOOP;

    //it goes counter-clockwise
    for (int i = 0; i <= num_elems; i ++)
    {
        if (!NEXT(elem) || !PREV(elem))
            return LIST_ERR_CODE::NULL_LINK;
        elem = PREV(elem);
    }

    if (elem != head)
        return LIST_ERR_CODE::BROAKEN_LOOP;


    //double check, forward and backward
    //i store ptr, so they all should be valid   !!!
    //before it, check if size has correct value !!!
    //mark node where i started and compare where i ended
    //if they are not the same, the loop is broken, taa daaaa
    //            /\                   /\
    //            ||                   ||
    //THIS DOES NOT WORK, LIST CAN BE CYCLED ON HEAD

    return LIST_ERR_CODE::SUCCESS;
}

LIST_ERR_CODE free_list_valid (list_* list)
{
    //free list is empty, nothing to check
    int free_size = list->capacity - list->size;
    if (!free_size)
        return LIST_ERR_CODE::SUCCESS;

    list_elem* head = list->free_head;
    list_elem* elem = head;

    // it goes cloсkwise
    for (int i = 0; i < free_size; i ++)
    {
        if (!NEXT(elem) || !PREV(elem))
            return LIST_ERR_CODE::NULL_LINK;
        elem = NEXT(elem);
    }

    if (elem != head)
        return LIST_ERR_CODE::BROAKEN_LOOP;

    //it goes counter-clockwise
    for (int i = 0; i < free_size; i ++)
    {
        if (!NEXT(elem) || !PREV(elem))
            return LIST_ERR_CODE::NULL_LINK;
        elem = PREV(elem);
    }

    if (elem != head)
        return LIST_ERR_CODE::BROAKEN_LOOP;

    return LIST_ERR_CODE::SUCCESS;
}

//-------------------------------------------------------------------------------------------------------------------------------//


//-----------------------------------------------------DUMP FUNCTIONS------------------------------------------------------------//

LIST_ERR_CODE print_list (list_* list, int list_type, print_value_t print_val);  //definition below
//void print_value_int (list_elem* elem);                                         //   ||
void print_value_chat_message(list_elem* elem);                                   //   ||
                                                                                  //   \/


LIST_ERR_CODE list_text_dump (list_* list)
{
    LIST_VALIDATE (list);

    assert (dump_file != NULL && "Please init Dump file");

    fprintf (dump_file, "\n\n-------------------------LIST DUMP OCCURED-----------------------\n\n");
    fprintf (dump_file, "List address [%p]\n", list);
    fprintf (dump_file, "List head address [%p]\n", list->buffer);

    fprintf   (dump_file, "Now in the list [%lu/%lu] elems are engaged\n" , list->size, list->capacity);

    fprintf   (dump_file, "listing of engaged elems:\n");
    print_list (list, NODE_STATUS::ENGAGED, print_value_chat_message);

    fprintf   (dump_file, "Now in the list [%lu] elems are free\n", list->capacity - list->size);

    fprintf   (dump_file, "Listing of free elems:\n");
    print_list (list, NODE_STATUS::FREE, print_value_chat_message);

    fprintf   (dump_file, "\n\n-------------------------LIST DUMP FINISHED-----------------------\n\n");

    LIST_VALIDATE (list);

    return LIST_ERR_CODE::SUCCESS;    
}

//!!!GRAPHIC DUMP, SO BEAUTIFUL!!! 

int get_elem_id (list_* list, int index, NODE_STATUS stat);

LIST_ERR_CODE list_graph_dump (list_* list)
{
    LIST_VALIDATE (list);

    assert (graph_file != NULL && "Please init GraphDump file");

    //initilizing starting attributes
    fprintf (graph_file,
    "digraph {\n\
    rankdir=LR;\n\
    pack=true;\n\
    splines=ortho;\n\
    node [ shape=record ];\n\
    ");


    for (size_t i = 0; i <= list->capacity; i ++) //initing each node, gives it color, data, order num
    {
        if (list->buffer[i].status == NODE_STATUS::ENGAGED)
        {
            int id = get_elem_id (list, i, NODE_STATUS::FREE);
            int wrong_id = static_cast<int>(LIST_ERR_CODE::WRONG_ID);

            if (id == wrong_id)
                return LIST_ERR_CODE::WRONG_ID;

            fprintf (graph_file, "\n\tNode%zu", i);
            fprintf (graph_file, "[label = \"INDX: %zu|NUM: %d|MSG_TYPE: %d\";];\n", i, id, list->buffer[i].data->msg_type);
            fprintf (graph_file, "\tNode%zu", i);
            fprintf (graph_file, "[color = \"green\";];\n");
        }
        else if (list->buffer[i].status == NODE_STATUS::FREE)
        {
            int id = get_elem_id (list, i, NODE_STATUS::FREE);
            int wrong_id = static_cast<int>(LIST_ERR_CODE::WRONG_ID);

            if (id == wrong_id)
                return LIST_ERR_CODE::WRONG_ID;

            fprintf (graph_file, "\n\tNode%zu", i);
            fprintf (graph_file, "[label = \"INDX: %zu|NUM: %d|MSG_TYPE: %d\";];\n", i, id, list->buffer[i].data->msg_type);
            fprintf (graph_file, "\tNode%zu", i);
            fprintf (graph_file, "[color = \"red\";];\n");
        }
        else if (list->buffer[i].status == NODE_STATUS::MASTER)
        {
            int id = get_elem_id (list, i, NODE_STATUS::FREE);
            int wrong_id = static_cast<int>(LIST_ERR_CODE::WRONG_ID);

            if (id == wrong_id)
                return LIST_ERR_CODE::WRONG_ID;

            fprintf (graph_file, "\n\tNode%zu", i);
            fprintf (graph_file, "[label = \"INDX: %zu|NUM: %d|MSG_TYPE: %d\";];\n", i, id, list->buffer[i].data->msg_type);
            fprintf (graph_file, "\tNode%zu", i);
            fprintf (graph_file, "[color = \"purple\";];\n");
        }
    }

    fprintf (graph_file, "\n");

    for (size_t i = 0; i < list->capacity; i ++)
        fprintf (graph_file, "\tNode%zu -> Node%zu[color = \"white\";];\n", i, i+1); //ordering elems as they are in ram

    fprintf (graph_file, "\n");

    for (size_t i = 0; i <= list->capacity; i++)
    {
        if (list->buffer[i].status == NODE_STATUS::ENGAGED || list->buffer[i].status == NODE_STATUS::MASTER)
        {
            int next_indx = list->buffer[i].next->index;
            fprintf (graph_file, "\tNode%zu -> Node%d [constraint = false;];\n", i, next_indx);
        }
        else 
        {
            int next_indx = list->buffer[i].next->index;
            fprintf (graph_file, "\tNode%zu -> Node%d [constraint = false;];\n", i, next_indx);
        }
    }

    fprintf (graph_file, "}\n");


    LIST_VALIDATE (list);

    return LIST_ERR_CODE::SUCCESS;
}

//-----------------------------------------------------------------------------------------------------------------------//


//------------------------------------------------------PRINT LIST FUNCTIONS---------------------------------------------//


LIST_ERR_CODE print_list (list_* list, NODE_STATUS node_type, print_value_t print_val)
{
    LIST_VALIDATE (list);

    if (node_type == NODE_STATUS::ENGAGED)
    {
        list_elem* head =       list->buffer;
        list_elem* debug_elem = NEXT(head);

        int order_num = 1;

        while (debug_elem != head)
        {
            fprintf (dump_file,"\nOrder number [%d]\n", order_num);
            print_val(debug_elem);

            order_num ++;
            debug_elem = NEXT(debug_elem);
        }
    }
    else if (node_type == NODE_STATUS::FREE)
    {
        list_elem* head = FREE_HEAD(list);

        if (head == list->buffer)
            return LIST_ERR_CODE::SUCCESS;
        
        int order_num = 1;

        fprintf (dump_file, "\nOrder number [%d]\n", order_num);
        print_val(head);
        order_num ++;

        list_elem* debug_elem = NEXT(head);

        while (debug_elem != head)
        {
            fprintf (dump_file, "\nOrder number [%d]\n", order_num);
            print_val(debug_elem);

            order_num ++;
            debug_elem = NEXT(debug_elem);
        }
    }

    LIST_VALIDATE (list);

    return LIST_ERR_CODE::SUCCESS;
}

// void print_value_int (list_elem* elem)
// {
//     fprintf (dump_file,  "Elem address [%p]\n",   elem);
//     fprintf (dump_file,  "Elem index   [%d]\n",   elem->index);
//     fprintf (dump_file,  "Elem data    [%d]\n",   *(elem->data));
//     fprintf (dump_file,  "Elem status  [%d]\n\n", elem->status);

//     return;
// }


void print_value_chat_message(list_elem* elem) {
    fprintf (dump_file,  "Elem address [%p]\n",   elem);
    fprintf (dump_file,  "Elem index   [%d]\n",   elem->index);

    fprintf(dump_file, "MSG TYPE:       %d\n", elem->data->msg_type);
    fprintf(dump_file, "Message body:   %s\n", elem->data->msg_body);

    fprintf (dump_file,  "Elem status  [%d]\n\n", elem->status);
}

//----------------------------------------------------------------------------------------------------------------------//

// function return elem's logical order number, by giving array index
int get_elem_id (list_* list, int index, NODE_STATUS stat)
{
    LIST_VALIDATE (list);

    list_elem* head = NULL;

    if (stat == NODE_STATUS::ENGAGED)
        head = list->buffer;
    else 
        head = FREE_HEAD(list);

    int counter = 0;

    if (index == head->index)
        return counter;

    counter++;

    list_elem* current_elem = NEXT(head);

    while (current_elem != head)
    {
        if (index == current_elem->index)
            return counter;
        
        current_elem = NEXT(current_elem);
        counter++;
    }

    return static_cast<int>(LIST_ERR_CODE::WRONG_ID); // its only returned if no elem was not found
}
