#include "g_local.h"
#include "darray.h"

#define ARRAY_INITIAL	8

static void *mylsearch (void *key, void *base, int num, int width,
	int (*cmpFn)(const void *, const void *));




















/* gamex86.dll 0x200058c0-0x200058e0 (manual-confirmed) */
/* gamei386.so: no symbol -- inlined into its callers */
static void ArrayFreeElem (array_t *array, int n)
{
	if (array->freefn)
		array->freefn (ArrayNth (array, n));
}

/* gamex86.dll 0x200059d0-0x20005a02 (manual-confirmed) */
/* gamei386.so: no symbol -- inlined into its callers */
static void ArrayNthCopy (array_t *array, void *elem, int n)
{
	memcpy (ArrayNth (array, n), elem, array->elemsize);
}

/* gamex86.dll 0x200059a0-0x200059d0 (manual-confirmed) */
/* gamei386.so: no symbol -- inlined into its callers */
static void ArrayGrow (array_t *array)
{
	array->size += array->growby;
	array->list = realloc (array->list, array->size * array->elemsize);
	assert (array->list);
}









/* gamex86.dll 0x20005830-0x20005880 (manual-confirmed) */
/* gamei386.so 0x000553d4-0x00055477 */
array_t *ArrayNew (int elemSize, int growby, void (*freefn)(void *elem))
{
	array_t	*array;
	array = malloc (sizeof(*array));
	assert (array);
	assert (elemSize);
	if (!growby)
		growby = ARRAY_INITIAL;
	array->count = 0;
	array->size = growby;
	array->elemsize = elemSize;
	array->growby = growby;
	array->freefn = freefn;
	array->list = malloc (array->size * array->elemsize);
	assert (array->list);
	return array;
}

/* gamex86.dll 0x20005880-0x200058c0 (manual-confirmed) */
/* gamei386.so 0x00055478-0x000554d0 */
void ArrayFree (array_t *array)
{
	int		i;
	assert (array);
	for (i = 0; i < array->count; i++)
		ArrayFreeElem (array, i);
	free (array->list);
	free (array);
}

/* gamex86.dll 0x200058e0-0x200058f0 (manual-confirmed(byte-identical)) */
/* gamei386.so 0x000554d0-0x000554d7 */
int ArrayLength (array_t *array)
{
	return array->count;
}
/* gamex86.dll 0x200058f0-0x20005910 (shape-matched(ratio=1.00)) */
/* gamei386.so 0x000554d8-0x00055508 */
void *ArrayNth (array_t *array, int n)
{
	assert ((n >= 0) && (n < array->count));
	return (char *)array->list + array->elemsize * n;
}

/* gamex86.dll 0x20005910-0x20005930 (call-propagated+collision-resolved) */
/* gamei386.so 0x00055508-0x0005551e */
void ArrayAppend (array_t *array, void *elem)
{
	ArrayInsertAt (array, elem, array->count);
}

/* gamex86.dll 0x20005930-0x200059a0 (manual-confirmed) */
/* gamei386.so 0x00055520-0x00055630 */
void ArrayInsertAt (array_t *array, void *elem, int n)
{
	assert ((n >= 0) && (n <= array->count));
	if (array->count == array->size)
		ArrayGrow (array);
	array->count++;
	if (n < array->count - 1)
		memmove (ArrayNth (array, n + 1), ArrayNth (array, n), (array->count - 1 - n) * array->elemsize);
	ArrayNthCopy (array, elem, n);
}

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x00055630-0x000556ea */
void ArrayDeleteAt (array_t *array, int n)
{
	assert ((n >= 0) && (n < array->count));
	ArrayFreeElem (array, n);
	if (n < array->count - 1)
	{
		int		next = n + 1;
		memmove (ArrayNth (array, n), ArrayNth (array, next), (array->count - next) * array->elemsize);
	}
	array->count--;
}
#line 138
/* gamex86.dll 0x20005a10-0x20005a33 (manual-confirmed) */
/* gamei386.so 0x000556ec-0x00055750 */
void ArrayReplaceAt (array_t *array, void *elem, int n)
{
	assert ((n >= 0) && (n < array->count));

	ArrayFreeElem (array, n);
	ArrayNthCopy (array, elem, n);
}

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x00055750-0x0005576d */
void ArraySort (array_t *array, int (*cmpFn)(const void *, const void *))
{
	qsort (array->list, array->count, array->elemsize, cmpFn);
}

/* gamex86.dll 0x20005a40-0x20005ab0 (manual-confirmed) */
/* gamei386.so 0x00055770-0x00055825 */
int ArraySearch (array_t *array, void *key, int (*cmpFn)(const void *, const void *),
	int start, qboolean sorted)
{
	void	*found;
	if (!array->count)
		return -1;
	if (sorted)
		found = bsearch (key, ArrayNth (array, start), array->count - start,
			array->elemsize, cmpFn);
	else
		found = mylsearch (key, ArrayNth (array, start), array->count - start,
			array->elemsize, cmpFn);
	if (found)
		return ((char *)found - (char *)array->list) / array->elemsize;
	return -1;
}

/* gamex86.dll 0x20005ab0-0x20005af0 (manual-confirmed(byte-identical structure)) */
/* gamei386.so 0x00055828-0x00055897 */
void ArrayMap (array_t *array, void (*fn)(void *elem, void *userdata), void *userdata)
{
	int		i;

	assert (fn);

	for (i = 0; i < array->count; i++)
		fn (ArrayNth (array, i), userdata);
}

/* gamex86.dll 0x20005af0-0x20005b40 (manual-confirmed) */
/* gamei386.so 0x00055898-0x00055968 */
static void *mylsearch (void *key, void *base, int num, int width,
	int (*cmpFn)(const void *, const void *))
{
	int		i;

	for (i = 0; i < num; i++)
		if (!cmpFn (key, (char *)base + i * width))
			return (char *)base + i * width;

	return NULL;
}
