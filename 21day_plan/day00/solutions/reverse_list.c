#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int value;
    struct Node *next;
} Node;

static Node *build_list(const int *arr, int n)
{
    Node *head = NULL;
    Node *tail = NULL;

    for (int i = 0; i < n; i++) {
        Node *node = malloc(sizeof(*node));
        if (node == NULL) {
            return head;
        }

        node->value = arr[i];
        node->next = NULL;

        if (head == NULL) {
            head = node;
            tail = node;
        } else {
            tail->next = node;
            tail = node;
        }
    }
    return head;
}

static Node *reverse_list(Node *head)
{
    Node *prev = NULL;
    Node *curr = head;
    
    while (curr != NULL) {
        Node *next = curr->next;
        curr->next = prev;
        prev = curr;
        curr = next;
    }
    return prev;
}

static void print_list(const Node *head)
{
    while (head != NULL) {
        printf("%d ", head->value);
        head = head->next;
    }
    printf("\n");
}

static void free_list(Node *head)
{
    while (head != NULL) {
        Node *next = head->next;
        free(head);
        head = next;
    }
}

int main(void)
{
    int arr[] = {10, 8, 17, 13, 19};
    int n = (int)(sizeof(arr) / sizeof(arr[0]));
    Node *head = build_list(arr, n);

    print_list(head);

    Node *rev = reverse_list(head);
    print_list(rev);
    free_list(rev);
    return 0;
}
