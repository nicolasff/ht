#include "dict.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DICT_MAX_LOAD	0.75
#define DICT_REHASH_BATCH_SIZE	20

/* Hash function by Daniel J. Bernstein */
static unsigned long
djb_hash(char *str, size_t sz) {

	unsigned long hash = 5381;
	char *p;

	for(p = str; p < str+sz; ++p) {
		hash = ((hash << 5) + hash) + (unsigned char)(*p); /* hash * 33 + c */
	}

	return hash;
}

/* create a new HT */
static struct ht *
ht_new(int sz) {
	struct ht *ht = calloc(sizeof(struct ht), 1);
	ht->sz = sz;

	ht->slots = calloc(sizeof(struct bucket), sz);
	if(ht->slots == NULL) {
		fprintf(stderr, "failed to allocate %zd bytes.\n",
				sz * sizeof(struct bucket));
		abort();
	}

	return ht;
}

static void
bucket_free(struct bucket *b) {
	if(b->collision_prev) {
		b->collision_prev->collision_next = b->collision_next;
		if(b->collision_next) {
			b->collision_next->collision_prev = b->collision_prev;
		}

		free(b);
	}
}


/* delete a HT */
static void
ht_free(struct ht *ht) {
	struct bucket *b, *tmp;

	/* delete all items from the HT */
	for(b = ht->first; b; ) {
		tmp = b->next;
		bucket_free(b);
		b = tmp;
	}

	/* delete the container */
	free(ht->slots);
	free(ht);
}


/* inset into a HT */
static struct bucket *
ht_insert(struct ht *ht, char *k, size_t sz, void *v) {

	unsigned long h = djb_hash(k, sz);
	struct bucket *b, *head, *tmp;

	tmp = head = b = &ht->slots[h % ht->sz];

	/* check for re-assignment */
	while(tmp && tmp->k) {
		if(tmp->sz == sz && memcmp(k, tmp->k, sz) == 0) {
			tmp->v = v; /* hit! replace */
			return NULL;
		}
		tmp = tmp->collision_next;
	}

	/* found nothing, add to head. */
	if(head->k != NULL) { /* add to existing list */
		b = calloc(sizeof(struct bucket), 1);
		b->collision_next = head->collision_next;
		head->collision_next = b;
		b->collision_prev = head;
	}
	b->k = k;
	b->sz = sz;
	b->v = v;

	return b;
}

/* lookup a key in a HT */
static struct bucket *
ht_get(struct ht *ht, unsigned long h, char *k, size_t sz) {

	struct bucket *b = &ht->slots[h % ht->sz];

	while(b) {
		if(b->k && sz == b->sz && memcmp(k, b->k, sz) == 0) { /* found! */
			return b;
		}
		b = b->collision_next;
	}
	return NULL;
}


/* record a bucket as used */
static void
ht_record_used_bucket(struct ht *ht, struct bucket *b) {

	if(ht->first) {
		ht->first->prev = b;
	}
	b->next = ht->first;
	ht->first = b;
}

/* create a new dictionary */
struct dict *
dict_new(int sz) {
	
	struct dict *d = calloc(sizeof(struct dict), 1);
	d->ht = ht_new(sz); /* a single pre-defined HT */

	return d;
}

/* delete a dictionary */
void
dict_free(struct dict *d) {

	/* free both hash tables */
	ht_free(d->ht);
	if(d->ht_old) ht_free(d->ht_old);

	free(d);
}

/* transfer K items from the old hash table to the new one. */
static void
dict_rehash(struct dict *d) {
	
	if(d->ht_old == NULL) {
		return;
	}
	int k = DICT_REHASH_BATCH_SIZE;

	/* transfer old elements to the new HT. */

	struct bucket *b, *next;
	for(b = d->ht_old->first; b && k--;) {

		struct bucket *b_new;

		if((b_new = ht_insert(d->ht, b->k, b->sz, b->v))) {
			/* new used slot, add to list. */
			ht_record_used_bucket(d->ht, b_new);
		}

		next = b->next;

		/* re-attach b's neighbours together and free b. */
		bucket_free(b);
		b = next;
	}

	if((d->ht_old->first = b)) {
		return;
	}

	ht_free(d->ht_old);
	d->ht_old = NULL;
}


/* add an item into the dictionary */
void
dict_add(struct dict *d, char *k, size_t sz, void *v) {

	struct bucket *b;

	/* check for important load and resize if need be. */
	if((float)d->count / (float)d->ht->sz > DICT_MAX_LOAD) {
		/* expand and replace HT */
		d->ht_old = d->ht;
		d->ht = ht_new(d->ht->sz * 2);
	}

	if((b = ht_insert(d->ht, k, sz, v))) {
		d->count++;
		ht_record_used_bucket(d->ht, b);
	}
	
	dict_rehash(d);
}

void*
dict_get(struct dict *d, char *k, size_t sz) {

	struct bucket *b;
	unsigned long h = djb_hash(k, sz);

	if((b = ht_get(d->ht, h, k, sz))) {
		return b->v;
	} else if(d->ht_old && (b = ht_get(d->ht_old, h, k, sz))) {
		return b->v;
	}

	return NULL;
}

int
dict_remove(struct dict *d, char *k, size_t sz) {

	struct bucket *b = NULL;
	unsigned long h = djb_hash(k, sz);

	if(!(b = ht_get(d->ht, h, k, sz))) {
		if(d->ht_old && !(b = ht_get(d->ht_old, h, k, sz))) {
			return -1;
		}
	}

	struct bucket *prev = b->prev, *next = b->next;
	prev->next = b->next;
	next->prev = b->prev;

	bucket_free(b);

	d->count--;
	return 0;
}

/* for each item, call a callback function */
void
dict_foreach(struct dict *d, foreach_cb fun, void *data) {

	int i;
	struct bucket *heads[2];

	heads[0] = d->ht->first;
	if(d->ht_old) {
		heads[1] = d->ht_old->first;
	} else {
		heads[1] = NULL;
	}

	/* call on each HT */
	for(i = 0; i < 2; ++i) {
		struct bucket *b;
		for(b = heads[i]; b; b = b->next) {
			fun(b->k, b->sz, b->v, data);
		}
	}
}

