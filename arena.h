// arena.h -- Rocket Arena 2 arena/team/round system

//
// tunable limits
//
#define	MAX_ARENAS			32
#define	MAX_TEAMS			256
#define	MAX_ARENA_SKINS		7		// skin/color numbers 0-6 handed out per arena

//
// arena_t.weapons bitmask -- which weapons are enabled in an arena
//
#define	RW_SHOTGUN				1
#define	RW_SUPERSHOTGUN			2
#define	RW_MACHINEGUN			4
#define	RW_CHAINGUN				8
#define	RW_GRENADELAUNCHER		16
#define	RW_ROCKETLAUNCHER		32
#define	RW_HYPERBLASTER			64
#define	RW_RAILGUN				128
#define	RW_BFG					256

//
// arena_t.state -- per-arena round state machine, driven by arena_think()
//
#define	ASTATE_WARMUP			0	// waiting for enough players/teams to fill the arena
#define	ASTATE_COUNTDOWN		1	// "fight!" countdown running, show_countdown() ticking
#define	ASTATE_FIGHTING			2	// round in progress
#define	ASTATE_ROUNDEND			3	// checking whether the round is over
#define	ASTATE_INTERMISSION		4	// short pause between rounds
#define	ASTATE_RESULTS			5	// showing round results
#define	ASTATE_NEXTROUND		6	// pulling in the next round's teams

//
// gclient_t->fightstate -- is this client alive and fighting this round?
//
#define	FIGHT_SPECTATING		0
#define	FIGHT_ALIVE				1
#define	FIGHT_DEAD				2

//
// gclient_t->omode -- observer/spectator camera mode, cycled by ChangeOMode()
//
#define	OMODE_NORMAL			0	// actually playing, not observing
#define	OMODE_FREEFLY			1	// noclipping around with no fixed target
#define	OMODE_TRACKCAM			2	// following a tracked player, vanilla-chasecam style
#define	OMODE_INEYES			3	// floating in the eyes of a tracked player

//
// means of death additions
//
#define	MOD_GRAPPLE				34

//
// player_state_t.stats[] slot used by CTFSetIDView to show a tracked
// player's name/health overlay to whoever is observing them
//
#define	STAT_CTF_ID_VIEW		27

//
// grapple hook state, ported from id's CTF grapple (see g_ctf.c upstream)
//
typedef enum
{
	CTF_GRAPPLE_STATE_FLY,
	CTF_GRAPPLE_STATE_PULL,
	CTF_GRAPPLE_STATE_HANG
} ctfgrapplestate_t;

#define	CTF_GRAPPLE_SPEED		650	// speed of grapple in flight
#define	CTF_GRAPPLE_PULL_SPEED	650	// speed player is pulled at

//
// message-of-the-day: one queued node per line of motd.txt, walked via
// add_to_queue()'s generic sentinel-head convention (see load_motd(),
// maploop.c). "motd" itself is the sentinel head node, not a line of text.
//
typedef struct motd_s
{
	char			*line;
	struct motd_s	*next;
	struct motd_s	*prev;
} motd_t;

//
// a team of players sharing spawn points, a skin/color and a fate this round
//
typedef struct team_s
{
	struct team_s	*next;			// arena's list of currently-assigned teams
	struct team_s	*prev;

	char		*name;
	int			teamnum;			// index into teams[]
	int			arenanum;			// which arena this team is assigned to, if any
	int			side;				// 0/1, which half of the arena's spawn points to use
	int			skin;				// 0..MAX_ARENA_SKINS-1, -1 = unassigned
	qboolean	locked;				// closed to new members
	qboolean	fighting;			// true once this team is in an active round

	gclient_t	*members;			// head of the member list (client->arena_next/arena_prev)
	int			nummembers;

	gclient_t	*queue;				// pickup mode: players waiting in line for THIS team specifically
} team_t;

//
// a staged copy of the votable arena_t settings, used by the propose/vote
// flow (menuShowSettingsPropose/Vote, Cmd_arenaadmin_f) to hold pending
// changes separately from the live settings until the vote passes
//
typedef struct
{
	int			playersperteam;
	int			rounds;
	int			weapons;
	int			armor;
	int			health;
	int			minping, maxping;
	int			armorprotect;
	int			healthprotect;
	qboolean	fallingdamage;
	qboolean	locked;
	qboolean	competition;
	qboolean	scorebydamage;
	qboolean	pending;			// true while a proposal is awaiting a vote
} arena_settings_t;

//
// one arena: a self-contained little deathmatch running its own round loop
//
typedef struct arena_s
{
	char		*name;				// display name, e.g. "Arena 1" / getarenaname()

	int			numteams;			// teams required before fill_arena() can start a round
	int			maxteams;			// teams currently allowed to sign up
	team_t		*teamlist;			// teams currently queued/assigned to this arena
	int			teamnum[8];			// which teams[] index currently occupies each team slot

	int			state;				// ASTATE_*
	float		statetime;			// level.time the current state was entered
	int			round;				// current round number, 1-based
	int			rounds;				// rounds per match

	int			playersperteam;
	int			weapons;			// RW_* bitmask
	qboolean	weaponlock[9];		// admin-forced-off weapons, indexed like weapon_vals[]
	int			armor;
	int			health;
	int			shells, bullets, slugs, grenades, rockets, cells;
	int			minping, maxping;
	qboolean	pickup;				// allow picking up dropped weapons/ammo/armor
	int			rocket_speed;
	qboolean	fastswitch;
	int			armorprotect;		// 0 damage all, 1 dont damage team, 2 damage self not team
	int			healthprotect;		// 0 damage all, 1 dont damage team, 2 damage self not team
	qboolean	fallingdamage;
	qboolean	competition;
	qboolean	scorebydamage;		// "Damage Scoring:" -- 1pt per 100 damage dealt, if set
	qboolean	locked;				// closed to new teams/voting
	qboolean	idarena;			// no dedicated spawn points -- share the map's own
	qboolean	active;				// a round is actually in progress
	qboolean	teamplay;			// separate spawn sides / team based scoring matters here

	// per-setting voting toggles -- can this be proposed/voted on for this arena?
	qboolean	allow_voting_armor;
	qboolean	allow_voting_health;
	qboolean	allow_voting_minping;
	qboolean	allow_voting_maxping;
	qboolean	allow_voting_playersperteam;
	qboolean	allow_voting_rounds;
	qboolean	allow_voting_maxteams;
	qboolean	allow_voting_armorprotect;
	qboolean	allow_voting_healthprotect;
	qboolean	allow_voting_shotgun;
	qboolean	allow_voting_supershotgun;
	qboolean	allow_voting_machinegun;
	qboolean	allow_voting_chaingun;
	qboolean	allow_voting_grenadelauncher;
	qboolean	allow_voting_rocketlauncher;
	qboolean	allow_voting_hyperblaster;
	qboolean	allow_voting_railgun;
	qboolean	allow_voting_bfg;
	qboolean	allow_voting_fallingdamage;

	int			sidepick;			// this round's coin flip: which team spawns on which side

	gclient_t	*waitqueue;			// players waiting for a slot to open up in this arena

	arena_settings_t	proposed;	// staged settings awaiting a vote (see arena_settings_t)
	float		proposetime;		// level.time a settings-change proposal expires, 0 = none
	edict_t		*proposer;

	int			votes_yes, votes_no;
	int			votetries;

	void		*statsptr;			// GameSpy stats-subsystem bucket handle for this arena

	char		vs[64];				// "Red Team vs Blue Team" style fight description

	team_t		*pickupteam[2];		// auto-created "Pickup Red"/"Pickup Blue" teams (idarena)
} arena_t;

//
// globals owned by arena.c
//
extern	int			votetries_setting;
extern	qboolean	allow_grapple;
extern	qboolean	broken;

extern	arena_t		arenas[MAX_ARENAS];
extern	int			num_arenas;
extern	qboolean	idmap;			// this map has no per-arena spawn point tagging

extern	team_t		*teams[MAX_TEAMS];

extern	motd_t		motd;
extern	cvar_t		*admincode;

extern	char		*teamskins[MAX_ARENA_SKINS];	// color name per skin/color number, e.g. "red"
extern	char		*vwepmodels[4];
extern	qboolean	teamskins_precachem[MAX_ARENA_SKINS];
extern	qboolean	teamskins_precachef[MAX_ARENA_SKINS];
extern	qboolean	teamskins_precachecw[MAX_ARENA_SKINS];
extern	qboolean	teamskins_precachecb[MAX_ARENA_SKINS];

extern	char		*omode_descriptions[4];

extern	char		*dm_statusbar;

extern	int			weapon_vals[9];		// bit value per weapon slot, indexed 0=shotgun..8=bfg (maploop.c)

//
// the join queue -- a simple intrusive doubly linked list of clients,
// reused both for "players waiting for an arena slot" and "players on
// a team roster" (client->arena_next / client->arena_prev)
//
void		add_to_queue (gclient_t *cl, gclient_t **head);
gclient_t	*remove_from_queue (gclient_t *cl, gclient_t **head);
void		add_to_front_queue (gclient_t *cl, gclient_t **head);
int			count_queue (gclient_t *head);
int			count_players_queue (gclient_t *head);

void		set_damage (int arenanum, int state);
void		give_ammo (edict_t *ent);

team_t		*add_to_team (edict_t *ent, char *teamname);
void		remove_from_team (edict_t *ent);

edict_t		*SelectRandomArenaSpawnPoint (char *classname, int arenanum, int side);
edict_t		*SelectFarthestArenaSpawnPoint (char *classname, int arenanum);

void		track_SetStats (edict_t *ent);
void		eyecam_think (edict_t *ent);
void		track_think (edict_t *ent);
void		track_change (edict_t *ent, int dir);
void		track_next (edict_t *ent);
void		track_prev (edict_t *ent);
void		SetObserverMode (edict_t *ent);

void		move_to_arena (edict_t *ent, int arenanum, int mode);
void		ChangeOMode (edict_t *ent);

int			getfreeskin (int arenanum);
char		*mylcase (char *s);
qboolean	checkvwepmodel (char *s);
void		setteamskin (edict_t *ent, char *userinfo, int skinnum);

void		SendTeamToArena (team_t *team, int arenanum, qboolean fighting, qboolean announce);
void		AddtoArena (edict_t *ent, int arenanum, int teamnum);
void		check_teams (int arenanum);

void		init_player (edict_t *ent);
void		reinit_player (edict_t *ent);

void		show_stringc (edict_t *ent, char *s);
void		show_string (edict_t *ent, char *s);
void		stuffcmd (edict_t *ent, char *s);
void		send_sound_to_arena (int arenanum, int soundindex);
void		send_configstring (int num);
void		show_countdown (int arenanum);
void		show_rank (edict_t *ent);

qboolean	check_for_teams (int arenanum);
void		fill_arena (int arenanum);
int			fight_done (int arenanum);

void		CTFSetIDView (edict_t *ent);
void		UpdateStatusBars (int arenanum);
void		check_telefrag (int arenanum);

void		start_voting (int arenanum);
void		check_voting (int arenanum);

void		arena_think (int arenanum);
void		multi_arena_think (void);
void		arena_init (edict_t *ent);

void		SP_trigger_teleport (edict_t *ent);
void		SP_func_illusionary (edict_t *ent);
void		SP_info_teleport_destination (edict_t *ent);

//
// ra2menus.c
//
char		*getarenaname (int arenanum);
void		menu_centerprint (edict_t *ent, char *message);

//
// the offhand grapple hook, ported near-verbatim from id's CTF g_ctf.c
//
void		CTFPlayerResetGrapple (edict_t *ent);
void		CTFResetGrapple (edict_t *self);
void		CTFGrappleTouch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf);
void		CTFGrappleDrawCable (edict_t *self);
void		CTFGrapplePull (edict_t *self);
void		CTFFireGrapple (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int effect);
void		CTFGrappleFire (edict_t *ent, vec3_t g_offset, int damage, int effect);
void		CTFWeapon_Grapple_Fire (edict_t *ent);
void		CTFWeapon_Grapple (edict_t *ent);

//
// gslog.c -- the local StdLog text logger + netlog forwarding
//
void		GSLogStartup (void);
void		GSLogShutdown (void);
void		GSLogNewmap (void);
void		GSLogEnter (edict_t *ent);
void		GSLogExit (edict_t *ent);
void		GSLogDeath (edict_t *self, edict_t *inflictor, edict_t *attacker);

//
// the GameSpy stats-server pipeline -- a whole separate subsystem (bucket/
// hashtable backed, see gbucket.c/hashtable.c/darray.c/md5.c/stats.c)
// that shipped only in the x86/Alpha builds, added after this codebase's
// v2.22 baseline. NewStatsPlayer/ValidatePlayer are implemented in
// arena.c itself (their real addresses cluster with arena.c's own
// functions, not the rest of this subsystem); everything else lives in
// stats.c.
//
void		*NewGame (void);
void		*NewPlayer (void *gamep);
void		*NewTeam (void *gamep);
void		NewStatsPlayer (void *arenastats, edict_t *ent, int flags);
qboolean	ValidatePlayer (void *arenastats, int clientnum);
void		RemovePlayer (void *arenastats, int clientnum);
void		RemoveTeam (void *gamep, int teamnum);
int			GetPlayerIndex (void *gamep, void *playerbucket);
int			GetTeamIndex (void *gamep, void *teambucket);
void		set_server_bucket_info (void *gamep, char *key, char *value);
void		SendGameSnapShot (void *game, int a, int b);
void		FreeGame (void *game);
int			InitStatsConnection (void);
void		CloseStatsConnection (void);
qboolean	IsStatsConnected (void);
char		*GetChallenge (void);
char		*GenerateAuth (char *cdkey, char *challenge);
