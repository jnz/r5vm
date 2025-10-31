int counter;          // test .bss
int initialized = 42; // test .data

static inline void vm_putchar(char c)
{
    register int a0 asm("a0") = c;
    register int a7 asm("a7") = 1; // host syscall id
    asm volatile ("ecall" : : "r"(a0), "r"(a7));
}

static void print(const char *s)
{
    while (*s) vm_putchar(*s++);
}

int main(void)
{
    counter += initialized; // 42
    print("Hello, World!\n");

    float f = 1.0f;
    for (int i = 0; i < 10; i++)
    {
        f += 0.5f;
    }
    if (f > 2.0f)
    {
        print("Float operations successful.\n");
    }
    else
    {
        print("Float operations failed.\n");
    }
    print("Counter value\n");

    return 0;
}

