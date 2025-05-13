

#include <rtthread.h>
#include "mem_section.h"

#define EPUB_MEMHEAP_POOL_SIZE (2*1024*1024)

struct rt_memheap epub_psram_memheap;

L2_NON_RET_BSS_SECT_BEGIN(epub_memheap)
L2_NON_RET_BSS_SECT(epub_memheap, ALIGN(4) static uint8_t epub_psram_memheap_pool[EPUB_MEMHEAP_POOL_SIZE]);
L2_NON_RET_BSS_SECT_END

static int app_cahe_memheap_init(void)
{
    rt_memheap_init(&epub_psram_memheap, "epub_psram_memheap", (void *)epub_psram_memheap_pool, EPUB_MEMHEAP_POOL_SIZE);

    return 0;
}
INIT_PREV_EXPORT(app_cahe_memheap_init);

static int is_memheap_addr(uint8_t *ptr)
{
    if((ptr >= &epub_psram_memheap_pool[0]) &&
        (ptr < &epub_psram_memheap_pool[EPUB_MEMHEAP_POOL_SIZE]))
        return 1;
    else
        return 0;
}

void *epub_mem_malloc(size_t size)
{
    uint8_t *p = NULL;

    p = (uint8_t *)rt_memheap_alloc(&epub_psram_memheap, size);

    if (!p)
    {
        rt_kprintf("epub_mem_alloc: size %d failed!", size);
        return 0;
    }

    return p;
}

void epub_mem_free(void *p)
{
    rt_memheap_free(p);
}

void *epub_mem_realloc(void *rmem, size_t newsize)
{
    return rt_memheap_realloc(&epub_psram_memheap, rmem, newsize);
}


void *epub_mem_calloc(size_t nelem, size_t elsize)
{
    return rt_memheap_calloc(&epub_psram_memheap, nelem, elsize);
}

void *cxx_mem_allocate(size_t size)
{
    return epub_mem_malloc(size);
}

void cxx_mem_free(void *ptr)
{
    return epub_mem_free(ptr);
}