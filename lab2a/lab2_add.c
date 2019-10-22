#include <stdio.h>
#include <getopt.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>

long long counter = 0;
int yield_flag = 0;
pthread_mutex_t lock;
int spinlock = 0;
int num_threads = 1;
int num_iterations = 1;
char sync_opt = 'u';

void add(long long *pointer, long long value) {
	long long sum = *pointer + value;
	if (yield_flag)
		sched_yield();
	*pointer = sum;
}

void* add_body() {
	int i, l;
	for (i = 0; i < num_iterations; i++) {
		switch (sync_opt) {
		case 'u':
			add(&counter, 1);
			break;
		case 'm':
			pthread_mutex_lock(&lock);
			add(&counter, 1);
			pthread_mutex_unlock(&lock);
			break;
		case 's':
			while (__sync_lock_test_and_set(&spinlock, 1))
				continue;
			add(&counter, 1);
			__sync_lock_release(&spinlock);
			break;
		case 'c': {
			long long old, new;
			do {
				old = counter;
				new = old + 1;
				if (yield_flag)
					sched_yield();
			} while (__sync_val_compare_and_swap(&counter, old, new) != old);
			break;
		}
		default:
			break;
		}
	}

	for (l = 0; l < num_iterations; l++) {
		switch (sync_opt) {
		case 'u':
			add(&counter, -1);
			break;
		case 'm':
			pthread_mutex_lock(&lock);
			add(&counter, -1);
			pthread_mutex_unlock(&lock);
			break;
		case 's':
			while (__sync_lock_test_and_set(&spinlock, 1))
				continue;
			add(&counter, -1);
			__sync_lock_release(&spinlock);
			break;
		case 'c': {
			long long old, new;
			do {
				old = counter;
				if (yield_flag)
					sched_yield();
				new = old - 1;
			} while (__sync_val_compare_and_swap(&counter, old, new) != old);
			break;
		}
		default:
			break;
		}
	}
	return NULL;
}

int main(int argc, char* argv[]) {
	int j, k;

	struct timespec start_time, end_time;

	struct option options[] = { { "threads", required_argument, NULL, 't' }, {
			"iterations", required_argument, NULL, 'i' }, { "sync",
	required_argument, NULL, 's' }, { "yield", no_argument, NULL, 'y' }, { 0, 0,
			0, 0 } };
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
			yield_flag = 1;
			break;
		case 's':
			sync_opt = optarg[0];
			if ((sync_opt != 'm') && (sync_opt != 's') && (sync_opt != 'c')) {
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

	pthread_t *thread_ids = (pthread_t*) malloc(sizeof(pthread_t) * num_threads);

	if (clock_gettime(CLOCK_MONOTONIC, &start_time)) {
		fprintf(stderr, "gettime function error\n");
		exit(1);
	}

	for (j = 0; j < num_threads; j++) {
		if (pthread_create(&thread_ids[j], NULL, add_body, NULL)) {
			fprintf(stderr, "pthread create error\n");
			exit(1);
		}
	}

	for (k = 0; k < num_threads; k++) {
		if (pthread_join(thread_ids[k], NULL)) {
			fprintf(stderr,"pthread join error\n");
			exit(1);
		}
	}

	if (clock_gettime(CLOCK_MONOTONIC, &end_time)) {
		fprintf(stderr, "gettime function error\n");
		exit(1);
	}

	if (yield_flag) {
		if (sync_opt == 'u')
			printf("add-yield-none,");
		else
			printf("add-yield-%c,", sync_opt);
	} else {
		if (sync_opt == 'u')
			printf("add-none,");
		else
			printf("add-%c,", sync_opt);
	}

	long long num_operations = num_threads * num_iterations * 2;
	long long run_time = (end_time.tv_sec - start_time.tv_sec) * 1000000000
			+ end_time.tv_nsec - start_time.tv_nsec;
	long long average_run_time = run_time / num_operations;

	printf("%d,%d,%lld,%lld,%lld,%lld\n", num_threads, num_iterations,
			num_operations, run_time, average_run_time, counter);

	free(thread_ids);

	exit(0);
}
