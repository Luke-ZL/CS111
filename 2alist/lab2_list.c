#include <stdio.h>
#include <getopt.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "SortedList.h"

int num_threads = 1;
int num_iterations = 1;
int num_elements;
int opt_yield = 0;
char sync_opt = 'u';
pthread_mutex_t lock;
int spinlock = 0;

SortedList_t* list;
SortedListElement_t* element_array;

void* list_body(void* index) {
	int start = *(int*) index;
	int end = start + num_iterations;

	//insert
	for (int i = start; i < end; i++) {
		switch (sync_opt) {
		case 'u':
			SortedList_insert(list, &element_array[i]);
			break;
		case 'm':
			pthread_mutex_lock(&lock);
			SortedList_insert(list, &element_array[i]);
			pthread_mutex_unlock(&lock);
			break;
		case 's':
			while (__sync_lock_test_and_set(&spinlock, 1))
				continue;
			SortedList_insert(list, &element_array[i]);
			__sync_lock_release(&spinlock);
			break;
		default:
			break;
		}
	}

	//length
	switch (sync_opt) {
	case 'u':
		if (SortedList_length(list) == -1) {
			fprintf(stderr, "Corrupted list\n");
			exit(2);
		}
		break;
	case 'm':
		pthread_mutex_lock(&lock);
		if (SortedList_length(list) == -1) {
			fprintf(stderr, "Corrupted list\n");
			exit(2);
		}
		pthread_mutex_unlock(&lock);
		break;
	case 's':
		while (__sync_lock_test_and_set(&spinlock, 1))
			continue;
		if (SortedList_length(list) == -1) {
			fprintf(stderr, "Corrupted list\n");
			exit(2);
		}
		__sync_lock_release(&spinlock);
		break;
	default:
		break;
	}

	//delete
	for (int i = start; i < end; i++) {
		switch (sync_opt) {
		case 'u':
			if ((SortedList_lookup(list, element_array[i].key) == NULL)
					|| (SortedList_delete(&element_array[i]) == 1)) {
				fprintf(stderr, "Corrupted list\n");
				exit(2);
			}
			break;
		case 'm':
			pthread_mutex_lock(&lock);
			if ((SortedList_lookup(list, element_array[i].key) == NULL)
					|| (SortedList_delete(&element_array[i]) == 1)) {
				fprintf(stderr, "Corrupted list\n");
				exit(2);
			}
			pthread_mutex_unlock(&lock);
			break;
		case 's':
			while (__sync_lock_test_and_set(&spinlock, 1))
				continue;
			if ((SortedList_lookup(list, element_array[i].key) == NULL)
					|| (SortedList_delete(&element_array[i]) == 1)) {
				fprintf(stderr, "Corrupted list\n");
				exit(2);
			}
			__sync_lock_release(&spinlock);
			break;
		default:
			break;
		}
	}

	return NULL;
}

int main(int argc, char* argv[]) {

	struct timespec start_time, end_time;

	struct option options[] = { { "threads", required_argument, NULL, 't' }, {
			"iterations", required_argument, NULL, 'i' }, { "sync",
	required_argument, NULL, 's' }, { "yield", required_argument, NULL, 'y' }, {
			0, 0, 0, 0 } };

	char c;

	while ((c = getopt_long(argc, argv, "", options, NULL)) != -1) {
		switch (c) {
		case 't':
			num_threads = atoi(optarg);
			break;
		case 'i':
			num_iterations = atoi(optarg);
			break;
		case 'y':
			for (int i = 0; i < strlen(optarg); i++) {
				switch (optarg[i]) {
				case 'i':
					opt_yield |= INSERT_YIELD;
					break;
				case 'd':
					opt_yield |= DELETE_YIELD;
					break;
				case 'l':
					opt_yield |= LOOKUP_YIELD;
					break;
				default:
					fprintf(stderr, "Invalid argument for --yield\n");
					exit(1);
				}
			}
			break;
		case 's':
			sync_opt = optarg[0];
			if ((sync_opt != 'm') && (sync_opt != 's')) {
				fprintf(stderr, "Invalid argument for --sync\n");
				exit(1);
			} else if (sync_opt == 'm') {
				if (pthread_mutex_init(&lock, NULL)) {
					fprintf(stderr, "error initializing pthread_mutex.\n");
					exit(1);
				}
			}
			break;
		default:
			fprintf(stderr, "invalid option\n");
			exit(1);
			break;
		}
	}

	//initialize list
	list = malloc(sizeof(SortedList_t));
	list->next = list;
	list->prev = list;
	list->key = NULL;

	num_elements = num_threads * num_iterations;
	element_array = malloc(num_elements * sizeof(SortedListElement_t));

	//fill the array with random elements
	srand(time(NULL));
	for (int i = 0; i < num_elements; i++) {
		char char_1 = 'Z' - (rand() % 26);
		char char_2 = 'Z' - (rand() % 26);
		char* key = malloc(3 * sizeof(char));
		key[0] = char_1;
		key[1] = char_2;
		key[2] = '\0';
		element_array[i].key = key;
	}

	pthread_t *thread_ids = (pthread_t*) malloc(
			sizeof(pthread_t) * num_threads);
	int *thread_element_index = (int *) malloc(sizeof(int) * num_threads);

	for (int i = 0; i < num_threads; i++) {
		thread_element_index[i] = i * num_iterations;
	}

	if (clock_gettime(CLOCK_MONOTONIC, &start_time)) {
		fprintf(stderr, "gettime function error\n");
		exit(1);
	}

	for (int i = 0; i < num_threads; i++) {
		if (pthread_create(&thread_ids[i], NULL, list_body,
				&thread_element_index[i])) {
			fprintf(stderr, "pthread create error\n");
			exit(1);
		}
	}

	for (int i = 0; i < num_threads; i++) {
		if (pthread_join(thread_ids[i], NULL)) {
			fprintf(stderr, "pthread join error\n");
			exit(1);
		}
	}

	if (clock_gettime(CLOCK_MONOTONIC, &end_time)) {
		fprintf(stderr, "gettime function error\n");
		exit(1);
	}

	free(thread_element_index);
	free(thread_ids);
	free(element_array);

	//print options
	printf("list-");

	if (opt_yield == 0)
		printf("none-");
	else {
		if (opt_yield & INSERT_YIELD)
			printf("i");
		if (opt_yield & DELETE_YIELD)
			printf("d");
		if (opt_yield & LOOKUP_YIELD)
			printf("l");
		printf("-");
	}

	switch (sync_opt) {
	case 'u':
		printf("none,");
		break;
	default:
		printf("%c,", sync_opt);
		break;
	}

	long long num_operations = num_elements * 3;
	long long run_time = (end_time.tv_sec - start_time.tv_sec) * 1000000000
			+ end_time.tv_nsec - start_time.tv_nsec;
	long long average_run_time = run_time / num_operations;

	printf("%d,%d,1,%lld,%lld,%lld\n", num_threads, num_iterations,
			num_operations, run_time, average_run_time);

	if (SortedList_length(list) != 0) {
		if (SortedList_length(list) < 0)
			fprintf(stderr, "corrupted list\n");
		else
			fprintf(stderr, "list is not empty\n");
		exit(2);
	} else
		exit(0);
}
