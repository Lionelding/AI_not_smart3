#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "linkedlist.h"

snode* create(int data,snode* next)
{
    snode* new_snode = (snode*)malloc(sizeof(snode));
    if(new_snode == NULL)
    {
        printf("Error creating a new snode.\n");
        exit(0);
    }
    new_snode->data = data;
    new_snode->next = next;

    return new_snode;
}

/*
    add a new snode at the beginning of the list
*/
snode* prepend(snode* head,int data)
{
    snode* new_snode = create(data,head);
    head = new_snode;
    return head;
}

/*
    add a new snode at the end of the list
*/
snode* append(snode* head, int data)
{
    if(head == NULL)
        return NULL;
    /* go to the last snode */
    snode *cursor = head;
    while(cursor->next != NULL)
        cursor = cursor->next;

    /* create a new snode */
    snode* new_snode =  create(data,NULL);
    cursor->next = new_snode;

    return head;
}

/*
    insert a new snode after the prev snode
*/
snode* insert_after(snode *head, int data, snode* prev)
{
    if(head == NULL || prev == NULL)
        return NULL;
    /* find the prev snode, starting from the first snode*/
    snode *cursor = head;
    while(cursor != prev)
        cursor = cursor->next;

    if(cursor != NULL)
    {
        snode* new_snode = create(data,cursor->next);
        cursor->next = new_snode;
        return head;
    }
    else
    {
        return NULL;
    }
}

/*
    insert a new snode before the nxt snode
*/
snode* insert_before(snode *head, int data, snode* nxt)
{
    if(nxt == NULL || head == NULL)
        return NULL;

    if(head == nxt)
    {
        head = prepend(head,data);
        return head;
    }

    /* find the prev snode, starting from the first snode*/
    snode *cursor = head;
    while(cursor != NULL)
    {
        if(cursor->next == nxt)
            break;
        cursor = cursor->next;
    }

    if(cursor != NULL)
    {
        snode* new_snode = create(data,cursor->next);
        cursor->next = new_snode;
        return head;
    }
    else
    {
        return NULL;
    }
}

/*
    traverse the linked list
*/
void traverse(snode* head,callback f)
{
    snode* cursor = head;
    while(cursor != NULL)
    {
        f(cursor);
        cursor = cursor->next;
    }
}
/*
    remove snode from the front of list
*/
snode* remove_front(snode* head)
{
    if(head == NULL)
        return NULL;
    snode *front = head;
    head = head->next;
    front->next = NULL;
    /* is this the last snode in the list */
    if(front == head)
        head = NULL;
    free(front);
    return head;
}

/*
    remove snode from the back of the list
*/
snode* remove_back(snode* head)
{
    if(head == NULL)
        return NULL;

    snode *cursor = head;
    snode *back = NULL;
    while(cursor->next != NULL)
    {
        back = cursor;
        cursor = cursor->next;
    }

    if(back != NULL)
        back->next = NULL;

    /* if this is the last snode in the list*/
    if(cursor == head)
        head = NULL;

    free(cursor);

    return head;
}

/*
    remove a snode from the list
*/
snode* remove_any(snode* head,snode* nd)
{
    if(nd == NULL)
        return NULL;
    /* if the snode is the first snode */
    if(nd == head)
        return remove_front(head);

    /* if the snode is the last snode */
    if(nd->next == NULL)
        return remove_back(head);

    /* if the snode is in the middle */
    snode* cursor = head;
    while(cursor != NULL)
    {
        if(cursor->next == nd)
            break;
        cursor = cursor->next;
    }

    if(cursor != NULL)
    {
        snode* tmp = cursor->next;
        cursor->next = tmp->next;
        tmp->next = NULL;
        free(tmp);
    }
    return head;

}
/*
    display a snode
*/
void display(snode* n)
{
    if(n != NULL){
        printf("%d\n", n->data);
    }
    return;
}

/*
    Search for a specific snode with input data

    return the first matched snode that stores the input data,
    otherwise return NULL
*/
snode* search(snode* head,int data)
{

    snode *cursor = head;
    while(cursor!=NULL)
    {
        if(cursor->data == data)
            return cursor;
        cursor = cursor->next;
    }
    return NULL;
}

/*
    remove all element of the list
*/
void dispose(snode *head)
{
    snode *cursor, *tmp;

    if(head != NULL)
    {
        cursor = head->next;
        head->next = NULL;
        while(cursor != NULL)
        {
            tmp = cursor->next;
            free(cursor);
            cursor = tmp;
        }
    }
}
/*
    return the number of elements in the list
*/
int count(snode *head)
{
    snode *cursor = head;
    int c = 0;
    while(cursor != NULL)
    {
        c++;
        cursor = cursor->next;
    }
    return c;
}
/*
    sort the linked list using insertion sort
*/
//snode* insertion_sort(snode* head)
//{
//    snode *x, *y, *e;
//
//    x = head;
//    head = NULL;
//
//    while(x != NULL)
//    {
//        e = x;
//        x = x->next;
//        if (head != NULL)
//        {
//            if(e->data > head->data)
//            {
//                y = head;
//                while ((y->next != NULL) && (e->data> y->next->data))
//                {
//                    y = y->next;
//                }
//                e->next = y->next;
//                y->next = e;
//            }
//            else
//            {
//                e->next = head;
//                head = e ;
//            }
//        }
//        else
//        {
//            e->next = NULL;
//            head = e ;
//        }
//    }
//    return head;
//}

/*
    reverse the linked list
*/
snode* reverse(snode* head)
{
    snode* prev    = NULL;
    snode* current = head;
    snode* next;
    while (current != NULL)
    {
        next  = current->next;
        current->next = prev;
        prev = current;
        current = next;
    }
    head = prev;
    return head;
}
/*
    display the menu
*/
void menu()
{
    printf("--- C Linked List Demonstration --- \n\n");
    printf("0.menu\n");
    printf("1.prepend an element\n");
    printf("2.append an element\n");
    printf("3.search for an element\n");
    printf("4.insert after an element\n");
    printf("5.insert before an element\n");
    printf("6.remove front snode\n");
    printf("7.remove back snode\n");
    printf("8.remove any snode\n");
    printf("9.sort the list\n");
    printf("10.Reverse the linked list\n");
    printf("-1.quit\n");

}

