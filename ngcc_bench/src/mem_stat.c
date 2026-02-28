#include "mem_stat.h"

#include <stdint.h>

#ifdef __linux__
#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
#include <malloc.h>
#define HAVE_MALLINFO2 1
#else
#define HAVE_MALLINFO2 0
#endif

uint64_t ngcc_mem_current_rss_bytes(void) {
#ifdef __linux__
    FILE *fp = fopen("/proc/self/statm", "r");
    unsigned long total_pages = 0;
    unsigned long rss_pages = 0;
    long page_size;

    if (fp == NULL) {
        return 0;
    }

    if (fscanf(fp, "%lu %lu", &total_pages, &rss_pages) != 2) {
        fclose(fp);
        return 0;
    }

    fclose(fp);

    page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        return 0;
    }

    return (uint64_t) rss_pages * (uint64_t) page_size;
#else
    return 0;
#endif
}

uint64_t ngcc_mem_peak_rss_bytes(void) {
#ifdef __linux__
    struct rusage usage;

    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return 0;
    }

    return (uint64_t) usage.ru_maxrss * 1024ULL;
#else
    return 0;
#endif
}

uint64_t ngcc_mem_heap_bytes(void) {
#if HAVE_MALLINFO2
    struct mallinfo2 mi = mallinfo2();
    return (uint64_t) mi.uordblks;
#else
    return 0;
#endif
}
