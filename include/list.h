#ifndef LIMBO_LIST_H_INCLUDED
#define LIMBO_LIST_H_INCLUDED

#include <stddef.h>

struct list_dlnode {
    void *ptr;
    struct list_dlnode *prev;
    struct list_dlnode *next;
};

typedef struct tag_dllist {
    size_t length;
    struct list_dlnode *head;
    struct list_dlnode *tail;
} dllist_t;

dllist_t *dll_create();
struct list_dlnode *dll_addend(dllist_t *list, void *element);
struct list_dlnode *dll_addstart(dllist_t *list, void *element);
struct list_dlnode *dll_addafter(dllist_t *list, void *element, struct list_dlnode *node);
struct list_dlnode *dll_addbefore(dllist_t *list, void *element, struct list_dlnode *node);

void *dll_popstart(dllist_t *list);
void *dll_popend(dllist_t *list);

void *dll_removenode(dllist_t *list, struct list_dlnode *node);

void dll_free(dllist_t *list);

#define DLLIST_FOREACH(_list, _curnode) \
for (struct list_dlnode *_curnode = _list->head; _curnode; _curnode = _curnode->next)

#define DLLIST_FOREACH_REV(_list, _curnode) \
for (struct list_dlnode *_curnode = _list->tail; _curnode; _curnode = _curnode->prev)

#endif // include guard
