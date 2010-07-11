#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "dict.h"

float now() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

void
print_item(char *k, size_t sz, void *v, void *p) {

	(void)sz;
	(void)p;
	printf("k=[%s], v=[%s]\n", k, (char*)v);
}

int
main() {

	struct dict *d = dict_new(64);

	/* speed measure */
	float t0, t1;

	int i;
	int count = 2*1000*1000;
	int key_size = 20, val_size = 20;

	char *keys = malloc(key_size * count);
	char *vals = malloc(val_size * count);
#define KEY(i) (keys + key_size * i)
#define VAL(i) (vals + val_size * i)

	printf("[info] Each key has at least %zd bytes of overhead.\n", sizeof(struct bucket));

	printf("Setting up keys and values...\n");
	for(i = 0; i < count; ++i) {
		sprintf(KEY(i), "k-%d", i);
		sprintf(VAL(i), "v-%d", i);
	}

	printf("Adding...\n");
	t0 = now();
	for(i = 0; i < count; ++i) {
		dict_add(d, KEY(i), strlen(KEY(i)), VAL(i));
		/*
		if(i == 100) {
			dict_foreach(d, print_item, NULL);
		}
		*/
	}

	t1 = now();
	printf("Added %d elements in %0.2f sec: %0.2f/sec\n", count, (t1-t0)/1000.0, 1000*(float)count/(t1-t0));
	t0 = t1;

	printf("Reading back...\n");
	for(i = 0; i < count; ++i) {

		void * data = dict_get(d, KEY(i), strlen(KEY(i)));

		if(!data || data != VAL(i)) {
			printf("HT is corrupted.\n");
			return EXIT_FAILURE;
		}
	}

	t1 = now();
	printf("Retrieved %d elements in %0.2f sec: %0.2f/sec\n", count, (t1-t0)/1000.0, 1000*(float)count/(t1-t0));
	t0 = t1;

	free(keys);
	free(vals);

	dict_free(d);

	return EXIT_SUCCESS;
}
