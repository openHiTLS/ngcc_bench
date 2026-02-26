#ifndef MEM_STAT_H
#define MEM_STAT_H

#include <stdint.h>

uint64_t ngcc_mem_current_rss_bytes(void);
uint64_t ngcc_mem_peak_rss_bytes(void);
uint64_t ngcc_mem_heap_bytes(void);

#endif
