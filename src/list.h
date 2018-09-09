#ifndef LIST_H
#define LIST_H


struct list_node {
    void *data;
    struct list_node *next;
};


typedef struct list_node ListNode;


typedef struct {
    ListNode *head;
    ListNode *tail;
    unsigned long len;
} List;


/* Compare function, accept two void * arguments, generally referring a node
   and his subsequent */
typedef int (*compare_func)(void *, void *);

/* Create an empty list */
List *list_init(void);

/* Release a list, accept a integer flag to control the depth of the free call
 * (e.g. going to free also data field of every node) */
void list_free(List *, int);

/* Attach a list to another one on tail */
List *list_attach(List *, ListNode *, unsigned long);

/* Insert data into a node and push it to the front of the list */
List *list_push(List *, void *);

/* Insert data into a node and push it to the back of the list */
List *list_push_back(List *, void *);

/* Remove a node from the list based on a compare function that must be
   previously defined and passed in as a function pointer, accept two void *
   args, which generally means a node and his subsequent */
ListNode *list_remove(ListNode *, ListNode *, compare_func);


#endif
