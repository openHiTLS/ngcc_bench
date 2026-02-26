#include "cycle_counter.h"

#include <linux/perf_event.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#if defined(__x86_64__)
static unsigned long long read_tsc(void) {
    unsigned int lo;
    unsigned int hi;
    __asm__ volatile ("lfence\n\t"
                      "rdtsc"
                      : "=a"(lo), "=d"(hi)
                      :
                      : "memory");
    return ((unsigned long long) hi << 32) | lo;
}
#endif

#if defined(__aarch64__)
static unsigned long long read_armv8_cntvct(void) {
    unsigned long long counter;
    __asm__ volatile ("isb" ::: "memory");
    __asm__ volatile ("mrs %0, cntvct_el0" : "=r"(counter));
    return counter;
}
#endif

void cycle_counter_close(cycle_counter_t *counter) {
    if (counter->perf_fd >= 0) {
        close(counter->perf_fd);
    }
    counter->perf_fd = -1;
    counter->source = CYCLE_SOURCE_NONE;
}

int cycle_counter_open(cycle_counter_t *counter, int cycles_enabled) {
    struct perf_event_attr pe;

    counter->source = CYCLE_SOURCE_NONE;
    counter->perf_fd = -1;

    if (!cycles_enabled) {
        return 0;
    }

    memset(&pe, 0, sizeof(pe));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(pe);
    pe.config = PERF_COUNT_HW_CPU_CYCLES;
    pe.disabled = 1;
    pe.exclude_kernel = 0;
    pe.exclude_hv = 0;

    counter->perf_fd = (int) syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
    if (counter->perf_fd >= 0) {
        counter->source = CYCLE_SOURCE_PERF;
        return 0;
    }

#if defined(__x86_64__)
    counter->source = CYCLE_SOURCE_TSC;
    return 0;
#elif defined(__aarch64__)
    counter->source = CYCLE_SOURCE_ARMV8_CNTVCT;
    return 0;
#else
    return -1;
#endif
}

unsigned long long cycle_counter_begin(cycle_counter_t *counter) {
    if (counter->source == CYCLE_SOURCE_PERF) {
        ioctl(counter->perf_fd, PERF_EVENT_IOC_RESET, 0);
        ioctl(counter->perf_fd, PERF_EVENT_IOC_ENABLE, 0);
        return 0;
    }

#if defined(__x86_64__)
    if (counter->source == CYCLE_SOURCE_TSC) {
        return read_tsc();
    }
#endif

#if defined(__aarch64__)
    if (counter->source == CYCLE_SOURCE_ARMV8_CNTVCT) {
        return read_armv8_cntvct();
    }
#endif

    return 0;
}

unsigned long long cycle_counter_end(cycle_counter_t *counter, unsigned long long start_cycles) {
    unsigned long long cycles = 0;

    if (counter->source == CYCLE_SOURCE_PERF) {
        ioctl(counter->perf_fd, PERF_EVENT_IOC_DISABLE, 0);
        if (read(counter->perf_fd, &cycles, sizeof(cycles)) != (ssize_t) sizeof(cycles)) {
            return 0;
        }
        return cycles;
    }

#if defined(__x86_64__)
    if (counter->source == CYCLE_SOURCE_TSC) {
        unsigned long long end_cycles = read_tsc();
        return end_cycles - start_cycles;
    }
#endif

#if defined(__aarch64__)
    if (counter->source == CYCLE_SOURCE_ARMV8_CNTVCT) {
        unsigned long long end_cycles = read_armv8_cntvct();
        return end_cycles - start_cycles;
    }
#endif

    return 0;
}
