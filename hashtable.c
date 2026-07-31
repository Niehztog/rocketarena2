#include "g_local.h"
#include "hashtable.h"



























/* gamex86.dll 0x2001cb10-0x2001cb80 (shape-matched(ratio=0.60)) */
/* gamei386.so 0x00055968-0x00055b45 */
table_t *TableNew (int elemSize, int nBuckets, int (*hashFn)(void *key, int nBuckets), int (*compFn)(void *key1, void *key2), void (*freefn)(void *entry))
{
	table_t	*table;
	int		i;
	assert (hashFn);
	assert (compFn);
	assert (elemSize);
	assert (nBuckets);

	table = malloc (sizeof(*table));
	assert (table);

	table->buckets = malloc (nBuckets * sizeof(array_t *));
	assert (table->buckets);

	for (i = 0; i < nBuckets; i++)
		table->buckets[i] = ArrayNew (elemSize, 0, freefn);
	table->nBuckets = nBuckets;
	table->freefn = freefn;
	table->compFn = compFn;
	table->hashFn = hashFn;
	return table;
}

/* gamex86.dll 0x2001cb80-0x2001cbc0 (shape-matched(ratio=1.00)) */
/* gamei386.so 0x00055b48-0x00055b98 */
void TableFree (table_t *table)
{
	int		i;
	assert (table);
	for (i = 0; i < table->nBuckets; i++)
		ArrayFree (table->buckets[i]);
	free (table->buckets);
	free (table);
}

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x00055b98-0x00055bc4 */
int TableCount (table_t *table)
{
	int		i, total;
	total = 0;
	for (i = 0; i < table->nBuckets; i++)
		total += ArrayLength (table->buckets[i]);
	return total;
}

/* gamex86.dll 0x2001cbc0-0x2001cc18 (aligned+size-corrected) */
/* gamei386.so 0x00055bc4-0x00055c20 */
void TableEnter (table_t *table, void *entry)
{
	int			h;
	int			idx;
	h = table->hashFn (entry, table->nBuckets);
	idx = ArraySearch (table->buckets[h], entry,
		(int (*)(const void *, const void *))table->compFn, 0, false);
	if (idx == -1)
		ArrayAppend (table->buckets[h], entry);
	else
		ArrayReplaceAt (table->buckets[h], entry, idx);
}

/* gamex86.dll 0x2001cc20-0x2001cc70 (shape-matched(ratio=0.72)) */
/* gamei386.so 0x00055c20-0x00055c6e */
void *TableLookup (table_t *table, void *key)
{
	int			h;
	int			idx;
	h = table->hashFn (key, table->nBuckets);
	idx = ArraySearch (table->buckets[h], key,
		(int (*)(const void *, const void *))table->compFn, 0, false);
	if (idx == -1)
		return NULL;
	return ArrayNth (table->buckets[h], idx);
}

/* gamex86.dll 0x2001cc70-0x2001ccb0 (manual-confirmed(byte-identical structure)) */
/* gamei386.so 0x00055c70-0x00055cbf */
void TableMap (table_t *table, void (*fn)(void *entry, void *userdata), void *userdata)
{
	int		i;
	assert (fn);

	for (i = 0; i < table->nBuckets; i++)
		ArrayMap (table->buckets[i], fn, userdata);
}
