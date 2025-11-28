#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct {
    LARGE_INTEGER t;
} hi_time;

static double hi_time_elapsed(hi_time start, hi_time end) {
    static LARGE_INTEGER freq = {0};
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    return (double)(end.t.QuadPart - start.t.QuadPart) / (double)freq.QuadPart;
}

static hi_time hi_time_now(void) {
    hi_time ht;
    QueryPerformanceCounter(&ht.t);
    return ht;
}

#else
#include <time.h>

typedef struct {
    struct timespec t;
} hi_time;

static double hi_time_elapsed(hi_time start, hi_time end) {
    return (end.t.tv_sec - start.t.tv_sec)
         + (end.t.tv_nsec - start.t.tv_nsec) * 1e-9;
}

static hi_time hi_time_now(void) {
    hi_time ht;
    clock_gettime(CLOCK_MONOTONIC, &ht.t);
    return ht;
}

#endif

