/*
 * css_linkedlist.c
 *
 *  Created on: 2014 Author: ss
 */

#include <stdio.h>
#include <stdlib.h>
#include "css_linkedlist.h"

static int css_list_get_entity(css_linked_list *list, int index,
		css_linked_list_entity **itr);
static int css_list_remove_entity(css_linked_list *list,
		css_linked_list_entity *itr);
static int css_list_is_head(css_linked_list *list, css_linked_list_entity *itr);

static int css_list_is_tail(css_linked_list *list, css_linked_list_entity *itr);

int css_list_init(css_linked_list *list)
{
	CHECK_CSS_LIST(list);
	list->size = 0;
	list->head = list->tail = NULL;
	return 0;
}

int css_list_push(css_linked_list *list, void* e)
{
	css_linked_list_entity *entity;
	CHECK_CSS_LIST(list);
	entity = (css_linked_list_entity*) malloc(sizeof(css_linked_list_entity));
	entity->data = e;
	if (css_list_is_empty(list)) {
		entity->next = entity->pre = NULL;
		list->head = list->tail = entity;
		list->size = 1;
	} else {
		list->tail->next = entity;
		entity->pre = list->tail;
		entity->next = NULL;
		list->tail = entity;
		list->size += 1;
	}
	return 0;
}

int css_list_get_entity(css_linked_list *list, int index,
		css_linked_list_entity **itr)
{
	int32_t i = 0;
	CHECK_CSS_LIST(list);
	CHECK_CSS_LIST_INDEX(list, index);
	*itr = list->head;

	while (i < index) {
		*itr = ((css_linked_list_entity *) *itr)->next;
		i++;
	}
	return 0;
}

int css_list_get(css_linked_list *list, int index, void** e)
{
	css_linked_list_entity *itr;
	int reply = css_list_get_entity(list, index, &itr);
	if (reply == 0) {
		*e = itr->data;
	}
	return reply;
}

int css_list_pop(css_linked_list *list, void **e)
{
	css_linked_list_entity *itr;
	CHECK_CSS_LIST_NOT_EMPTY(list);
	itr = list->head;
	list->head = itr->next;
	if (css_list_is_tail(list, itr)) {
		list->head = list->tail = NULL;
	} else {
		itr->next->pre = NULL;
	}
	list->size -= 1;
	*e = itr->data;
	free(itr);
	return 0;
}

int css_list_is_empty(css_linked_list *list)
{
	CHECK_CSS_LIST(list);
	return css_list_size(list) == 0 ? 1 : 0;
}

int css_list_is_head(css_linked_list *list, css_linked_list_entity *itr)
{
	return list->head == itr;
}

int css_list_is_tail(css_linked_list *list, css_linked_list_entity *itr)
{
	return list->tail == itr;
}

int css_list_remove_entity(css_linked_list *list, css_linked_list_entity *itr)
{
	list->size -= 1;
	if (css_list_is_head(list, itr) && css_list_is_tail(list, itr)) {
		list->head = list->tail = NULL;
		free(itr);
	} else if (css_list_is_head(list, itr)) {
		list->head = itr->next;
		itr->next->pre = NULL;
		free(itr);
	} else if (css_list_is_tail(list, itr)) {
		list->tail = itr->pre;
		itr->pre->next = NULL;
		free(itr);
	} else {
		itr->pre->next = itr->next;
		itr->next->pre = itr->pre;
		free(itr);
	}
	return 0;
}

int css_list_remove(css_linked_list *list, void *e)
{
	css_linked_list_entity *itr;
	CHECK_CSS_LIST_NOT_EMPTY(list);
	itr = list->head;
	while (itr != NULL) {
		if (itr->data == e) {
			return css_list_remove_entity(list, itr);
		}
		itr = itr->next;
	}
	return -1;
}

int css_list_remove_at(css_linked_list *list, int index)
{
	css_linked_list_entity *itr;
	int reply = css_list_get_entity(list, index, &itr);
	if (reply == 0) {
		return css_list_remove_entity(list, itr);
	}
	return reply;

}
int css_list_insert_at(css_linked_list *list, int index, void *e)
{
	css_linked_list_entity *itr;
	css_linked_list_entity *entity;
	int reply = css_list_get_entity(list, index, &itr);
	if (reply == 0) {
		entity = (css_linked_list_entity*) malloc(
				sizeof(css_linked_list_entity));
		entity->data = e;
		itr->pre->next = entity;
		entity->pre = itr->pre;
		itr->pre = entity;
		entity->next = itr;
		list->size += 1;
	}
	return reply;

}
int css_list_size(css_linked_list *list)
{
	CHECK_CSS_LIST(list);
	return list->size;
}
int css_list_foreach(css_linked_list *list, foreach_cb cb)
{
	css_linked_list_entity *itr, *next;
	CHECK_CSS_LIST(list);
	itr = list->head;
	while (itr != NULL) {
		next = itr->next;
		cb(itr->data);
		itr = next;
	}
	return 0;
}
/**
 * warnning: do not free all date of entity.
 */
int css_list_destroy(css_linked_list *list)
{
	css_linked_list_entity *itr, *next;
	CHECK_CSS_LIST(list);
	itr = list->head;
	while (itr != NULL) {
		next = itr->next;
		free(itr);			// do not free itr.data
		itr = next;
	}
	list->size = 0;
	free(list);
	return 0;
}

#ifdef CSS_TEST
#include <assert.h>
static int count = 0;
css_linked_list *l;
void test_css_linked_list_foreach_fun(void *t) {
	printf("%s ", t);
	count += 1;
}

void test_css_linked_list_foreach_remove(void *t) {
	assert(0 == css_list_remove(l,t));
	free(t);
}

void test_css_linked_list_suite() {

	char *s2 = "test2";
	char *s1 = "test";
	char *s3 = "nothing";
	char *s4 = (char*)malloc(sizeof(1));
	char *s5 = (char*)malloc(sizeof(1));
	char *s6 = (char*)malloc(sizeof(1));
	int i;
	char *t = NULL;
	l = (css_linked_list*)malloc(sizeof(css_linked_list));
	printf("start test_css_linked_list...\n");
	css_list_init(l);
	assert(0 == css_list_size(l));
	assert(1 == css_list_is_empty(l));
	assert(-1 == css_list_push(NULL,s1));
	assert(0 == css_list_push(l, s1));

	assert(-1 == css_list_get(NULL,0,(void**)&t));
	assert(-1 == css_list_get(l, 1, (void** )&t));

	assert(1 == css_list_size(l));
	assert(0 == css_list_get(l, 0, (void** )&t));
	assert(t == s1);

	assert(0 == css_list_push(l, s2));
	assert(2 == css_list_size(l));
	assert(0 == css_list_get(l, 1, (void** )&t));
	assert(t == s2);

	for (i = 0; i < 100; i++) {
		assert(0 == css_list_push(l, s3));
	}
	assert(102 == css_list_size(l));
	assert(0 == css_list_get(l, 101, (void** )&t));
	assert(t == s3);

	for (i = 0; i < 100; i++) {
		assert(0 == css_list_remove_at(l, 2));
	}
//	printf("size:%d\n",css_list_size(l));
	assert(2 == css_list_size(l));
	assert(0 == css_list_insert_at(l, 1, s3));
	assert(0 == css_list_get(l, 1, (void** )&t));
	assert(t == s3);
	assert(0 == css_list_get(l, 2, (void** )&t));
	assert(t == s2);
	assert(3 == css_list_size(l));
//
	printf("dump list:");
	assert(0 == css_list_foreach(l, test_css_linked_list_foreach_fun));
	printf("\n");
	assert(3 == count);

//
	assert(0 == css_list_pop(l, (void** )&t));
	assert(s1 == t);
	assert(2 == css_list_size(l));
	assert(0 == css_list_pop(l, (void** )&t));
	assert(s3 == t);
	assert(0 == css_list_pop(l, (void** )&t));
	assert(s2 == t);
	assert(0 == css_list_size(l));
	assert(NULL == l->head);
	assert(NULL == l->tail);
//
	assert(0 == css_list_push(l, s1));
	assert(0 == css_list_push(l, s2));
	assert(0 == css_list_push(l, s3));
	assert(3 == css_list_size(l));

	assert(0 == css_list_remove(l, s1));
	assert(0 == css_list_remove(l, s2));
	assert(0 == css_list_remove(l, s3));
	assert(0 == css_list_size(l));

	assert(0 == css_list_push(l, s1));
	assert(0 == css_list_push(l, s2));
	assert(0 == css_list_push(l, s3));
	assert(3 == css_list_size(l));
	assert(0 == css_list_remove(l, s2));
	assert(0 == css_list_remove(l, s3));
	assert(0 == css_list_remove(l, s1));
	assert(0 == css_list_size(l));
// test pop and push
	assert(0 == css_list_push(l, s1));
	assert(0 == css_list_push(l, s2));
	assert(0 == css_list_push(l, s3));
	assert(3 == css_list_size(l));
	assert(0 == css_list_pop(l,(void **)&t));
	assert(0 == css_list_push(l, s1));
	assert(0 == css_list_pop(l,(void **)&t));
	assert(0 == css_list_push(l, s2));
	assert(0 == css_list_pop(l,(void **)&t));
	assert(0 == css_list_push(l, s3));
	assert(0 == css_list_remove(l, s2));
	assert(0 == css_list_remove(l, s3));
	assert(0 == css_list_remove(l, s1));
	assert(0 == css_list_size(l));
// test foreach remove
	assert(0 == css_list_push(l, s4));
	assert(0 == css_list_push(l, s5));
	assert(0 == css_list_push(l, s6));

	assert(3 == css_list_size(l));
	assert(0 == css_list_foreach(l, test_css_linked_list_foreach_remove));
	assert(0 == css_list_size(l));
	css_list_destroy(l);
	printf("test_css_linked_list successful.\n");
}

#endif

