#ifndef LINKEDLIST_H
#define LINKEDLIST_H


typedef struct{
    int data;
    struct snode* next;
}snode;

typedef void (*callback)(snode* data);
snode* create(int data,snode* next);
snode* prepend(snode* head,int data);
snode* append(snode* head, int data);
snode* insert_after(snode *head, int data, snode* prev);
snode* insert_before(snode *head, int data, snode* nxt);
void traverse(snode* head,callback f);
snode* remove_front(snode* head);
snode* remove_back(snode* head);
snode* remove_any(snode* head,snode* nd);
void display(snode* n);
snode* search(snode* head,int data);
void dispose(snode *head);
int count(snode *head);
snode* insertion_sort(snode* head);
snode* reverse(snode* head);
void menu();

#endif
