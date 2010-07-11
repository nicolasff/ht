#ifndef DICT_H
#define DICT_H

#include <stdlib.h>

struct bucket {
	
	char *k;
	void *v;

	size_t sz;

	struct bucket *collision_prev; /* same hash! */
	struct bucket *collision_next;

	struct bucket *next;
	struct bucket *prev;
} __attribute__((__packed__));

struct ht {
	size_t sz;
	struct bucket* slots;
	struct bucket *first;
};

struct dict {
	int count;

	struct ht *ht;
	struct ht *ht_old;
};


struct dict *
dict_new(int sz);

void
dict_free(struct dict *d);

void
dict_resize(struct dict *d, size_t sz);

void
dict_add(struct dict *d, char *k, size_t sz, void *v);

void*
dict_get(struct dict *d, char *k, size_t sz);

#endif /* DICT_H */
