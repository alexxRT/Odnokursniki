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

#define ELEM( lst, pos ) (lst->buffer  + pos)
#define NEXT( lst, pos ) (lst->buffer + (lst->buffer + pos)->next)
#define PREV( lst, pos ) (lst->buffer + (lst->buffer + pos)->prev) 

static const int HEAD_ID = 0;

void InitLogFile (const char* const log_path)
{
    log_file = fopen (log_path, "a");
    assert (log_file != NULL);
}

void DestroyLogFile ()
{
    fflush (log_file);
    fclose (log_file);
    log_file = NULL;
}

void InitDumpFile (const char* const dump_path)
{
    dump_file = fopen (dump_path, "a");
    assert (dump_file != NULL);
}

void DestroyDumpFile ()
{
    fflush (dump_file);
    fclose (dump_file);
    dump_file = NULL;
}

void InitGraphDumpFile (const char* const graph_dump_path)
{
    graph_file = fopen (graph_dump_path, "w");
    assert (graph_file != NULL);
}

void DestroyGraphDumpFile ()
{
    fflush (graph_file);
    fclose (graph_file);
    graph_file = NULL;
}

//-----------------------------------------------------------------------------------------------------------------------------//



//------------------------------------------------------LOGING ERRORS----------------------------------------------------------//

void PrintErr (my_list* list, LIST_ERR_CODE ErrCode, const int line, const char* func)
{
    assert (log_file != NULL && "Please init log file");

    switch (ErrCode)
    {
        case LIST_NULL:
            fprintf (log_file, "LIST is a NULL ptr ");
            break;
        case DATA_NULL:
            fprintf (log_file, "LIST [%p] has NULL buffer ptr ", list);
            break;
        case LIST_OVERFLOW:
            fprintf (log_file, "LIST [%p] overflowed, size - [%lu], capacity - [%lu] ", list, list->size, list->capacity);
            break;
        case LIST_UNDERFLOW:
            fprintf (log_file, "LIST [%p] underflowed, size - [%lu], capacity - [%lu] ",list, list->size, list->capacity);
            break;
        case CAPACITY_INVALID:
            fprintf (log_file, "LIST [%p] capacity equal zero - [%lu] ", list, list->capacity);
            break;
        case WRONG_SIZE:
            fprintf (log_file, "LIST [%p] has incorrect size - [%lu] ", list, list->size);
            break;
        case BROKEN_LOOP_ENGAGED:
            fprintf (log_file, "LIST [%p] the ENGAGED loop is broken ", list);
            break;
        case BROKEN_LOOP_FREE:
            fprintf (log_file, "LIST [%p] the FREE loop is broken ", list);
            break;
        case NULL_LINK:
            fprintf (log_file, "LIST [%p] one of elems has invalid NULL link ", list);
            break;
        case WRONG_POS:
            fprintf (log_file, "LIST [%p] Elem's position you use is invalid, please check your list ", list);
            break;
        case WRONG_INDX:
            fprintf (log_file, "LIST [%p] Elem's array index is invalid, please check your list ", list);
            break;
        case HEAD_DELEATE:
            fprintf (log_file, "LIST [%p] Trying to delete HEAD! ", list);
            break;
        case HEAD_INSERT:
            fprintf (log_file, "LIST [%p] Trying to insert by index in the place of HEAD! ", list);
            break;
        case SUCCESS:
            fprintf (log_file, "LIST [%p] is consistant!\n\n", list);
            break;
        default:
        {
            fprintf (log_file, "Unexpected error code is %d\n", ErrCode);
            assert (0 && "!!Unexpected error code in ErrPrint function!!\n");
        }
    }

    if (ErrCode != SUCCESS)
        fprintf (log_file, "error happend in function [%s()] and on the line [%d]\n", func, line);

}

//------------------------------------------------------------------------------------------------------------------------//



//---------------------------------------------------VALIDATION FUNCTIONS-------------------------------------------------//

LIST_ERR_CODE ListFieldsValid (my_list* list);
LIST_ERR_CODE EngagedListValid (my_list* list);
LIST_ERR_CODE FreeListValid (my_list* list);


LIST_ERR_CODE ListValid (my_list* list)
{
    LIST_ERR_CODE ErrCode = SUCCESS;

    ErrCode = ListFieldsValid  (list);
    if (ErrCode)
        return ErrCode;

    ErrCode = EngagedListValid (list);
    if (ErrCode) 
      return ErrCode;

    ErrCode = FreeListValid    (list);
    if (ErrCode) 
        return ErrCode;


    return ErrCode;
}

LIST_ERR_CODE ListFieldsValid (my_list* list)
{
    if (!list)                       return LIST_NULL;
    if (!list->buffer)               return DATA_NULL;
    if (list->size > list->capacity) return LIST_OVERFLOW;
    if (list->capacity <= 0)         return CAPACITY_INVALID; // can not create list for 0 elems, its not resizable otherwise

    return SUCCESS;
}

LIST_ERR_CODE EngagedListValid (my_list* list) { //check if loop is safe and sound, no empty links
    size_t num_elems = list->size;
    size_t counter = 0;

    for (size_t i = 0; i <= list->capacity; i ++) {
        if (list->buffer[i].status == NODE_STATUS::ENGAGED)
            counter ++;
    }

    if (num_elems != counter) {
        printf ("WRONG SIZE IN ENGAGED LIST\n");
        printf ("list size: %lu, counter %lu\n", num_elems, counter);

        return LIST_ERR_CODE::WRONG_SIZE;
    }
    
    //list cycled on its head
    if (list->size > 1 && ELEM(list, HEAD_ID)->next == HEAD_ID) {
        printf ("broke first\n");
        return LIST_ERR_CODE::BROKEN_LOOP_ENGAGED;
    }
    size_t head = HEAD_ID;
    size_t elem = head;

    // it goes cloсkwise
    for (size_t i = 0; i <= num_elems; i ++)
        elem = NEXT(list, elem)->index;

    if (elem != head) {
        printf("broke second\n");
        return LIST_ERR_CODE::BROKEN_LOOP_ENGAGED;
    }

    //it goes counter-clockwise
    for (size_t i = 0; i <= num_elems; i ++)
        elem = PREV(list, elem)->index;

    if (elem != head) {
        printf("brok third\n");
        return LIST_ERR_CODE::BROKEN_LOOP_ENGAGED;
    }

    // ? ? ? ? ? TODO IS IT CORRECT ? ? ? ? ? ?
    //double check, forward and backward
    //i store ptr, so they all should be valid   !!!
    //before it, check if size has correct value !!!
    //mark node where i started and compare where i ended
    //if they are not the same, the loop is broken, taa daaaa

    return SUCCESS;
}

LIST_ERR_CODE FreeListValid (my_list* list) {
    //free list is empty, nothing to check

    size_t free_size = list->capacity - list->size;
    size_t counter = 0;

    for (size_t i = 0; i <= list->capacity; i ++) {
        if (list->buffer[i].status == NODE_STATUS::FREE)
            counter ++;
    }

    if (counter != free_size) {
        printf ("WRONG SIZE IN FREE LIST\n");
        printf ("free list size: %lu, counter %lu\n", free_size, counter);
        return WRONG_SIZE;
    }

    if (!free_size)
        return SUCCESS;

    //free list cycled on its head
    if (ELEM(list, list->free_head_id)->next == list->free_head_id && free_size > 1)
        return BROKEN_LOOP_FREE;

    size_t head = list->free_head_id;
    size_t elem = head;

    // it goes cloсkwise
    for (size_t i = 0; i < free_size; i ++) {
        elem = NEXT(list, elem)->index;
    }

    if (elem != head)
        return BROKEN_LOOP_FREE;

    //it goes counter-clockwise
    for (size_t i = 0; i < free_size; i ++)
        elem = PREV(list, elem)->index;

    if (elem != head)
        return BROKEN_LOOP_FREE;

    return SUCCESS;
}

//-------------------------------------------------------------------------------------------------------------------------------//


//-----------------------------------------------------DUMP FUNCTIONS------------------------------------------------------------//

LIST_ERR_CODE PrintList (my_list* list, NODE_STATUS node_type);  //definition below
void PrintListElem      (my_list* list, size_t elem);                    //   ||
                                                                         //   ||
                                                                         //   \/


LIST_ERR_CODE ListTextDump (my_list* list) {

    LIST_VALIDATE (list);

    assert (dump_file != NULL && "Please init Dump file");

    fprintf (dump_file, "\n\n-------------------------LIST DUMP OCCURED-----------------------\n\n");
    fprintf (dump_file, "List address [%p]\n", list);
    fprintf (dump_file, "List head address [%p]\n", list->buffer);

    fprintf   (dump_file, "Now in the list [%lu/%lu] elems are engaged\n" , list->size, list->capacity);

    fprintf   (dump_file, "listing of engaged elems:\n");
    PrintList (list, NODE_STATUS::ENGAGED);

    fprintf   (dump_file, "Now in the list [%lu] elems are free\n", list->capacity - list->size);

    fprintf   (dump_file, "Listing of free elems:\n");
    PrintList (list, NODE_STATUS::FREE);

    fprintf   (dump_file, "\n\n-------------------------LIST DUMP FINISHED-----------------------\n\n");

    LIST_VALIDATE (list);

    return SUCCESS;    
}

//!!!GRAPHIC DUMP, SO BEAUTIFUL!!! 

int GetElemPos (my_list* list, size_t index, NODE_STATUS stat);

LIST_ERR_CODE ListGraphDump (my_list* list) {
    assert (graph_file != NULL && "Please init GraphDump file");

    //initilizing starting attributes
    fprintf (graph_file,
    "digraph {\n\
    rankdir=LR;\n\
    pack=true;\n\
    splines=ortho;\n\
    node [ shape=record ];\n\
    ");

    printf("\nSTART GRAPH DUMP\n");

    //initing each node, gives it color, data, order num
    for (size_t i = 0; i <= list->capacity; i ++) { 
        size_t pos = GetElemPos (list, i, list->buffer[i].status);

        fprintf (graph_file, "\n\tNode%zu", i);
        fprintf (graph_file, "[label = \"INDX: %zu|NUM: %lu|DATA: %d\";];\n", i, pos, list->buffer[i].data);
        fprintf (graph_file, "\tNode%zu", i);

        switch (list->buffer[i].status) {
            case NODE_STATUS::ENGAGED:
                 fprintf (graph_file, "[color = \"green\";];\n");
                 break;
            case NODE_STATUS::MASTER:
                fprintf (graph_file, "[color = \"purple\";];\n");
                break;
            case NODE_STATUS::FREE:
                fprintf(graph_file, "[color = \"red\";];\n");
        }
    }

    fprintf (graph_file, "\n");

    for (size_t i = 0; i < list->capacity; i ++)
        fprintf (graph_file, "\tNode%zu -> Node%zu[color = \"white\";];\n", i, i+1); //ordering elems as they are in ram

    fprintf (graph_file, "\n");

    for (size_t i = 0; i <= list->capacity; i++)
    {
        if (list->buffer[i].status == NODE_STATUS::ENGAGED || list->buffer[i].status == NODE_STATUS::MASTER) {
            int next_indx = NEXT(list, i)->index;
            fprintf (graph_file, "\tNode%zu -> Node%d [constraint = false;];\n", i, next_indx);
        }
        else {
            int next_indx = NEXT(list, i)->index;
            fprintf (graph_file, "\tNode%zu -> Node%d [constraint = false;];\n", i, next_indx);
        }
    }

    fprintf (graph_file, "}\n");

    return SUCCESS;
}

//-----------------------------------------------------------------------------------------------------------------------//


//------------------------------------------------------PRINT LIST FUNCTIONS---------------------------------------------//


LIST_ERR_CODE PrintList (my_list* list, NODE_STATUS node_type) {
    LIST_VALIDATE (list);

    if (node_type == NODE_STATUS::ENGAGED) {
        size_t head =       HEAD_ID;
        size_t debug_elem = ELEM(list, head)->next;

        int order_num = 1;

        while (debug_elem != head) {
            fprintf (dump_file,"\nOrder number [%d]\n", order_num);
            PrintListElem (list, debug_elem);

            order_num ++;
            debug_elem = NEXT(list, debug_elem)->index;
        }
    }
    else if (node_type == NODE_STATUS::FREE) {
        size_t head = list->free_head_id;

        if (head == HEAD_ID)
            return SUCCESS;
        
        int order_num = 1;

        fprintf (dump_file, "\nOrder number [%d]\n", order_num);
        PrintListElem (list, head);
        order_num ++;

        size_t debug_elem = ELEM(list, head)->next;

        while (debug_elem != head) {
            fprintf (dump_file, "\nOrder number [%d]\n", order_num);
            PrintListElem (list, debug_elem);

            order_num ++;
            debug_elem = NEXT(list, debug_elem)->index;
        }
    }

    LIST_VALIDATE (list);

    return SUCCESS;
}

void PrintListElem (my_list* list, size_t elem) {
    fprintf (dump_file,  "Elem address [%p]\n",   ELEM(list, elem));
    fprintf (dump_file,  "Elem index   [%lu]\n",  ELEM(list, elem)->index);
    fprintf (dump_file,  "Elem data    [%d]\n",   ELEM(list, elem)->data);
    fprintf (dump_file,  "Elem status  [%d]\n\n", ELEM(list, elem)->status);

    return;
}

//----------------------------------------------------------------------------------------------------------------------//

// function return elem's logical order number, by giving array index
int GetElemPos (my_list* list, size_t index, NODE_STATUS stat) {
    LIST_VALIDATE (list);

    size_t head = 0;

    if (stat == NODE_STATUS::ENGAGED ||
        stat == NODE_STATUS::MASTER)
        head = HEAD_ID;
    else 
        head = list->free_head_id;

    size_t counter = 0;

    if (index == ELEM(list, head)->index)
        return counter;

    counter++;

    size_t current_elem = ELEM(list, head)->next;

    while (current_elem != head) {
        if (index == current_elem)
            return counter;
        
        current_elem = ELEM(list, current_elem)->next;
        //current_elem = NEXT(list, current_elem)->index;

        if (counter >= list->capacity)
            break;

        counter++;
    }

    return WRONG_INDX; // its only returned if no elem was not found
}
