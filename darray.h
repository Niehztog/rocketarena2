// darray.h -- generic dynamic array, see darray.c

typedef struct array_s
{
	void	*list;
	int		count;
	int		size;
	int		elemsize;
} array_t;

array_t	*ArrayNew (int elemsize);
void	ArrayFree (array_t *array);
int		ArrayLength (array_t *array);
void	*ArrayNth (array_t *array, int n);
void	ArrayAppend (array_t *array, void *elem);
void	ArrayInsertAt (array_t *array, int n, void *elem);
void	ArrayReplaceAt (array_t *array, int n, void *elem);
void	ArrayDeleteAt (array_t *array, int n);
int		ArraySearch (array_t *array, void *elem, int (*cmpFn)(const void *, const void *));
void	ArraySort (array_t *array, int (*cmpFn)(const void *, const void *));
void	ArrayMap (array_t *array, void (*mapFn)(void *));
