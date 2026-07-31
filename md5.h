// md5.h -- RSA Data Security, Inc. MD5 Message-Digest Algorithm.
// Function names (MD5Init/MD5Update/MD5Final/MD5_memcpy/MD5_memset)
// confirmed real via nm on gamei386.so -- this exact naming (including
// the MD5_memcpy/MD5_memset wrapper names) is the signature of RSA's
// own public-domain reference implementation, not a custom rewrite, so
// the standard reference algorithm is used verbatim here.

typedef struct
{
	unsigned long	state[4];
	unsigned long	count[2];
	unsigned char	buffer[64];
} MD5_CTX;

void MD5Init (MD5_CTX *context);
void MD5Update (MD5_CTX *context, unsigned char *input, unsigned int inputLen);
void MD5Final (unsigned char digest[16], MD5_CTX *context);
char *MD5Print (unsigned char digest[16]);
char *MD5Digest (unsigned char *input, unsigned int inputLen);
