#include <stddef.h>
#include <stdio.h>

static void *my_memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    if (d == s || n == 0) {
        return dst;
    }

    if (d < s) {
        while (n-- > 0) {
            *d++ = *s++;
        }
    } else {
        d += n;
        s += n;
        while (n-- > 0) {
            *--d = *--s;
        }
    }
    return dst;
}

int main(void)
{
    char buf[] = "0123456789";

    my_memmove(buf + 2, buf, 5);
    printf("%s\n", buf);
    return 0;
}
