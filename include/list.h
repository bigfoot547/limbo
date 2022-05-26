#ifndef LIMBO_LIST_H_INCLUDED
#define LIMBO_LIST_H_INCLUDED

#include <stddef.h>
#include <pthread.h>
#include <stdbool.h>

struct list_dlnode {
    void *ptr;
    struct list_dlnode *prev;
    struct list_dlnode *next;
};

typedef struct tag_dllist {
    size_t length;
    struct list_dlnode *head;
    struct list_dlnode *tail;

    pthread_mutex_t *mutex;
} dllist_t;

dllist_t *dll_create();
dllist_t *dll_create_sync();

#define dll_lock(_list) if ((_list)->mutex) pthread_mutex_lock((_list)->mutex)
#define dll_unlock(_list) if ((_list)->mutex) pthread_mutex_unlock((_list)->mutex)

struct list_dlnode *dll_addend(dllist_t *list, void *element);
struct list_dlnode *dll_addstart(dllist_t *list, void *element);
struct list_dlnode *dll_addafter(dllist_t *list, void *element, struct list_dlnode *node);
struct list_dlnode *dll_addbefore(dllist_t *list, void *element, struct list_dlnode *node);

void *dll_popstart(dllist_t *list);
void *dll_popend(dllist_t *list);

void *dll_removenode(dllist_t *list, struct list_dlnode *node);

void dll_free(dllist_t *list);

#define DLLIST_FOREACH(_list, _curnode) \
do { dll_lock(_list); for (struct list_dlnode *_curnode = (_list)->head, *_next = (_list)->head ? (_list)->head->next : NULL; _curnode; _curnode = _next, _next = _curnode ? _curnode->next : NULL)

#define DLLIST_FOREACH_REV(_list, _curnode) \
do { dll_lock(_list); for (struct list_dlnode *_curnode = (_list)->tail, *_prev = (_list)->tail ? (_list)->tail->prev : NULL; _curnode; _curnode = _prev, _prev = _curnode ? _curnode->prev : NULL)

#define DLLIST_FOREACH_DONE(_list) \
dll_unlock(_list); } while (0)

#endif // include guard
