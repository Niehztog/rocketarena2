#define MAX_ARENAS          32
#define MAX_TEAMS           256
#define MAX_ARENA_SKINS     7

#define ASTATE_WARMUP           0
#define ASTATE_COUNTDOWN        1
#define ASTATE_FIGHTING         2
#define ASTATE_ROUNDEND         3
#define ASTATE_INTERMISSION     4
#define ASTATE_RESULTS          5
#define ASTATE_NEXTROUND        6

#define FIGHT_SPECTATING        0
#define FIGHT_ALIVE             1
#define FIGHT_DEAD              2

#define MOD_GRAPPLE             34

#define STAT_LINEPOSITION       19

#define STAT_CTF_ID_VIEW        20

#define MAX_STATUS_TEAMS        2
#define MAX_STATUS_MEMBERS      4

typedef enum {
    CTF_GRAPPLE_STATE_FLY,
    CTF_GRAPPLE_STATE_PULL,
    CTF_GRAPPLE_STATE_HANG
} ctfgrapplestate_t;

#define CTF_GRAPPLE_SPEED       650 // speed of grapple in flight
#define CTF_GRAPPLE_PULL_SPEED  650 // speed player is pulled at

typedef struct motd_s {
    char            *line;
    struct motd_s   *next;
    struct motd_s   *prev;
} motd_t;

typedef struct team_s {
    char        *name;
    int         teamnum;
    int         arenanum;
    int         wins;

    qmenu_t     arenalink;

    bool    locked;

    int         side;

    int         skin;

    bool    fighting;

    bool    outofline;
} team_t;

#define TEAM(node)  ((team_t *)(node)->it)

typedef struct arena_settings_s {
    int         playersperteam;
    int         rounds;
    int         weapons;
    int         armor;
    int         health;
    int         minping;
    int         maxping;
    int         rocket_speed;
    int         shells, bullets, slugs, grenades, rockets, cells;

    int         startdelay;

    int         fastswitch;
    int         armorprotect;
    int         healthprotect;
    int     fallingdamage;
    int     allow_voting_armor;
    int     allow_voting_health;
    int     allow_voting_minping;
    int     allow_voting_maxping;
    int     allow_voting_playersperteam;
    int     allow_voting_rounds;
    int     allow_voting_maxteams;
    int     allow_voting_armorprotect;
    int     allow_voting_healthprotect;
    int     allow_voting_shotgun;
    int     allow_voting_supershotgun;
    int     allow_voting_machinegun;
    int     allow_voting_chaingun;
    int     allow_voting_grenadelauncher;
    int     allow_voting_rocketlauncher;
    int     allow_voting_hyperblaster;
    int     allow_voting_railgun;
    int     allow_voting_bfg;
    int     allow_voting_fallingdamage;
    int     locked;
    int     competition;
    int     scorebydamage;
    int     changed;
} arena_settings_t;

typedef struct arena_s {
    int         numteams;
    int         _arena_unidentified0;

    qmenu_t     waitingteams;

    qmenu_t     activeteams;

    int         state;

    bool    teamplay;

    int         countdown_next_tick;

    int         countdown;

    bool    active;

    char        msg[160];

    char        vs[64];

    int         playersperteam;

    int         rounds;

    int         weapons;

    int         armor;
    int         health;

    int         minping, maxping;

    int         rocket_speed;

    int         shells, bullets, slugs, grenades, rockets, cells;

    int         startdelay;

    int         fastswitch;
    int         armorprotect;
    int         healthprotect;
    int     fallingdamage;
    int     allow_voting_armor;
    int     allow_voting_health;
    int     allow_voting_minping;
    int     allow_voting_maxping;
    int     allow_voting_playersperteam;
    int     allow_voting_rounds;
    int     allow_voting_maxteams;
    int     allow_voting_armorprotect;
    int     allow_voting_healthprotect;
    int     allow_voting_shotgun;
    int     allow_voting_supershotgun;
    int     allow_voting_machinegun;
    int     allow_voting_chaingun;
    int     allow_voting_grenadelauncher;
    int     allow_voting_rocketlauncher;
    int     allow_voting_hyperblaster;
    int     allow_voting_railgun;
    int     allow_voting_bfg;
    int     allow_voting_fallingdamage;
    int     locked;
    int     competition;
    int     scorebydamage;
    int     changed;
    float       proposetime;

    arena_settings_t    proposed;

    int         votetries;
    int         votes_yes, votes_no;
    edict_t     *proposer;

    bool    idarena;

    int         sidepick;

    int         maxteams;

    int         round;
    team_t      *pickupteam[2];

    struct ra2_round_s  *stats;     // NULL when statsfile is off
} arena_t;

extern  int         votetries_setting;
extern  bool    allow_grapple;
extern  bool    broken;

extern  arena_t     arenas[MAX_ARENAS];
extern  int         num_arenas;
extern  bool    idmap;

extern  qmenu_t     *teams;

extern  motd_t      motd;
extern  cvar_t      *admincode;

extern  char        *teamskins[MAX_ARENA_SKINS];
extern  char        *vwepmodels[4];
extern  int    teamskins_precachem[MAX_ARENA_SKINS];
extern  int    teamskins_precachef[MAX_ARENA_SKINS];
extern  int    teamskins_precachecw[MAX_ARENA_SKINS];
extern  int    teamskins_precachecb[MAX_ARENA_SKINS];

extern  char        *omode_descriptions[4];

extern const char   dm_statusbar[];

extern  int         weapon_vals[9];

int         count_queue(qmenu_t *head);
int         count_players_queue(qmenu_t *head);

void        set_damage(int arenanum, int state);
void        give_ammo(edict_t *ent);

team_t      *add_to_team(edict_t *ent, char *teamname);
void        remove_from_team(edict_t *ent);

edict_t     *SelectRandomArenaSpawnPoint(char *classn, int arenanum, int side);
edict_t     *SelectFarthestArenaSpawnPoint(char *classn, int arenanum, edict_t *ignore);

void        track_SetStats(edict_t *ent);
void        eyecam_think(edict_t *ent, usercmd_t *ucmd);
void        track_think(edict_t *ent, usercmd_t *ucmd);
void        track_change(edict_t *ent, int dir);
void        track_next(edict_t *ent);
void        track_prev(edict_t *ent);
void        SetObserverMode(edict_t *ent);

void        move_to_arena(edict_t *ent, int arenanum, int mode);
void        ChangeOMode(edict_t *ent);

int         getfreeskin(int arenanum);
void        setteamskin(edict_t *ent, char *userinfo, int skinnum);

void        SendTeamToArena(qmenu_t *team, int arenanum, bool observer, bool announce);
int         AddtoArena(edict_t *ent, int arenanum, int allow_partial, int skip_checks);
void        check_teams(int arenanum);

void        init_player(edict_t *ent);
void        reinit_player(edict_t *ent);

void        show_stringc(char *s, int context);
void        show_string(int priority, char *s, int context);
void        stuffcmd(edict_t *ent, char *s);
void        send_sound_to_arena(char *soundname, int context);
void        send_configstring(edict_t *e, int index, char *string);
void        show_countdown(int countdown, int arenanum);
int     show_rank(qmenu_t *node);

bool    check_for_teams(int arenanum);
int         fill_arena(int arenanum);
int         fight_done(int arenanum);

void        CTFSetIDView(edict_t *ent);
void        UpdateStatusBars(int arenanum);
void        check_telefrag(int arenanum);

void        start_voting(edict_t *proposer, int arenanum);
void        check_voting(int arenanum);

void        arena_think(int arenanum);
void        multi_arena_think(void);
void        arena_init(edict_t *wsent);

void        SP_trigger_teleport(edict_t *ent);
void        SP_func_illusionary(edict_t *ent);
void        SP_info_teleport_destination(edict_t *ent);

char        *getarenaname(int arenanum);
void        menu_centerprint(edict_t *ent, char *message);
int         menuRefreshTeamList(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg);

void        motd_menu(edict_t *ent);

void        CTFPlayerResetGrapple(edict_t *ent);
void        CTFResetGrapple(edict_t *self);
void        CTFGrappleTouch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf);
void        CTFGrappleDrawCable(edict_t *self);
void        CTFGrapplePull(edict_t *self);
void        CTFFireGrapple(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int effect);
void        CTFGrappleFire(edict_t *ent, const vec3_t g_offset, int damage, int effect);
void        CTFWeapon_Grapple_Fire(edict_t *ent);
void        CTFWeapon_Grapple(edict_t *ent);

void        GSLogStartup(void);
void        GSLogShutdown(void);
void        GSLogNewmap(void);
void        GSLogEnter(edict_t *ent);
void        GSLogExit(edict_t *ent);
void        GSLogDeath(edict_t *self, edict_t *inflictor, edict_t *attacker);
#ifdef _WIN32
bool        GSNetStartup(void);
void        GSNetShutdown(void);
#endif

