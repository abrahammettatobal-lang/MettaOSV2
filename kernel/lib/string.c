/* Freestanding string helpers for kernel link (SA-03). */
#include <stddef.h>

void *memcpy(void *dst, const void *src, size_t n)
{
    void *d = dst;
    size_t words = n / 8;
    __asm__ volatile("rep movsq" : "+D"(d), "+S"(src), "+c"(words) :: "memory");
    n &= 7;
    __asm__ volatile("rep movsb" : "+D"(d), "+S"(src), "+c"(n) :: "memory");
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    unsigned char *d = dst;
    unsigned char  b = (unsigned char)c;

    while (n-- != 0) {
        *d++ = b;
    }
    return dst;
}

size_t strlen(const char *s)
{
    const char *p = s;

    while (*p != '\0') {
        p++;
    }
    return (size_t)(p - s);
}

int strcmp(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (int)((unsigned char)*a - (unsigned char)*b);
}

char *strchr(const char *s, int c)
{
    char ch = (char)c;

    while (*s != '\0') {
        if (*s == ch) {
            return (char *)s;
        }
        s++;
    }
    return (ch == '\0') ? (char *)s : NULL;
}

char *strstr(const char *haystack, const char *needle)
{
    if (needle[0] == '\0') {
        return (char *)haystack;
    }
    for (; *haystack != '\0'; haystack++) {
        const char *h = haystack;
        const char *n = needle;

        while (*h != '\0' && *n != '\0' && *h == *n) {
            h++;
            n++;
        }
        if (*n == '\0') {
            return (char *)haystack;
        }
    }
    return NULL;
}
