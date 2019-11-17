#include "list.h"
#include <string.h>

void list_add(list_head_t* list,list_node_t* node,void* data)
{
	if(!list->offset)
		list->offset = (ptrdiff_t)data - (ptrdiff_t)node;
	
	if(!list->first) list->first = node;
	if(list->last) list->last->next = node;
	node->prev = list->last;
	list->last = node;
}

void list_remove(list_head_t* list,list_node_t* node)
{
	if(node->next) node->next->prev = node->prev;
	if(node->prev) node->prev->next = node->next;
	
	if(list->first == node) list->first = node->next;
	if(list->last == node) list->last = node->prev;
}
