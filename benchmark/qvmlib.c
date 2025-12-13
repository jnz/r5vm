#include "qvmlib.h"
#include <stddef.h>
#include <stdarg.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Math ---------------------------------------------------------------

float sqrtf(float x) {
    if (x <= 0.0f) return 0.0f;
    float y = x;
    for (int i = 0; i < 5; ++i)
        y = 0.5f * (y + x / y);
    return y;
}

float fmodf(float x, float y) {
    if (y == 0.0f) return 0.0f;
    long n = (long)(x / y);
    return x - (float)n * y;
}

static float _wrap_angle(float x) {
    const float two_pi = 2.0f * (float)M_PI;
    while (x >  M_PI) x -= two_pi;
    while (x < -M_PI) x += two_pi;
    return x;
}

float sinf(float x) {
    x = _wrap_angle(x);
    float x2 = x * x;
    return x * (1.0f - x2 * (1.0f/6.0f - x2 * (1.0f/120.0f - x2 * (1.0f/5040.0f))));
}

float cosf(float x) {
    return sinf(x + (float)M_PI * 0.5f);
}

// --- String -------------------------------------------------------------

size_t strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++));
    return dst;
}

char *strcat(char *dst, const char *src) {
    char *d = dst;
    while (*d) d++;
    while ((*d++ = *src++));
    return dst;
}

int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

// --- Rand ---------------------------------------------------------------

static int randSeed = 1;

void srand(unsigned seed) {
    randSeed = (int)seed;
}

int rand(void) {
    randSeed = (69069 * randSeed + 1);
    return randSeed & 0x7fff;
}

// --- Conversion ---------------------------------------------------------

float atof(const char *s) {
    float val = 0.0f, sign = 1.0f, scale = 1.0f;
    int exp_sign = 1, exp_val = 0;

    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;

    if (*s == '-') { sign = -1.0f; s++; }
    else if (*s == '+') s++;

    while (*s >= '0' && *s <= '9')
        val = val * 10.0f + (*s++ - '0');

    if (*s == '.') {
        s++;
        float div = 10.0f;
        while (*s >= '0' && *s <= '9') {
            val += (*s++ - '0') / div;
            div *= 10.0f;
        }
    }

    if (*s == 'e' || *s == 'E') {
        s++;
        if (*s == '-') { exp_sign = -1; s++; }
        else if (*s == '+') s++;
        while (*s >= '0' && *s <= '9')
            exp_val = exp_val * 10 + (*s++ - '0');
        while (exp_val--)
            scale = exp_sign > 0 ? scale * 10.0f : scale / 10.0f;
    }

    return sign * val * scale;
}

int atoi(const char *s) {
    int neg = 0, val = 0;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        val = val * 10 + (*s++ - '0');
    return neg ? -val : val;
}

char *itoa(int value, char *buf, int base) {
    static const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char tmp[32];
    int i = 0;
    unsigned int v = (value < 0 && base == 10) ? -value : (unsigned)value;

    if (base < 2 || base > 36) {
        buf[0] = '\0';
        return buf;
    }

    do {
        tmp[i++] = digits[v % base];
        v /= base;
    } while (v && i < (int)sizeof(tmp));

    if (value < 0 && base == 10)
        tmp[i++] = '-';

    int j = 0;
    while (i--)
        buf[j++] = tmp[i];
    buf[j] = '\0';
    return buf;
}

int abs(int n) {
    return n < 0 ? -n : n;
}

float fabsf(float x) {
    return x < 0.0f ? -x : x;
}

// --- vsprintf -----------------------------------------------------------

static void puts_buf(char **buf, const char *s) {
    while (*s) {
        *(*buf)++ = *s++;
    }
}

static void putu_buf(char **buf, unsigned int val, int base, int uppercase) {
    char tmp[16];
    int i = 0;
    do {
        int d = val % base;
        tmp[i++] = (d < 10) ? '0' + d : (uppercase ? 'A' : 'a') + d - 10;
        val /= base;
    } while (val && i < (int)sizeof(tmp));
    while (i--) {
        *(*buf)++ = tmp[i];
    }
}

int vsprintf(char *buffer, const char *fmt, va_list ap) {
    char *buf = buffer;
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            *buf++ = *fmt;
            continue;
        }
        fmt++;
        switch (*fmt) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            puts_buf(&buf, s);
            break;
        }
        case 'i':
        case 'd': {
            int v = va_arg(ap, int);
            if (v < 0) { *buf++ = '-'; v = -v; }
            putu_buf(&buf, (unsigned)v, 10, 0);
            break;
        }
        case 'u':
            putu_buf(&buf, va_arg(ap, unsigned), 10, 0);
            break;
        case 'x':
            putu_buf(&buf, va_arg(ap, unsigned), 16, 0);
            break;
        case 'X':
            putu_buf(&buf, va_arg(ap, unsigned), 16, 1);
            break;
        case 'c':
            *buf++ = (char)va_arg(ap, int);
            break;
        case '%':
            *buf++ = '%';
            break;
        default:
            *buf++ = '%';
            *buf++ = *fmt;
            break;
        }
    }
    *buf = '\0';
    return (int)(buf - buffer);
}

int sprintf(char *buffer, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsprintf(buffer, fmt, ap);
    va_end(ap);
    return n;
}

// --- Memory -------------------------------------------------------------

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--)
        *d++ = *s++;
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    while (n--)
        *d++ = (unsigned char)c;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *p1 = (const unsigned char *)a;
    const unsigned char *p2 = (const unsigned char *)b;
    while (n--) {
        if (*p1 != *p2)
            return (int)*p1 - (int)*p2;
        p1++;
        p2++;
    }
    return 0;
}

void* memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    if (d == s || n == 0)
        return dst;

    if (d < s) {
        while (n--)
            *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--)
            *--d = *--s;
    }
    return dst;
}

