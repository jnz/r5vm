#ifndef QVMLIB_H
#define QVMLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stddef.h>

// --- Math ---------------------------------------------------------------
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float sqrtf(float x);
float fmodf(float x, float y);
float sinf(float x);
float cosf(float x);
float fabsf(float x);

// --- String -------------------------------------------------------------
size_t strlen(const char *s);
char  *strcpy(char *dst, const char *src);
char  *strcat(char *dst, const char *src);
int    strcmp(const char *a, const char *b);

// --- Conversion ---------------------------------------------------------
float atof(const char *s);
int   atoi(const char *s);
char *itoa(int value, char *buf, int base);

// --- Misc ---------------------------------------------------------------
int   abs(int n);
void  srand(unsigned seed);
int   rand(void);

// --- Formatted output ---------------------------------------------------
int vsprintf(char *buffer, const char *fmt, va_list ap);

// Optional convenience wrapper:
int sprintf(char *buffer, const char *fmt, ...);

// --- Memory -------------------------------------------------------------
void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
int   memcmp(const void *a, const void *b, size_t n);
void *memmove(void *dst, const void *src, size_t n);

#ifdef __cplusplus
}
#endif

#endif // QVMLIB_H
