#include "qvmlib.h"

static void vm_putchar(char c)
{
    register int a0 asm("a0") = c;
    register int a7 asm("a7") = 1; // host syscall id
    asm volatile ("ecall" : : "r"(a0), "r"(a7));
}

static void print(const char *s)
{
    while (*s) {
        vm_putchar(*s++);
    }
}

static inline void riscv_ebreak(const char* errmsg)
{
    __asm__ volatile ("ebreak");
}

void fizzbuff(int upto)
{
    for (int i = 1; i <= upto; ++i) {
        if (i % 15 == 0) print(", FizzBuzz");
        else if (i % 3 == 0) print(", Fizz");
        else if (i % 5 == 0) print(", Buzz");
        else {
            char out[32];
            sprintf(out, "%s%i", i>1?", ":"", i);
            print(out);
        }
    }
    print("\n");
}

static void compute_int32()
{
    volatile uint32_t a = 0x89abcdefU;
    volatile uint32_t b = 0x87654321U;
    for (int i = 0; i < 500000; ++i) {
        a += b ^ (a >> 7);
        b = (b << 3) ^ (a * 0x7f4a7c15U);
        a = (a ^ b) + (a >> 5);
        if ((i & 1023) == 0) a ^= b >> 11;
    }
    static volatile uint32_t sink32;
    sink32 = a + b;
}

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
    static volatile uint64_t sink64;
    sink64 = a + b;
    // Result compute_int 1977415932351729775
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
    static volatile float sinkf;
    sinkf = x + y + z;
    // Result compute_fp 5.129297
}

static void compute_mem()
{
    static uint32_t v[16 * 1024];
    size_t N = sizeof(v)/sizeof(v[0]);

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

    static volatile uint64_t sink64;
    sink64 = sum ^ v[17];
    // Result compute_mem 70356638551666
}

static void compute_branch()
{
    volatile unsigned int acc = 0;
    for (unsigned int i = 0; i < 100000; ++i) {
        if ((i * 1103515245 + 12345) & 0x40000000)
            acc += i & 255;
        else if (i & 1)
            acc ^= i;
        else
            acc -= i & 7;
    }
    static volatile uint64_t sink64;
    sink64 = acc;
    //Result compute_branch 5466932
}

int main()
{
    fizzbuff(15);
    compute_int32();
    compute_int();
    compute_fp();
    compute_mem();
    compute_branch();

    return 0;
}

