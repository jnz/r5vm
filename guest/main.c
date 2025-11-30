int counter;          // test .bss
int initialized = 42; // test .data
const int readonly = 23;

static void vm_putchar(char c)
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
    print("Hello, world!\n");

    return 0;
}

