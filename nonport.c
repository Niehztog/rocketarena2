#include <stdlib.h>
#include <string.h>
#include "net_compat.h"

/* gamex86.dll 0x2001fad0-0x2001fad6 (manual-confirmed) */
/* gamei386.so 0x00055cc0-0x00055d1c */
unsigned long current_time (void)
{
#ifdef _WIN32
    return GetTickCount ();
#else
    struct timeval  tv;

    gettimeofday (&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x00055d1c-0x00055d36 */
void msleep (unsigned long ms)
{
#ifdef _WIN32
    Sleep (ms);
#else
    usleep (ms * 1000);
#endif
}

/* gamex86.dll 0x2001fae0-0x2001fafc (manual-confirmed) */
/* gamei386.so 0x00055d38-0x00055d39 */
void SocketStartUp (void)
{
#ifdef _WIN32
    WSADATA     data;

    WSAStartup (MAKEWORD (1, 1), &data);
#endif
}

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x00055d3c-0x00055d3d */
void SocketShutDown (void)
{
#ifdef _WIN32
    WSACleanup ();
#endif
}

#ifndef _WIN32

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x00055d40-0x00055d6d */
char *_strdup (const char *src)
{
    char    *ret;

    ret = (char *)malloc (strlen (src) + 1);
    strcpy (ret, src);

    return ret;
}

#endif
