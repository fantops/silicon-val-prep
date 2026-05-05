#include <stddef.h>
#include <stdio.h>

struct PackedByOrder {
    char c;
    int x;
};

struct BetterOrder {
    int x;
    char c;
};

static size_t misleading_count_from_pointer(int *arr)
{
    size_t pointer_bytes = sizeof(arr);

    return pointer_bytes / sizeof(arr[0]);
}

int main(void)
{
    int arr[5] = {1, 2, 3, 4, 5};

    printf("sizeof arr in main: %zu elements\n", sizeof(arr) / sizeof(arr[0]));
    printf("misleading pointer-derived count: %zu elements (array decayed)\n",
           misleading_count_from_pointer(arr));
    printf("sizeof struct PackedByOrder: %zu, x offset: %zu\n",
           sizeof(struct PackedByOrder), offsetof(struct PackedByOrder, x));
    printf("sizeof struct BetterOrder: %zu, c offset: %zu\n",
           sizeof(struct BetterOrder), offsetof(struct BetterOrder, c));
    return 0;
}
