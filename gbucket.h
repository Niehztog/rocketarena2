#ifndef _GBUCKET_H
#define _GBUCKET_H

#include "hashtable.h"

typedef enum
{
    bt_int,
    bt_float,
    bt_string
} buckettype_t;

#define NUMBUCKETTYPES  3

typedef struct
{
    char    *buf;
    int     size;
    int     len;
} dumpbuf_t;

#define DUMPBUF_START   128

typedef struct bucket_s
{
    char            *key;
    buckettype_t    type;
    int             samples;
    union
    {
        int     ival;
        double  dval;
        char    *sval;
    } value;
} bucket_t;

typedef struct bucketset_s
{
    table_t *buckets;
} bucketset_t;

typedef void *(*bucketop_t) (bucketset_t *set, char *key, void *value);

#define NUMBUCKETOPS    7

#define BUCKET_SET      0
#define BUCKET_ADD      1
#define BUCKET_SUB      2
#define BUCKET_MULT     3
#define BUCKET_DIV      4
#define BUCKET_CONCAT   5
#define BUCKET_AVG      6

extern bucketop_t   bucketfuncs[NUMBUCKETOPS];

typedef int (*bucketopfn_t) (void *gamep, char *key, bucketop_t op, int value, int owner);

typedef char *(*bucketopstrfn_t) (void *gamep, char *key, bucketop_t op, char *value, int owner);

#define NUMBUCKETOPFNS  9

#define BOP_SERVER_INT      0
#define BOP_SERVER_FLOAT    1
#define BOP_SERVER_STRING   2
#define BOP_TEAM_INT        3
#define BOP_TEAM_FLOAT      4
#define BOP_TEAM_STRING     5
#define BOP_PLAYER_INT      6
#define BOP_PLAYER_FLOAT    7
#define BOP_PLAYER_STRING   8

extern bucketopfn_t bopfuncs[NUMBUCKETOPFNS];

#define SETINT(gamep,key,value) \
    bopfuncs[BOP_SERVER_INT] (gamep, key, bucketfuncs[BUCKET_SET], value, 0)
#define SETSTR(gamep,key,value) \
    ((bucketopstrfn_t)bopfuncs[BOP_SERVER_STRING]) (gamep, key, \
        bucketfuncs[BUCKET_SET], value, 0)

bucketset_t *NewBucketSet (void);
void        FreeBucketSet (bucketset_t *set);
char        *DumpBucketSet (bucketset_t *set);
void        *BucketNew (bucketset_t *set, char *key, buckettype_t type, void *value);
void        *BucketSet (bucketset_t *set, char *key, void *value);
void        *BucketGet (bucketset_t *set, char *key);
void        *BucketAdd (bucketset_t *set, char *key, void *value);
void        *BucketSub (bucketset_t *set, char *key, void *value);
void        *BucketMult (bucketset_t *set, char *key, void *value);
void        *BucketDiv (bucketset_t *set, char *key, void *value);
void        *BucketConcat (bucketset_t *set, char *key, void *value);
void        *BucketAvg (bucketset_t *set, char *key, void *value);

void        *bint (int value);
void        *bfloat (double value);

#endif // _GBUCKET_H
