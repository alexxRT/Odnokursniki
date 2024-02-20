#ifndef MEMORY_H
#define MEMORY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static const void* POISNED_PTR = (const void*)0x78;
static const char POISNED_ELEM = 0x77;

#if defined(__APPLE__) //Mac OS X build
#include <malloc/malloc.h>
#define FREE( ptr ) { memset (ptr, POISNED_ELEM, malloc_size (ptr)); free (ptr); }

#elif defined(__gnu_linux__) //Linux build
#include <malloc.h>
#define FREE( ptr ) { memset (ptr, POISNED_ELEM, malloc_usable_size(ptr)); free (ptr); }

#elif defined(_WIN32) //Windows build, never tried, use mac os or linux instead ANYWAY
#include <malloc.h>
#define FREE( ptr ) { memset (ptr, POISNED_ELEM, _msize(ptr)); free (ptr); }

#endif

#define CALLOC( count, type )            (type*)calloc(count, sizeof(type))

#endif
