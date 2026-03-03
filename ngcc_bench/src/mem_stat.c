#include "mem_stat.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

int ngcc_mem_analyze_static(const char *lib_path, ngcc_static_mem_t *out) {
#ifdef __linux__
    char cmd[1024];
    char line[256];
    FILE *fp;

    if (lib_path == NULL || out == NULL) {
        return -1;
    }

    memset(out, 0, sizeof(*out));

    snprintf(cmd, sizeof(cmd), "size -A %s 2>/dev/null", lib_path);
    fp = popen(cmd, "r");
    if (fp == NULL) {
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char name[64];
        unsigned long long sz = 0;

        if (sscanf(line, "%63s %llu", name, &sz) != 2) {
            continue;
        }
        if (strcmp(name, ".text") == 0) {
            out->text_size = (uint64_t) sz;
        } else if (strcmp(name, ".data") == 0) {
            out->data_size = (uint64_t) sz;
        } else if (strcmp(name, ".bss") == 0) {
            out->bss_size = (uint64_t) sz;
        } else if (strcmp(name, ".rodata") == 0) {
            out->rodata_size = (uint64_t) sz;
        }
    }
    pclose(fp);

    out->total = out->text_size + out->data_size + out->bss_size + out->rodata_size;
    return 0;
#else
    (void) lib_path;
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    return -1;
#endif
}
