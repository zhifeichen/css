/*
 * css_linkedlist.h
 *
 *  Created on: 2014-7-18
 *      Author: ss
 */

#ifndef CSS_LINKEDLIST_H_
#define CSS_LINKEDLIST_H_

#include <stdint.h>
#include <stddef.h>

#define CHECK_CSS_LIST(l) if((l) == NULL) return -1;
#define CHECK_CSS_LIST_NOT_EMPTY(l) if((l) == NULL || (l)->size <= 0) return -1;
#define CHECK_CSS_LIST_INDEX(l,index) if((index) < 0 || (l)->size-1 < (index)) return -1;

typedef struct css_linked_list_entity_a{
	void *data;
	struct css_linked_list_entity_a *next;
	struct css_linked_list_entity_a *pre;
}css_linked_list_entity;

typedef struct {
	css_linked_list_entity *head;
	css_linked_list_entity *tail;
	int32_t size;
}css_linked_list;

typedef void (*foreach_cb)(void* e);
/**
 * init linked list when first used.
 * @param list
 */
int css_list_init(css_linked_list *list);
int css_list_destroy(css_linked_list *list);
/**
 *
 */
int css_list_push(css_linked_list *list,void *e);
int css_list_pop(css_linked_list *list,void **e);
int css_list_get(css_linked_list *list,int index,void **e);
int css_list_is_empty(css_linked_list *list);
int css_list_remove(css_linked_list *list,void *e);
int css_list_remove_at(css_linked_list *list,int index);
int css_list_insert_at(css_linked_list *list,int index,void *e);
int css_list_size(css_linked_list *list);
int css_list_foreach(css_linked_list *list,foreach_cb cb);


void test_css_linked_list();
#endif /* CSS_LINKEDLIST_H_ */
