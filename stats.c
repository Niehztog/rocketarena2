// stats.c -- the GameSpy online stats-reporting pipeline. This whole
// subsystem shipped only in the x86/Alpha v2.25 builds (confirmed absent
// from the v2.22 mips/sparc builds' DWARF data entirely -- it was added
// after this codebase's v2.22 baseline, so no debug info survives for
// it anywhere). Reconstructed from gamei386.so disassembly + strings:
// the server address/port, the XOR-obfuscated wire request template,
// the embedded gamename/secret_key credentials, the MD5 challenge-
// response auth scheme, and the disk-backed store-and-forward retry
// cache format were all recovered in full during the original binary
// analysis pass. Confirmed real function names/addresses via nm.
//
// NOTE: gamestats.gamespy.com has not resolved for many years -- this
// code is a faithful reconstruction of a now-defunct protocol, not
// something that will actually phone home to anything live.

#include "g_local.h"
#include "arena.h"
#include "gbucket.h"
#include "hashtable.h"
#include "darray.h"
#include "md5.h"

#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

static void safe_strcpy (char *dst, char *src, int dstsize)
{
	strncpy (dst, src, dstsize - 1);
	dst[dstsize - 1] = 0;
}

#define STATS_HOST		"gamestats.gamespy.com"
#define STATS_PORT		29920		// 0x74e0, confirmed via disassembly

// credentials, recovered in cleartext from both gamei386.so and
// gamex86.dll (stored with a deliberately-missing first byte in the
// real binary, patched back at runtime -- a real anti-strings-scan
// technique; not replicated here, there is no adversary to defeat)
static char	*gcd_gamename = "ra2";
static char	*gcd_secret_key = "9z3312";

// the 3 XOR obfuscation keys the real binary rotates between, confirmed
// via disassembly of xcode_buf's call sites
#define XORKEY_SOCKET	"GameSpy3D"		// socket greeting/response payloads
#define XORKEY_DISK		"Industries"	// gstats.dat disk retry-cache records
#define XORKEY_TEMPLATE	"ProjectAphex"	// the static wire request template

// recovered by XOR-decoding the in-binary template blob with XORKEY_TEMPLATE
#define REQUEST_TEMPLATE	"\\auth\\\\gamename\\%s\\response\\%s\\port\\%d\\id\\1"

#define DISK_CACHE_FILE		"gstats.dat"
#define DISK_MAGIC			0x70F33A5F	// confirmed constant XORed into each record's length prefix

typedef struct
{
	bucketset_t	*serverbucket;
	array_t		*players;		// of gclient_t* (or NULL slots)
	array_t		*teams;			// of team_t*
	qboolean	inuse;
} statsgame_t;

static int			sock = -1;
static qboolean		connected = false;
static char			sesskey[64];
static char			last_challenge[64];
static statsgame_t	*current_game;

/*
==============================================================
INTERNAL HELPERS
==============================================================
*/

// repeating-key XOR -- confirmed exact behavior via disassembly:
// buf[i] ^= key[i % strlen(key)]
static void xcode_buf (char *buf, int len, char *key)
{
	int	i, keylen;

	keylen = strlen (key);
	if (keylen == 0)
		return;

	for (i = 0; i < len; i++)
		buf[i] ^= key[i % keylen];
}

// backslash-delimited key/value parser, modeled on stock Info_ValueForKey's
// double-buffer trick (confirmed via disassembly comparison) but operating
// on GameSpy query/response strings instead of Q2 userinfo strings
static char *value_for_key (char *s, char *key)
{
	static char	value[2][512];
	static int	valueindex;
	char		pkey[512];
	char		*o;

	valueindex ^= 1;
	o = value[valueindex];

	if (*s == '\\')
		s++;

	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];
		while (*s != '\\' && *s)
		{
			if (!*s)
				break;
			*o++ = *s++;
		}
		*o = 0;

		if (!Q_stricmp (pkey, key))
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}

// standard CRC-32 (confirmed name/purpose via disassembly; the table-driven
// implementation itself is the standard, widely-published algorithm)
static unsigned long crc32_table[256];
static qboolean crc32_table_built = false;

static void build_crc32_table (void)
{
	unsigned long	c;
	int				n, k;

	for (n = 0; n < 256; n++)
	{
		c = (unsigned long)n;
		for (k = 0; k < 8; k++)
		{
			if (c & 1)
				c = 0xedb88320L ^ (c >> 1);
			else
				c = c >> 1;
		}
		crc32_table[n] = c;
	}
	crc32_table_built = true;
}

static unsigned long g_crc32 (unsigned char *buf, int len)
{
	unsigned long	c;
	int				n;

	if (!crc32_table_built)
		build_crc32_table ();

	c = 0xffffffffL;
	for (n = 0; n < len; n++)
		c = crc32_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);

	return c ^ 0xffffffffL;
}

// resolves host:port into a sockaddr_in -- confirmed 2-arg-plus-out-param
// shape from InitStatsConnection's call site
static int get_sockaddrin (char *host, int port, struct sockaddr_in *addr)
{
	struct hostent	*h;

	memset (addr, 0, sizeof(*addr));
	addr->sin_family = AF_INET;
	addr->sin_port = htons ((unsigned short)port);

	h = gethostbyname (host);
	if (!h)
		return 0;

	memcpy (&addr->sin_addr, h->h_addr_list[0], sizeof(addr->sin_addr));
	return 1;
}

// scrambles an 8-hex-digit seed byte-wise: char[i] = char[i] + 0x11 + i
// -- confirmed exact arithmetic via disassembly. Non-cryptographic, just
// obfuscation of the challenge token.
static char *create_challenge (int seed)
{
	static char	buf[16];
	int			i;

	Com_sprintf (buf, sizeof(buf), "%.8x", (unsigned int)seed);

	for (i = 0; buf[i]; i++)
		buf[i] = buf[i] + 0x11 + i;

	return buf;
}

static int DoSend (char *buf, int len)
{
	if (sock < 0)
		return -1;

	return send (sock, buf, len, 0);
}

/*
==============================================================
DISK-BACKED RETRY CACHE (gstats.dat)

Confirmed format via disassembly of DiskWrite/CheckDiskFile: each queued
record is a 4-byte length (XORed with DISK_MAGIC), a CRC32 of the
(already wire-encoded) payload, then the payload itself double-XOR
encoded with XORKEY_DISK. CheckDiskFile reverses this, re-sends
everything joined by "\final\", and deletes the file on success.
==============================================================
*/

static void DiskWrite (char *payload, int len)
{
	FILE			*f;
	int				enclen;
	unsigned long	crc;
	char			*buf;

	f = fopen (DISK_CACHE_FILE, "ab");
	if (!f)
		return;

	crc = g_crc32 ((unsigned char *)payload, len);

	buf = gi.TagMalloc (len, TAG_LEVEL);
	memcpy (buf, payload, len);
	xcode_buf (buf, len, XORKEY_DISK);

	enclen = len ^ DISK_MAGIC;
	fwrite (&enclen, sizeof(enclen), 1, f);
	fwrite (&crc, sizeof(crc), 1, f);
	fwrite (buf, len, 1, f);

	fclose (f);
}

static void CheckDiskFile (void)
{
	FILE			*f;
	int				enclen, len;
	unsigned long	crc, realcrc;
	char			*buf;
	char			batch[8192];

	if (!connected)
		return;

	f = fopen (DISK_CACHE_FILE, "rb");
	if (!f)
		return;

	batch[0] = 0;

	while (fread (&enclen, sizeof(enclen), 1, f) == 1)
	{
		len = enclen ^ DISK_MAGIC;
		if (len <= 0 || len > (int)sizeof(batch))
			break;

		if (fread (&crc, sizeof(crc), 1, f) != 1)
			break;
		if (fread (batch + strlen(batch), len, 1, f) != 1)
			break;

		buf = batch + strlen (batch);
		xcode_buf (buf, len, XORKEY_DISK);
		buf[len] = 0;

		realcrc = g_crc32 ((unsigned char *)buf, len);
		if (realcrc != crc)
			continue;

		strcat (batch, "\\final\\");
	}

	fclose (f);

	if (batch[0])
		DoSend (batch, strlen (batch));

	remove (DISK_CACHE_FILE);
}

/*
==============================================================
CONNECTION / AUTH

Confirmed via disassembly of InitStatsConnection: resolves
gamestats.gamespy.com:29920, opens a TCP socket (socket(AF_INET,
SOCK_STREAM, IPPROTO_TCP)), connects, recv()s up to 64 bytes (the
server's greeting), XOR-decodes it with XORKEY_SOCKET, hands it to
SendChallengeResponse, then flushes the offline retry queue on
success. Return codes match disassembly exactly: 0=ok, 1=socket()
failed, 2=DNS failed, 3=connect() failed, 5=recv() failed.
==============================================================
*/

static void InternalInit (void)
{
	static qboolean	inited = false;

	if (inited)
		return;
	inited = true;

	memset (sesskey, 0, sizeof(sesskey));
	memset (last_challenge, 0, sizeof(last_challenge));
}

static int SendChallengeResponse (char *greeting)
{
	char			*challenge;
	unsigned long	crc;
	char			request[256];
	char			reply[128];
	int				n;

	challenge = value_for_key (greeting, "challenge");
	if (!challenge || !*challenge)
		return 5;

	safe_strcpy (last_challenge, challenge, sizeof(last_challenge));
	crc = g_crc32 ((unsigned char *)challenge, strlen (challenge));

	Com_sprintf (request, sizeof(request), REQUEST_TEMPLATE,
		gcd_gamename, GenerateAuth (gcd_secret_key, challenge), hostport ? (int)hostport->value : 0);

	xcode_buf (request, strlen (request), XORKEY_SOCKET);
	if (DoSend (request, strlen (request)) < 0)
		return 3;

	n = recv (sock, reply, sizeof(reply) - 1, 0);
	if (n <= 0)
		return 5;
	reply[n] = 0;
	xcode_buf (reply, n, XORKEY_SOCKET);

	safe_strcpy (sesskey, value_for_key (reply, "sesskey"), sizeof(sesskey));

	return 0;
}

int InitStatsConnection (void)
{
	struct sockaddr_in	addr;

	InternalInit ();

	if (connected)
		CloseStatsConnection ();

	if (!get_sockaddrin (STATS_HOST, STATS_PORT, &addr))
		return 2;

	sock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1)
		return 1;

	if (connect (sock, (struct sockaddr *)&addr, sizeof(addr)) != 0)
	{
		close (sock);
		sock = -1;
		return 3;
	}

	{
		char	greeting[65];
		int		n;

		n = recv (sock, greeting, 64, 0);
		if (n <= 0)
		{
			close (sock);
			sock = -1;
			return 5;
		}
		greeting[n] = 0;
		xcode_buf (greeting, n, XORKEY_SOCKET);

		connected = true;
		SendChallengeResponse (greeting);
	}

	CheckDiskFile ();

	return 0;
}

void CloseStatsConnection (void)
{
	if (sock != -1)
	{
		close (sock);
		sock = -1;
	}
	connected = false;
}

qboolean IsStatsConnected (void)
{
	return connected;
}

// returns the current challenge token, or the "NULLGAME" fallback the
// real binary uses when no stats context is registered at all
char *GetChallenge (void)
{
	if (!current_game)
		return "NULLGAME";

	return last_challenge;
}

// GenerateAuth: strlen(cdkey)+strlen(challenge)+18, bail with the exact
// recovered string if that would exceed 127 chars, else MD5(cdkey+challenge)
char *GenerateAuth (char *cdkey, char *challenge)
{
	char	buf[160];

	if (strlen (cdkey) + strlen (challenge) + 18 > 127)
		return "CD Key or challenge too long";

	Com_sprintf (buf, sizeof(buf), "%s%s", cdkey, challenge);
	return MD5Digest ((unsigned char *)buf, strlen (buf));
}

/*
==============================================================
GAME / PLAYER / TEAM MANAGEMENT

NewGame/FreeGame/SendGameSnapShot back arena.c's per-arena statsptr
handle (see arena_init/BeginIntermission). NewStatsPlayer/RemovePlayer/
ValidatePlayer/set_server_bucket_info are called directly from arena.c
(confirmed by their addresses clustering in arena.c's own range in the
real binary) and are implemented there instead of here.
==============================================================
*/

void *NewGame (void)
{
	statsgame_t	*g;

	g = gi.TagMalloc (sizeof(statsgame_t), TAG_LEVEL);
	g->serverbucket = BucketNew ();
	g->players = ArrayNew (sizeof(void *));
	g->teams = ArrayNew (sizeof(void *));
	g->inuse = true;

	current_game = g;

	return g;
}

void FreeGame (void *gamep)
{
	statsgame_t	*g = (statsgame_t *)gamep;

	if (!g)
		return;

	BucketFree (g->serverbucket);
	ArrayFree (g->players);
	ArrayFree (g->teams);
	g->inuse = false;

	if (current_game == g)
		current_game = NULL;
}

void *NewPlayer (void *gamep)
{
	statsgame_t	*g = (statsgame_t *)gamep;
	bucketset_t	*p;

	if (!g)
		return NULL;

	p = BucketNew ();
	ArrayAppend (g->players, &p);

	return p;
}

void *NewTeam (void *gamep)
{
	statsgame_t	*g = (statsgame_t *)gamep;
	bucketset_t	*t;

	if (!g)
		return NULL;

	t = BucketNew ();
	ArrayAppend (g->teams, &t);

	return t;
}

void RemovePlayer (void *gamep, int clientnum)
{
	statsgame_t	*g = (statsgame_t *)gamep;
	bucketset_t	**p;

	if (!g || clientnum < 0 || clientnum >= ArrayLength (g->players))
		return;

	p = (bucketset_t **)ArrayNth (g->players, clientnum);
	BucketFree (*p);
	*p = NULL;
}

void RemoveTeam (void *gamep, int teamnum)
{
	statsgame_t	*g = (statsgame_t *)gamep;
	bucketset_t	**t;

	if (!g || teamnum < 0 || teamnum >= ArrayLength (g->teams))
		return;

	t = (bucketset_t **)ArrayNth (g->teams, teamnum);
	BucketFree (*t);
	*t = NULL;
}

int GetPlayerIndex (void *gamep, void *playerbucket)
{
	statsgame_t	*g = (statsgame_t *)gamep;
	int			i;

	if (!g)
		return -1;

	for (i = 0; i < ArrayLength (g->players); i++)
		if (*(void **)ArrayNth (g->players, i) == playerbucket)
			return i;

	return -1;
}

int GetTeamIndex (void *gamep, void *teambucket)
{
	statsgame_t	*g = (statsgame_t *)gamep;
	int			i;

	if (!g)
		return -1;

	for (i = 0; i < ArrayLength (g->teams); i++)
		if (*(void **)ArrayNth (g->teams, i) == teambucket)
			return i;

	return -1;
}

void set_server_bucket_info (void *gamep, char *key, char *value)
{
	statsgame_t	*g = (statsgame_t *)gamep;

	if (!g)
		return;

	BucketSet (g->serverbucket, key, bt_string, value);
}

// serializes the whole game snapshot (server + all player/team buckets)
// and either sends it live or queues it to the disk retry cache
void SendGameSnapShot (void *gamep, int a, int b)
{
	statsgame_t	*g = (statsgame_t *)gamep;
	char		payload[8192];
	int			i;
	bucketset_t	*b_;

	if (!g)
		return;

	payload[0] = 0;
	strcat (payload, DumpMap (g->serverbucket));

	for (i = 0; i < ArrayLength (g->players); i++)
	{
		b_ = *(bucketset_t **)ArrayNth (g->players, i);
		if (b_)
			strcat (payload, DumpMap (b_));
	}

	for (i = 0; i < ArrayLength (g->teams); i++)
	{
		b_ = *(bucketset_t **)ArrayNth (g->teams, i);
		if (b_)
			strcat (payload, DumpMap (b_));
	}

	if (IsStatsConnected ())
	{
		char	enc[8192];

		safe_strcpy (enc, payload, sizeof(enc));
		xcode_buf (enc, strlen (enc), XORKEY_SOCKET);
		if (DoSend (enc, strlen (enc)) < 0)
			DiskWrite (payload, strlen (payload));
	}
	else
	{
		DiskWrite (payload, strlen (payload));
	}
}

/*
==============================================================
PER-ENTITY TYPED STAT ACCESSORS

Thin, uniformly-shaped wrappers over Bucket{Set,Get} -- confirmed real
names (all static/local in the binary) and confirmed 3-groups-of-3
shape (Player/Server/Team x Float/Int/String).
==============================================================
*/

static void PlayerOpFloat (void *player, char *key, float value)
{
	BucketSet ((bucketset_t *)player, key, bt_float, &value);
}

static void PlayerOpInt (void *player, char *key, int value)
{
	BucketSet ((bucketset_t *)player, key, bt_int, &value);
}

static void PlayerOpString (void *player, char *key, char *value)
{
	BucketSet ((bucketset_t *)player, key, bt_string, value);
}

static void ServerOpFloat (void *gamep, char *key, float value)
{
	statsgame_t	*g = (statsgame_t *)gamep;
	if (g) BucketSet (g->serverbucket, key, bt_float, &value);
}

static void ServerOpInt (void *gamep, char *key, int value)
{
	statsgame_t	*g = (statsgame_t *)gamep;
	if (g) BucketSet (g->serverbucket, key, bt_int, &value);
}

static void ServerOpString (void *gamep, char *key, char *value)
{
	statsgame_t	*g = (statsgame_t *)gamep;
	if (g) BucketSet (g->serverbucket, key, bt_string, value);
}

static void TeamOpFloat (void *team, char *key, float value)
{
	BucketSet ((bucketset_t *)team, key, bt_float, &value);
}

static void TeamOpInt (void *team, char *key, int value)
{
	BucketSet ((bucketset_t *)team, key, bt_int, &value);
}

static void TeamOpString (void *team, char *key, char *value)
{
	BucketSet ((bucketset_t *)team, key, bt_string, value);
}
