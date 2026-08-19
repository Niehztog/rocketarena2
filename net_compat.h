#ifdef _WIN32

#include <winsock.h>

#define close(s)    closesocket(s)

#else

#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define WSAGetLastError()   errno

#endif
