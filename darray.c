// darray.c -- generic dynamic array, backing the GameSpy stats subsystem.
// Confirmed real function names/addresses via nm on gamei386.so; the
// struct field names below (list, count) are confirmed via embedded
// assert() strings ("array->list", "(n >= 0) && (n < array->count)").
// Internal growth/element-size bookkeeping is a reasonable, idiomatic
// implementation matching that evidence, not a byte-exact recovery.

#include "g_local.h"
#include "darray.h"

#define ARRAY_INITIAL	8

static void ArrayGrow (array_t *array, int mincount)
{
	int		newsize;
	void	*newlist;

	newsize = array->size ? array->size : ARRAY_INITIAL;
	while (newsize < mincount)
		newsize *= 2;

	newlist = gi.TagMalloc (newsize * array->elemsize, TAG_LEVEL);
	if (array->list)
	{
		memcpy (newlist, array->list, array->count * array->elemsize);
	}
	array->list = newlist;
	array->size = newsize;
}

array_t *ArrayNew (int elemsize)
{
	array_t	*array;

	array = gi.TagMalloc (sizeof(array_t), TAG_LEVEL);
	array->list = NULL;
	array->count = 0;
	array->size = 0;
	array->elemsize = elemsize;

	return array;
}

void ArrayFree (array_t *array)
{
	if (!array)
		return;
	array->list = NULL;
	array->count = 0;
	array->size = 0;
}

int ArrayLength (array_t *array)
{
	assert (array);
	return array->count;
}

void *ArrayNth (array_t *array, int n)
{
	assert (array && array->list);
	assert ((n >= 0) && (n < array->count));

	return (char *)array->list + n * array->elemsize;
}

void ArrayAppend (array_t *array, void *elem)
{
	assert (array);

	if (array->count >= array->size)
		ArrayGrow (array, array->count + 1);

	memcpy ((char *)array->list + array->count * array->elemsize, elem, array->elemsize);
	array->count++;
}

void ArrayInsertAt (array_t *array, int n, void *elem)
{
	char	*dst;

	assert (array);
	assert ((n >= 0) && (n <= array->count));

	if (array->count >= array->size)
		ArrayGrow (array, array->count + 1);

	dst = (char *)array->list + n * array->elemsize;
	if (n < array->count)
		memmove (dst + array->elemsize, dst, (array->count - n) * array->elemsize);

	memcpy (dst, elem, array->elemsize);
	array->count++;
}

void ArrayReplaceAt (array_t *array, int n, void *elem)
{
	assert (array && array->list);
	assert ((n >= 0) && (n < array->count));

	memcpy ((char *)array->list + n * array->elemsize, elem, array->elemsize);
}

void ArrayDeleteAt (array_t *array, int n)
{
	char	*dst;

	assert (array && array->list);
	assert ((n >= 0) && (n < array->count));

	dst = (char *)array->list + n * array->elemsize;
	if (n < array->count - 1)
		memmove (dst, dst + array->elemsize, (array->count - n - 1) * array->elemsize);

	array->count--;
}

int ArraySearch (array_t *array, void *elem, int (*cmpFn)(const void *, const void *))
{
	int		i;

	assert (array);

	for (i = 0; i < array->count; i++)
	{
		if (cmpFn (ArrayNth (array, i), elem) == 0)
			return i;
	}

	return -1;
}

void ArraySort (array_t *array, int (*cmpFn)(const void *, const void *))
{
	assert (array);

	if (array->count > 1)
		qsort (array->list, array->count, array->elemsize, cmpFn);
}

void ArrayMap (array_t *array, void (*mapFn)(void *))
{
	int		i;

	assert (array);

	for (i = 0; i < array->count; i++)
		mapFn (ArrayNth (array, i));
}
