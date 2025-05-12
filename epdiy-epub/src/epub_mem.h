

#ifndef EPUB_MEM_H
#define EPUB_MEM_H
#include "rtthread.h"

#ifdef __cplusplus
extern "C"
{
#endif


void *epub_mem_malloc(size_t size);
void epub_mem_free(void *p);
void *epub_mem_realloc(void *rmem, size_t newsize);
void *epub_mem_calloc(size_t nelem, size_t elsize);
#ifdef __cplusplus
}
#endif
#endif