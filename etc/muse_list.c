#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "muse_list.h"

LISTNODE *AddNode(LIST *list, void *data, unsigned long sz)
{
	LISTNODE *node;

	if (!(list))
		return NULL;

	if (!(list->head)) {/*uninitialized list! set up head & tail nodes.*/
		list->head = malloc(sizeof(LISTNODE));
		if (!(list->head))
			return NULL;
		list->tail = list->head; /*initial degenerate case, head==tail*/
		node = list->tail;
	} else {
		list->tail->next = malloc(sizeof(LISTNODE));/*make room for the next one*/
		if (!(list->tail->next))
			return NULL;
		list->tail = list->tail->next; /*tail must always point to the last node*/
		node = list->tail; /*tail is always the last node by definition*/
	}
	memset(node, 0x00, sizeof(LISTNODE));

	node->data = malloc(sz);
	memset(node->data, 0x00, sz);
	node->sz = sz;
	memcpy(node->data, data, sz);

	return node;
}

void DeleteNode(LIST *list, LISTNODE *node)
{
	LISTNODE *del;

	if (!(list))
		return;

	if (node == list->head) {
		list->head = node->next;

		free(node->data);
		free(node);

		return;
	}

	del = list->head;
	while (del != NULL && del->next != node)
		del = del->next;

	if (!(del))
		return;

	if(del->next == list->tail)
		list->tail = del;

	del->next = del->next->next;

	free(node->data);
	free(node);
}

LISTNODE *FindNodeByRef(LIST *list, void *data)
{
	LISTNODE *node;

	if (!(list))
		return NULL;

	node = list->head;
	while ((node)) {
		if (node->data == data)
			return node;

		node = node->next;
	}

	return NULL;
}

LISTNODE *FindNodeByValue(LIST *list, void *data, unsigned long sz)
{
	LISTNODE *node;

	if (!(list))
		return NULL;

	node = list->head;
	while ((node)) {
		if (!memcmp(node->data, data, (sz > node->sz) ? node->sz : sz))
			return node;

		node = node->next;
	}

	return NULL;
}

/*frees all nodes in a list, including their data*/
void FreeNodes(LIST *list, int FreeParameterAsWell)
{
	LISTNODE *node, *previous;
	if(!(list))
		return;
	node = list->head;
	while(node){
		free(node->data);
		previous = node;
		node = node->next;
		free(previous);
	}
	if(FreeParameterAsWell) free(list);

	return;
}

void FreeList(LIST *list)
{
	FreeNodes(list, 0);
	return;
}
