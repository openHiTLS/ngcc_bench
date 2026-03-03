#ifndef MEM_STAT_H
#define MEM_STAT_H

#include <stdint.h>

/* ── Static memory analysis (ELF segments) ────────────────────── */

typedef struct {
    uint64_t text_size;     /* .text 代码段 */
    uint64_t data_size;     /* .data 已初始化数据 */
    uint64_t bss_size;      /* .bss 未初始化数据 */
    uint64_t rodata_size;   /* .rodata 只读数据 */
    uint64_t total;         /* 总静态内存 */
} ngcc_static_mem_t;

int ngcc_mem_analyze_static(const char *lib_path, ngcc_static_mem_t *out);

/* ── Dynamic memory queries ───────────────────────────────────── */

uint64_t ngcc_mem_current_rss_bytes(void);
uint64_t ngcc_mem_peak_rss_bytes(void);
uint64_t ngcc_mem_heap_bytes(void);

#endif
