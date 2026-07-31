#include "g_local.h"

#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>


extern cvar_t	*logfile;
extern cvar_t	*netlog;

FILE		*StdLogFile;
static fd_set	global_fds;


/*
=================
net_name_to_address
=================
*/
struct sockaddr_in *net_name_to_address (struct sockaddr_in *addr, char *name)
{
	struct sockaddr_in	sin;
	struct hostent		*hp;
	char			*s, *portstr;
	unsigned long		a;
	unsigned short		port;

	memset (&sin, 0, sizeof(sin));

	s = strdup (name);
	strtok (s, ":");
	portstr = strtok (NULL, "");

	if (portstr)
	{
		port = (unsigned short)strtol (portstr, NULL, 10);
		if (!port)
		{
			fprintf (stderr, "net_name_to_address: %s: invalid port number\n", portstr);
			exit (1);
		}
	}
	else
		port = 0;

	a = inet_addr (s);
	if (a == INADDR_NONE)
	{
		if (!(hp = gethostbyname (s)))
		{
			fprintf (stderr, "%s: %s\n", "net_name_to_addr", s);
			exit (1);
		}
		sin.sin_addr.s_addr = *(unsigned long *)hp->h_addr_list[0];
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons (port);
	free (s);

	*addr = sin;
	return addr;
}

/*
=================
net_send
=================
*/
void net_send (int sock, char *buf, int len)
{
	int	r;

	r = send (sock, buf, len, 0);
	if (r != len)
	{
		perror ("send");
		if (errno)
			exit (1);
	}
}

/*
=================
net_open_socket
=================
*/
int net_open_socket (void)
{
	int	sock;

	sock = socket (AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		perror ("socket");
		exit (1);
	}

	FD_SET (sock, &global_fds);

	return sock;
}

/*
=================
net_close_socket
=================
*/
void net_close_socket (int sock)
{
	if (sock)
	{
		if (close (sock) < 0)
		{
			perror ("close");
			exit (1);
		}
	}

	FD_CLR (sock, &global_fds);
}

/*
=================
net_connect_socket
=================
*/
void net_connect_socket (int sock, struct sockaddr_in *addr, unsigned short port)
{
	addr->sin_port = htons (port);

	if (connect (sock, (struct sockaddr *)addr, sizeof(*addr)) < 0)
	{
		perror ("connect");
		exit (1);
	}
}

/*
=================
GSSendLine
=================
*/
void GSSendLine (char *line)
{
	struct sockaddr_in	addr;
	int			sock;
	unsigned short		port;

	net_name_to_address (&addr, netlog->string);
	port = ntohs (addr.sin_port);

	sock = net_open_socket ();
	net_connect_socket (sock, &addr, port);
	net_send (sock, line, strlen (line));
	net_close_socket (sock);
}

/*
=================
GSOpenLog
=================
*/
void GSOpenLog (void)
{
	cvar_t	*gamedir, *logname;
	char	path[MAX_OSPATH];

	gamedir = gi.cvar ("game", ".", CVAR_LATCH);
	logname = gi.cvar ("logname", "stdlog.log", 0);

	strcpy (path, gamedir->string);
	strcat (path, "/");
	strcat (path, logname->string);

	StdLogFile = fopen (path, "a+t");
}

/*
=================
GSCloseLog
=================
*/
void GSCloseLog (void)
{
	fclose (StdLogFile);
}

/*
=================
GSLogShutdown
=================
*/
void GSLogShutdown (void)
{
	cvar_t	*gamedir, *logname;
	char	path[MAX_OSPATH];

	if (logfile->value != 2)
		return;

	gamedir = gi.cvar ("game", ".", CVAR_LATCH);
	logname = gi.cvar ("logname", "stdlog.log", 0);

	strcpy (path, gamedir->string);
	strcat (path, "/");
	strcat (path, logname->string);

	StdLogFile = fopen (path, "a+t");

	fprintf (StdLogFile, "\t\tGameEnd\t\t\t%d\n", (int)level.time);

	fclose (StdLogFile);
}

/*
=================
GSLogStartup
=================
*/
void GSLogStartup (void)
{
	cvar_t	*gamedir, *logname;
	char	path[MAX_OSPATH];

	if (logfile->value != 2)
		return;

	gamedir = gi.cvar ("game", ".", CVAR_LATCH);
	logname = gi.cvar ("logname", "stdlog.log", 0);

	strcpy (path, gamedir->string);
	strcat (path, "/");
	strcat (path, logname->string);

	StdLogFile = fopen (path, "a+t");

	fprintf (StdLogFile, "\t\tStdLog\t1.22\n");
	fprintf (StdLogFile, "\t\tPatchName\tRocket Arena 2 %s\n", "v2.25");

	fclose (StdLogFile);
}

/*
=================
GSLogNewmap
=================
*/
void GSLogNewmap (void)
{
	cvar_t	*gamedir, *logname;
	char	path[MAX_OSPATH];

	if (logfile->value != 2)
		return;

	gamedir = gi.cvar ("game", ".", CVAR_LATCH);
	logname = gi.cvar ("logname", "stdlog.log", 0);

	strcpy (path, gamedir->string);
	strcat (path, "/");
	strcat (path, logname->string);

	StdLogFile = fopen (path, "a+t");

	fprintf (StdLogFile, "\t\tMAP\t%s\n", level.level_name);
	fprintf (StdLogFile, "\t\tGameStart\t\t\t%d\n", (int)level.time);

	fclose (StdLogFile);
}

/*
=================
GSdodeathlog
=================
*/
void GSdodeathlog (char *line)
{
	fprintf (StdLogFile, line);

	if (netlog->string[0])
		GSSendLine (line);
}

/*
=================
GSLogDeath
=================
*/
void GSLogDeath (edict_t *self, edict_t *inflictor, edict_t *attacker)
{
	cvar_t	*gamedir, *logname;
	char	path[MAX_OSPATH];
	char	line[1000];
	gitem_t	*weap;
	char	*weapname;

	if (logfile->value != 2)
		return;

	gamedir = gi.cvar ("game", ".", CVAR_LATCH);
	logname = gi.cvar ("logname", "stdlog.log", 0);

	strcpy (path, gamedir->string);
	strcat (path, "/");
	strcat (path, logname->string);

	StdLogFile = fopen (path, "a+t");

	if (attacker == self)
	{
		weap = self->client->pers.weapon;

		if (weap &&
		    (!strcmp (weap->classname, "weapon_grenadelauncher") ||
		     !strcmp (weap->classname, "weapon_rocketlauncher") ||
		     !strcmp (weap->classname, "weapon_bfg")))
		{
			Com_sprintf (line, sizeof(line), "%s\t\tSuicide\t%s\t-1\t%d\t%d\n",
				self->client->pers.netname, weap->pickup_name,
				(int)level.time, self->client->ping);
		}
		else
		{
			Com_sprintf (line, sizeof(line), "%s\t\tSuicide\t\t-1\t%d\t%d\n",
				self->client->pers.netname, (int)level.time, self->client->ping);
		}
	}
	else if (attacker && attacker->client)
	{
		weap = attacker->client->pers.weapon;
		weapname = weap ? weap->pickup_name : "BFG10K";

		Com_sprintf (line, sizeof(line), "%s\t%s\tKill\t%s\t1\t%d\t%d\n",
			attacker->client->pers.netname, self->client->pers.netname,
			weapname, (int)level.time, attacker->client->ping);
	}
	else
	{
		Com_sprintf (line, sizeof(line), "%s\t\tSuicide\t\t-1\t%d\t%d\n",
			self->client->pers.netname, (int)level.time, self->client->ping);
	}

	GSdodeathlog (line);

	fclose (StdLogFile);
}

/*
=================
GSLogEnter
=================
*/
void GSLogEnter (edict_t *ent)
{
	cvar_t	*gamedir, *logname;
	char	path[MAX_OSPATH];

	if (logfile->value != 2)
		return;

	gamedir = gi.cvar ("game", ".", CVAR_LATCH);
	logname = gi.cvar ("logname", "stdlog.log", 0);

	strcpy (path, gamedir->string);
	strcat (path, "/");
	strcat (path, logname->string);

	StdLogFile = fopen (path, "a+t");

	fprintf (StdLogFile, "\t\tPlayerConnect\t%s\t\t%d\n",
		ent->client->pers.netname, (int)level.time);

	fclose (StdLogFile);
}

/*
=================
GSLogExit
=================
*/
void GSLogExit (edict_t *ent)
{
	cvar_t	*gamedir, *logname;
	char	path[MAX_OSPATH];

	if (logfile->value != 2)
		return;

	gamedir = gi.cvar ("game", ".", CVAR_LATCH);
	logname = gi.cvar ("logname", "stdlog.log", 0);

	strcpy (path, gamedir->string);
	strcat (path, "/");
	strcat (path, logname->string);

	StdLogFile = fopen (path, "a+t");

	fprintf (StdLogFile, "\t\tPlayerLeft\t%s\t\t%d\n",
		ent->client->pers.netname, (int)level.time);

	fclose (StdLogFile);
}
