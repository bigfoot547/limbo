#include "list.h"

#include <stdlib.h>
#include <string.h>

dllist_t *dll_create() {
    dllist_t *list = malloc(sizeof(dllist_t));
    memset(list, 0, sizeof(dllist_t));
    return list;
}

struct list_dlnode *list_dlnode_new(void *element) {
    struct list_dlnode *node = malloc(sizeof(struct list_dlnode));
    memset(node, 0, sizeof(struct list_dlnode));
    node->ptr = element;
    return node;
}

struct list_dlnode *dll_addend(dllist_t *list, void *element) {
    struct list_dlnode *node = list_dlnode_new(element);
    if (!list->tail) {
        list->head = list->tail = node;
        list->length = 1;
        return node;
    }

    list->tail->next = node;
    node->prev = list->tail;
    list->tail = node;
    ++list->length;
    return node;
}

struct list_dlnode *dll_addstart(dllist_t *list, void *element) {
    struct list_dlnode *node = list_dlnode_new(element);
    if (!list->head) {
        list->head = list->tail = node;
        list->length = 1;
        return node;
    }

    list->head->prev = node;
    node->next = list->head;
    list->head = node;
    ++list->length;
    return node;
}

struct list_dlnode *dll_addafter(dllist_t *list, void *element, struct list_dlnode *node) {
    struct list_dlnode *newnode = list_dlnode_new(element);
    struct list_dlnode *next = node->next;
    node->next = newnode;
    newnode->prev = node;

    if (next) {
        next->prev = newnode;
        newnode->next = next;
    } else {
        list->tail = newnode;
    }

    ++list->length;
    return newnode;
}

struct list_dlnode *dll_addbefore(dllist_t *list, void *element, struct list_dlnode *node) {
    struct list_dlnode *newnode = list_dlnode_new(element);
    struct list_dlnode *prev = node->prev;
    node->prev = newnode;
    newnode->next = node;

    if (prev) { // this is the new head of the tree
        prev->next = newnode;
        newnode->prev = prev;
    } else {
        list->head = newnode;
    }

    ++list->length;
    return newnode;
}

void *dll_popstart(dllist_t *list) {
    if (!list->head) return NULL;
    struct list_dlnode *head = list->head;
    list->head = head->next;
    void *elem = head->ptr;

    if (list->head) list->head->prev = NULL;
    free(head);

    --list->length;
    return elem;
}

void *dll_popend(dllist_t *list) {
    if (!list->tail) return NULL;
    struct list_dlnode *tail = list->tail;
    list->tail = tail->prev;
    void *elem = tail->ptr;

    if (list->tail) list->tail->next = NULL;
    free(tail);

    --list->length;
    return elem;
}

void *dll_removenode(dllist_t *list, struct list_dlnode *node) {
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        list->head = node->next;
    }

    if (node->next) {
        node->next->prev = node->prev;
    } else {
        list->tail = node->prev;
    }

    void *elem = node->ptr;
    free(node);
    --list->length;
    return elem;
}

void dll_free(dllist_t *list) {
    if (!list) return;

    struct list_dlnode *cur = list->head, *del = cur;
    while (cur) {
        del = cur;
        cur = cur->next;
        free(del);
    }
    free(list);
}
