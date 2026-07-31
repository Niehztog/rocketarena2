// gbucket.h -- tagged-value store backing per-player/team/server stats,
// see gbucket.c

typedef enum
{
	bt_int,
	bt_float,
	bt_string
} buckettype_t;

typedef struct bucket_s
{
	char			*key;
	buckettype_t	type;
	union
	{
		int		ival;
		float	fval;
		char	*sval;
	} value;
	struct bucket_s	*next;
} bucket_t;

typedef struct bucketset_s
{
	bucket_t	*buckets;
	int			count;
} bucketset_t;

bucketset_t	*BucketNew (void);
void		BucketFree (bucketset_t *set);
void		FreeBucketSet (bucketset_t *set);
void		BucketSet (bucketset_t *set, char *key, buckettype_t type, void *value);
bucket_t	*BucketGet (bucketset_t *set, char *key);
void		BucketAdd (bucketset_t *set, char *key, buckettype_t type, void *value);
void		BucketSub (bucketset_t *set, char *key, buckettype_t type, void *value);
void		BucketMult (bucketset_t *set, char *key, float scale);
void		BucketDiv (bucketset_t *set, char *key, float scale);
float		BucketAvg (bucketset_t *set, char *key, int count);
void		BucketConcat (bucketset_t *dst, bucketset_t *src);
char		*DumpMap (bucketset_t *set);
