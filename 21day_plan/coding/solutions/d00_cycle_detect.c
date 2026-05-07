#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int value;
    struct Node *next;
} Node;

static Node *new_node(int value)
{
    Node *node = malloc(sizeof(*node));
    if (node == NULL) {
        return NULL;
    }
    node->value = value;
    node->next = NULL;
    return node;
}

static bool has_cycle(const Node *head)
{
    const Node *slow = head;
    const Node *fast = head;

    while (fast != NULL && fast->next != NULL) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) {
            return true;
        }
    }
    return false;
}

int main(void)
{
    Node *a = new_node(1);
    Node *b = new_node(2);
    Node *c = new_node(3);

    if (a == NULL || b == NULL || c == NULL) {
        free(a);
        free(b);
        free(c);
        return 1;
    }

    a->next = b;
    b->next = c;
    printf("acyclic: %s\n", has_cycle(a) ? "cycle" : "no cycle");

    c->next = b;
    printf("cyclic: %s\n", has_cycle(a) ? "cycle" : "no cycle");

    c->next = NULL;
    free(c);
    free(b);
    free(a);
    return 0;
}
