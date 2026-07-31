// hashtable.c -- generic hash table, backing the GameSpy stats subsystem.
// Confirmed real function names/addresses via nm on gamei386.so; the
// field names below (hashFn, compFn, elemSize, nBuckets, table,
// table->buckets) are confirmed via embedded assert() strings naming
// them exactly. Bucket-chain implementation is a standard, idiomatic
// separate-chaining hash table matching that evidence, not a
// byte-exact recovery of the original arithmetic.

#include "g_local.h"
#include "hashtable.h"

table_t *TableNew (int nBuckets, int (*hashFn)(void *key), int (*compFn)(void *key1, void *key2))
{
	table_t	*table;

	assert (nBuckets > 0);
	assert (hashFn);
	assert (compFn);

	table = gi.TagMalloc (sizeof(table_t), TAG_LEVEL);
	table->hashFn = hashFn;
	table->compFn = compFn;
	table->nBuckets = nBuckets;
	table->count = 0;
	table->buckets = gi.TagMalloc (nBuckets * sizeof(tableentry_t *), TAG_LEVEL);

	return table;
}

void TableFree (table_t *table)
{
	assert (table);
	assert (table->buckets);

	table->buckets = NULL;
	table->count = 0;
}

void TableEnter (table_t *table, void *key, void *data)
{
	int				h;
	tableentry_t	*entry;

	assert (table);
	assert (table->buckets);

	h = table->hashFn (key) % table->nBuckets;
	if (h < 0)
		h += table->nBuckets;

	entry = gi.TagMalloc (sizeof(tableentry_t), TAG_LEVEL);
	entry->key = key;
	entry->data = data;
	entry->next = table->buckets[h];
	table->buckets[h] = entry;
	table->count++;
}

void *TableLookup (table_t *table, void *key)
{
	int				h;
	tableentry_t	*entry;

	assert (table);
	assert (table->buckets);

	h = table->hashFn (key) % table->nBuckets;
	if (h < 0)
		h += table->nBuckets;

	for (entry = table->buckets[h]; entry; entry = entry->next)
	{
		if (table->compFn (entry->key, key) == 0)
			return entry->data;
	}

	return NULL;
}

void TableMap (table_t *table, void (*mapFn)(void *key, void *data))
{
	int				i;
	tableentry_t	*entry;

	assert (table);
	assert (table->buckets);

	for (i = 0; i < table->nBuckets; i++)
		for (entry = table->buckets[i]; entry; entry = entry->next)
			mapFn (entry->key, entry->data);
}

int TableCount (table_t *table)
{
	assert (table);
	return table->count;
}
