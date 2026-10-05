// CPU time and memory of the benchmark process, shared by every benchmark program.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#endif
#if defined(__APPLE__)
#include <mach/mach.h>
#endif

// CPU time of every thread in the process, user + system
static inline double bench_process_cpu_ms()
{
#if defined(_WIN32)
    FILETIME create_time, exit_time, kernel_time, user_time;
    GetProcessTimes(GetCurrentProcess(), &create_time, &exit_time, &kernel_time, &user_time);
    const auto ms = [](const FILETIME& t) { return static_cast<double>((static_cast<uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime) / 1e4; };
    return ms(kernel_time) + ms(user_time);
#else
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return static_cast<double>(usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1e3
        + static_cast<double>(usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1e3;
#endif
}

// CPU time of the calling thread
static inline double bench_thread_cpu_ms()
{
#if defined(_WIN32)
    FILETIME create_time, exit_time, kernel_time, user_time;
    GetThreadTimes(GetCurrentThread(), &create_time, &exit_time, &kernel_time, &user_time);
    const auto ms = [](const FILETIME& t) { return static_cast<double>((static_cast<uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime) / 1e4; };
    return ms(kernel_time) + ms(user_time);
#else
    struct timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return static_cast<double>(ts.tv_sec) * 1e3 + static_cast<double>(ts.tv_nsec) / 1e6;
#endif
}

// Memory the process holds right now and its high-water mark, in MB. macOS: physical footprint (what Activity Monitor
// shows; freed heap pages stop counting at once). Linux: resident set. Windows: working set.
static inline void bench_memory_mb(double& current_mb, double& peak_mb)
{
    current_mb = peak_mb = 0;
#if defined(__APPLE__)
    task_vm_info_data_t info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
        current_mb = static_cast<double>(info.phys_footprint) / 1048576.0;
        peak_mb = static_cast<double>(info.ledger_phys_footprint_peak) / 1048576.0;
    }
#elif defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        current_mb = static_cast<double>(counters.WorkingSetSize) / 1048576.0;
        peak_mb = static_cast<double>(counters.PeakWorkingSetSize) / 1048576.0;
    }
#else
    FILE* f = fopen("/proc/self/status", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                current_mb = static_cast<double>(atol(line + 6)) / 1024.0;
            } else if (strncmp(line, "VmHWM:", 6) == 0) {
                peak_mb = static_cast<double>(atol(line + 6)) / 1024.0;
            }
        }
        fclose(f);
    }
#endif
}

// high-water mark of the process memory in MB, printed after each test (the 4 parameter test runs first)
static inline double bench_peak_mb()
{
    double current_mb, peak_mb;
    bench_memory_mb(current_mb, peak_mb);
    return peak_mb;
}
