#include "mem_stat.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __linux__
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
#include <malloc.h>
#define HAVE_MALLINFO2 1
#else
#define HAVE_MALLINFO2 0
#endif

#ifdef __linux__
static FILE *open_size_output(const char *lib_path, pid_t *out_pid) {
    int pipefd[2];
    int devnull_fd;
    pid_t pid;
    FILE *fp;

    if (lib_path == NULL || out_pid == NULL) {
        return NULL;
    }
    if (pipe(pipefd) != 0) {
        return NULL;
    }

    pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        char *const argv[] = {"size", "-A", (char *) lib_path, NULL};

        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
            _exit(127);
        }

        devnull_fd = open("/dev/null", O_WRONLY);
        if (devnull_fd >= 0) {
            if (dup2(devnull_fd, STDERR_FILENO) < 0) {
                close(devnull_fd);
                _exit(127);
            }
            close(devnull_fd);
        }

        close(pipefd[1]);
        execvp(argv[0], argv);
        _exit(127);
    }

    close(pipefd[1]);
    fp = fdopen(pipefd[0], "r");
    if (fp == NULL) {
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
        return NULL;
    }

    *out_pid = pid;
    return fp;
}

static int close_size_output(FILE *fp, pid_t pid) {
    int status;
    int rc = 0;

    if (fp == NULL) {
        return -1;
    }
    if (fclose(fp) != 0) {
        rc = -1;
    }
    if (waitpid(pid, &status, 0) < 0) {
        return -1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return -1;
    }
    return rc;
}
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

uint64_t ngcc_mem_vm_peak_bytes(void) {
#ifdef __linux__
    FILE *fp;
    char line[256];
    uint64_t vm_peak_kb = 0;

    fp = fopen("/proc/self/status", "r");
    if (fp == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "VmPeak:", 7) == 0) {
            unsigned long long val = 0;
            if (sscanf(line + 7, " %llu", &val) == 1) {
                vm_peak_kb = (uint64_t) val;
            }
            break;
        }
    }

    fclose(fp);
    return vm_peak_kb * 1024ULL;
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
    char line[256];
    FILE *fp;
    pid_t child_pid;

    if (lib_path == NULL || out == NULL) {
        return -1;
    }

    memset(out, 0, sizeof(*out));

    fp = open_size_output(lib_path, &child_pid);
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
    if (close_size_output(fp, child_pid) != 0) {
        memset(out, 0, sizeof(*out));
        return -1;
    }

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
