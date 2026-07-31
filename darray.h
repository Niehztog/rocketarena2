#ifndef _DARRAY_H
#define _DARRAY_H

typedef struct array_s
{
	int		count;
	int		size;
	int		elemsize;
	int		growby;
	void	(*freefn)(void *elem);
	void	*list;
} array_t;

array_t	*ArrayNew (int elemSize, int growby, void (*freefn)(void *elem));
void	ArrayFree (array_t *array);
int		ArrayLength (array_t *array);
void	*ArrayNth (array_t *array, int n);
void	ArrayAppend (array_t *array, void *elem);
void	ArrayInsertAt (array_t *array, void *elem, int n);
void	ArrayReplaceAt (array_t *array, void *elem, int n);
void	ArrayDeleteAt (array_t *array, int n);
int		ArraySearch (array_t *array, void *key, int (*cmpFn)(const void *, const void *),
			int start, qboolean sorted);
void	ArraySort (array_t *array, int (*cmpFn)(const void *, const void *));
void	ArrayMap (array_t *array, void (*mapFn)(void *elem, void *userdata), void *userdata);

#endif // _DARRAY_H
