#ifndef LIST_DEBUG
#define LIST_DEBUG

#include "list.h"

typedef void (*print_value_t)(list_elem*);


void init_log_file (const char* const log_path);
void destroy_log_file ();
void init_dump_file (const char* const dump_path);
void destroy_dump_file ();
void init_graph_dump_file (const char* const graph_dump_path);
void destroy_graph_dump_file ();

LIST_ERR_CODE list_text_dump  (list_* list);
LIST_ERR_CODE list_graph_dump (list_* list);

LIST_ERR_CODE print_list (list_* list, NODE_STATUS node_type, print_value_t print_val);
void          print_value_chat_message(list_elem* elem);
//void          print_value_int (list_elem* elem);



LIST_ERR_CODE list_valid (list_* list);
void          print_err  (list_* list, LIST_ERR_CODE ErrCode, const int line, const char* func);


#ifdef DEBUG_VERSION //this custom assert will print error that happened and 'll exit function
#define LIST_VALIDATE( lst_ptr )                                 \
do{                                                              \
    LIST_ERR_CODE ErrCode = list_valid (lst_ptr);                 \
    if (ErrCode != SUCCESS)                                      \
        {                                                        \
            print_err (lst_ptr, ErrCode, __LINE__, __func__);     \
            return ErrCode;                                      \
        }                                                        \
}                                                                \
while (0)                                                        \

#else
#define LIST_VALIDATE( lst_ptr ) (void*)0
#endif




#endif