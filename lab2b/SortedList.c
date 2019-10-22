#include "SortedList.h"

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

void SortedList_insert(SortedList_t *list, SortedListElement_t *element)
{
	if((list == NULL) || (element == NULL)) return;
	SortedListElement_t *current = list->next;
	while (current->key != NULL)
	{
		if (strcmp(current->key, element->key) > 0) break;
		else current = current->next;
	}

	//yield
	if (opt_yield & INSERT_YIELD) sched_yield();

	element->next = current;
	element->prev = current->prev;
	current->prev->next = element;
	current->prev = element;
}

int SortedList_delete( SortedListElement_t *element)
{
	if ((element == NULL)||(element->prev->next != element) || (element->next->prev != element)) return 1;

	//yield
	if (opt_yield & DELETE_YIELD) sched_yield();

	element->prev->next = element->next;
	element->next->prev = element->prev;
	return 0;
}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key)
{
	if((list == NULL) || (key == NULL)) return NULL;
	SortedListElement_t *current = list->next;

	while (current != list)
	{
		if (strcmp(current->key, key) == 0) return current;
		else if (strcmp(current->key, key) > 0) return NULL;
		else {
			if (opt_yield & LOOKUP_YIELD) sched_yield();
			current = current -> next;
		}
	}
	return NULL;
}

int SortedList_length(SortedList_t *list)
{
	if(list == NULL) return -1;
	int counter = 0;
	SortedListElement_t *current = list->next;

	while (current != list)
	{
		if (current->prev->next != current || current->next->prev != current) return -1;
		counter++;
		if (opt_yield & LOOKUP_YIELD) sched_yield();
		current = current -> next;
	}
	return counter;
}


