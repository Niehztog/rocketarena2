// hashtable.h -- generic hash table, see hashtable.c

typedef struct tableentry_s
{
	void				*key;
	void				*data;
	struct tableentry_s	*next;
} tableentry_t;

typedef struct table_s
{
	int		(*hashFn)(void *key);
	int		(*compFn)(void *key1, void *key2);
	int		elemSize;
	int		nBuckets;
	int		count;
	tableentry_t	**buckets;
} table_t;

table_t	*TableNew (int nBuckets, int (*hashFn)(void *key), int (*compFn)(void *key1, void *key2));
void	TableFree (table_t *table);
void	TableEnter (table_t *table, void *key, void *data);
void	*TableLookup (table_t *table, void *key);
void	TableMap (table_t *table, void (*mapFn)(void *key, void *data));
int		TableCount (table_t *table);
