#ifndef __LIST_H
#define __LIST_H

#include <stddef.h>

//Двухсвязный список
//Нужен при работе в классах, которые глобально объявлены

typedef struct list_node_s {
	struct list_node_s* prev;
	struct list_node_s* next;
} list_node_t;

typedef struct {
	list_node_s* first;
	list_node_s* last;
	ptrdiff_t offset; //Смещение к данным относительно node списка
} list_head_t;

void list_add(list_head_t* list,list_node_t* node,void* data);
void list_remove(list_head_t* list,list_node_t* node);

#define LIST_ZERO_INIT(list) list_head_t list = {NULL,NULL,0}
#define LIST_DATA(list,node) (void*)((char*)node+list.offset)
#define LIST_ITER_BEGIN(list)			\
	if(list.first)						\
	{									\
		list_node_t* item = list.first;	\
		do {
#define LIST_ITER_END()					\
		} while((item=item->next));		\
	}


#endif