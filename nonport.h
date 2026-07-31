#ifndef __NONPORT_H__
#define __NONPORT_H__

unsigned long current_time (void);
void msleep (unsigned long ms);
void SocketStartUp (void);
void SocketShutDown (void);

#ifndef _WIN32
char *_strdup (const char *src);
#else
#define strcasecmp	_stricmp
#define strncasecmp	_strnicmp
#endif

#endif
