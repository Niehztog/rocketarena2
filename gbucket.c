// gbucket.c -- tagged-value store backing per-player/team/server GameSpy
// stats. Confirmed real function names/addresses via nm on gamei386.so;
// the "pbucket->type == bt_string" assert string confirms both the
// field name and the bt_string enumerator name exactly. DoFind/DoGet/
// DoSet/DumpMap are this file's own confirmed-real internal helpers
// (static/local in the binary, matching their linkage here).

#include "g_local.h"
#include "gbucket.h"

static bucket_t *DoFind (bucketset_t *set, char *key)
{
	bucket_t	*b;

	assert (set);

	for (b = set->buckets; b; b = b->next)
	{
		if (!Q_stricmp (b->key, key))
			return b;
	}

	return NULL;
}

static bucket_t *DoSet (bucketset_t *set, char *key, buckettype_t type, void *value)
{
	bucket_t	*b;

	assert (set);

	b = DoFind (set, key);
	if (!b)
	{
		b = gi.TagMalloc (sizeof(bucket_t), TAG_LEVEL);
		b->key = gi.TagMalloc (strlen(key) + 1, TAG_LEVEL);
		strcpy (b->key, key);
		b->next = set->buckets;
		set->buckets = b;
		set->count++;
	}

	b->type = type;
	switch (type)
	{
	case bt_int:
		b->value.ival = *(int *)value;
		break;
	case bt_float:
		b->value.fval = *(float *)value;
		break;
	case bt_string:
		b->value.sval = gi.TagMalloc (strlen((char *)value) + 1, TAG_LEVEL);
		strcpy (b->value.sval, (char *)value);
		break;
	}

	return b;
}

static void DoGet (bucket_t *b, void *out)
{
	assert (b);

	switch (b->type)
	{
	case bt_int:
		*(int *)out = b->value.ival;
		break;
	case bt_float:
		*(float *)out = b->value.fval;
		break;
	case bt_string:
		*(char **)out = b->value.sval;
		break;
	}
}

bucketset_t *BucketNew (void)
{
	bucketset_t	*set;

	set = gi.TagMalloc (sizeof(bucketset_t), TAG_LEVEL);
	set->buckets = NULL;
	set->count = 0;

	return set;
}

void BucketFree (bucketset_t *set)
{
	if (!set)
		return;
	set->buckets = NULL;
	set->count = 0;
}

void FreeBucketSet (bucketset_t *set)
{
	BucketFree (set);
}

void BucketSet (bucketset_t *set, char *key, buckettype_t type, void *value)
{
	DoSet (set, key, type, value);
}

bucket_t *BucketGet (bucketset_t *set, char *key)
{
	return DoFind (set, key);
}

void BucketAdd (bucketset_t *set, char *key, buckettype_t type, void *value)
{
	bucket_t	*b;
	int			iv;
	float		fv;

	b = DoFind (set, key);
	if (!b)
	{
		DoSet (set, key, type, value);
		return;
	}

	assert (b->type == type);

	if (type == bt_int)
	{
		iv = b->value.ival + *(int *)value;
		DoSet (set, key, type, &iv);
	}
	else if (type == bt_float)
	{
		fv = b->value.fval + *(float *)value;
		DoSet (set, key, type, &fv);
	}
}

void BucketSub (bucketset_t *set, char *key, buckettype_t type, void *value)
{
	bucket_t	*b;
	int			iv;
	float		fv;

	b = DoFind (set, key);
	if (!b)
		return;

	assert (b->type == type);

	if (type == bt_int)
	{
		iv = b->value.ival - *(int *)value;
		DoSet (set, key, type, &iv);
	}
	else if (type == bt_float)
	{
		fv = b->value.fval - *(float *)value;
		DoSet (set, key, type, &fv);
	}
}

void BucketMult (bucketset_t *set, char *key, float scale)
{
	bucket_t	*b;
	float		fv;

	b = DoFind (set, key);
	if (!b)
		return;

	if (b->type == bt_float)
		fv = b->value.fval * scale;
	else
		fv = (float)b->value.ival * scale;

	DoSet (set, key, bt_float, &fv);
}

void BucketDiv (bucketset_t *set, char *key, float scale)
{
	bucket_t	*b;
	float		fv;

	if (scale == 0.0f)
		return;

	b = DoFind (set, key);
	if (!b)
		return;

	if (b->type == bt_float)
		fv = b->value.fval / scale;
	else
		fv = (float)b->value.ival / scale;

	DoSet (set, key, bt_float, &fv);
}

float BucketAvg (bucketset_t *set, char *key, int count)
{
	bucket_t	*b;

	if (count <= 0)
		return 0.0f;

	b = DoFind (set, key);
	if (!b)
		return 0.0f;

	if (b->type == bt_float)
		return b->value.fval / (float)count;

	return (float)b->value.ival / (float)count;
}

void BucketConcat (bucketset_t *dst, bucketset_t *src)
{
	bucket_t	*b;
	void		*v;
	int			iv;
	float		fv;
	char		*sv;

	if (!src)
		return;

	for (b = src->buckets; b; b = b->next)
	{
		switch (b->type)
		{
		case bt_int:
			iv = b->value.ival;
			v = &iv;
			break;
		case bt_float:
			fv = b->value.fval;
			v = &fv;
			break;
		default:
			sv = b->value.sval;
			v = sv;
			break;
		}
		DoSet (dst, b->key, b->type, v);
	}
}

// Serializes a bucket set into GameSpy's "\key\value\key\value..." wire
// format for SendGameSnapShot -- the same backslash-delimited convention
// used throughout this whole protocol (see value_for_key in stats.c).
char *DumpMap (bucketset_t *set)
{
	static char	buf[4096];
	bucket_t	*b;
	char		tmp[128];
	int			ival;
	float		fval;
	char		*sval;

	buf[0] = 0;

	if (!set)
		return buf;

	for (b = set->buckets; b; b = b->next)
	{
		strcat (buf, "\\");
		strcat (buf, b->key);
		strcat (buf, "\\");

		DoGet (b, b->type == bt_int ? (void *)&ival : b->type == bt_float ? (void *)&fval : (void *)&sval);

		switch (b->type)
		{
		case bt_int:
			Com_sprintf (tmp, sizeof(tmp), "%d", ival);
			strcat (buf, tmp);
			break;
		case bt_float:
			Com_sprintf (tmp, sizeof(tmp), "%f", fval);
			strcat (buf, tmp);
			break;
		case bt_string:
			strcat (buf, sval);
			break;
		}
	}

	return buf;
}
