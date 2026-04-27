#include<stdio.h>
#include<stdlib.h>

typedef struct Node{
    int value;
    struct Node* next;
}Node;

Node* build_list(int *arr, int n){
    if (sizeof(arr)==0)
        return NULL;

    Node* tmp = NULL;
    Node* curr = NULL;

    for (int i=0;i<n;i++){
        Node* node = malloc(sizeof(Node));

        node->value = arr[i];
        node->next = NULL;

        if (tmp == NULL){
            tmp = node;
            curr = node;
        } else {
            curr->next = node;
            curr= node;
        }
    }
    return tmp;
}

Node* reverse_list(Node* head){
    Node *prev = head;
    Node *curr = NULL;
    
    while (prev != NULL){
        Node * tmp = prev->next;
        prev->next = curr;
        curr = prev;
        prev = tmp;
    }
    return curr;
}

void print_list(Node* head){
    while(head!=NULL){
        printf("%d ",head->value);
        head = head->next;
    }
    printf("\n");
}

int main(){
    int arr[] ={10,8,17,13,19};
    int n = sizeof(arr)/sizeof(arr[0]);
    Node* head = build_list(arr,n);

    print_list(head);

    Node *rev= reverse_list(head);
    print_list(rev);
    return 0;
}