#include "memory.h"
#include "list.h"
#include "list_debug.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

///----------------------------------------------------FILES FOR DEBUG PRINT-------------------------------------------//

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
