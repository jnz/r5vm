#include "qvmlib.h"

static volatile uint64_t sink64;
static volatile float sinkf;

static void compute_int()
{
    volatile uint64_t a = 0x123456789abcdefULL;
    volatile uint64_t b = 0xfedcba987654321ULL;
    for (int i = 0; i < 500000; ++i) {
        a += b ^ (a >> 7);
        b = (b << 3) ^ (a * 0x9e3779b97f4a7c15ULL);
        a = (a ^ b) + (a >> 5);
        if ((i & 1023) == 0) a ^= b >> 11;
    }
    sink64 = a + b;
}

static void compute_fp()
{
    volatile float x = 0.1f, y = 1.1f, z = 0.5f;
    for (int i = 0; i < 30000; ++i) {
        x = sinf(y) + cosf(z) * 0.3f;
        y = x * y + sqrtf(fabsf(z) + 1.0f);
        z = fmodf(y + z * 0.5f, 3.14159f);
        if (z < 0.001f) z += 1.0f;
    }
    sinkf = x + y + z;
}

static void compute_mem()
{
    size_t N = 256 * 1024;
    static uint32_t v[256 * 1024];

    for (size_t i = 0; i < N; ++i)
        v[i] = (uint32_t)(i * 2654435761u);

    volatile uint64_t sum = 0;

    for (int it = 0; it < 2; ++it) {
        for (size_t i = 0; i < N; ++i)
            sum += (v[(i + it * 13) & (N - 1)] ^ it) + 1;

        size_t k = (size_t)(it & 255);
        if (k) {
            uint32_t tmp[k];
            for (size_t i = 0; i < k; ++i)
                tmp[i] = v[i];
            for (size_t i = 0; i < N - k; ++i)
                v[i] = v[i + k];
            for (size_t i = 0; i < k; ++i)
                v[N - k + i] = tmp[i];
        }
    }

    sink64 = sum ^ v[17];
}

static void compute_branch()
{
    volatile unsigned int acc = 0;
    for (unsigned int i = 0; i < 100'000; ++i) {
        if ((i * 1103515245 + 12345) & 0x40000000)
            acc += i & 255;
        else if (i & 1)
            acc ^= i;
        else
            acc -= i & 7;
    }
    sink64 = acc;
}

int main()
{
    compute_int();
    compute_fp();
    compute_mem();
    compute_branch();

    return 0;
}

