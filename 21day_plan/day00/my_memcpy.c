#include <stddef.h>
#include <stdio.h>

static void *my_memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    while (n-- > 0) {
        *d++ = *s++;
    }
    return dst;
}

int main(void)
{
    char src[] = "embedded-c";
    char dst[sizeof(src)] = {0};

    my_memcpy(dst, src, sizeof(src));
    printf("%s\n", dst);
    return 0;
}
