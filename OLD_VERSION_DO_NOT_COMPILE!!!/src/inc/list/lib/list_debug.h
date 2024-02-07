#ifndef LIST_DEBUG
#define LIST_DEBUG


// TO DO 
// add flag that turns on/off debug logs and outputs
// list will be still verified but without any text warnings


void InitLogFile (const char* const log_path);
void DestroyLogFile ();
void InitDumpFile (const char* const dump_path);
void DestroyDumpFile ();
void InitGraphDumpFile (const char* const graph_dump_path);
void DestroyGraphDumpFile ();

LIST_ERR_CODE ListTextDump (my_list* list);
LIST_ERR_CODE ListGraphDump (my_list* list);

LIST_ERR_CODE PrintList     (my_list* list, NODE_STATUS node_type);  //definition below
void          PrintListElem (my_list* list, size_t elem);


LIST_ERR_CODE ListValid (my_list* list);
void          PrintErr (my_list* list, LIST_ERR_CODE ErrCode, const int line, const char* func);


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




#endif