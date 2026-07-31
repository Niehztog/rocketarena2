#ifndef _HASHTABLE_H
#define _HASHTABLE_H

#include "darray.h"

typedef struct table_s
{
	array_t	**buckets;
	int		nBuckets;
	void	(*freefn)(void *entry);
	int		(*hashFn)(void *key, int nBuckets);
	int		(*compFn)(void *key1, void *key2);
} table_t;

table_t	*TableNew (int elemSize, int nBuckets, int (*hashFn)(void *key, int nBuckets), int (*compFn)(void *key1, void *key2), void (*freefn)(void *entry));
void	TableFree (table_t *table);
void	TableEnter (table_t *table, void *entry);
void	*TableLookup (table_t *table, void *key);
void	TableMap (table_t *table, void (*mapFn)(void *entry, void *userdata), void *userdata);
int		TableCount (table_t *table);

#endif // _HASHTABLE_H
