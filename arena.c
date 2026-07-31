// arena.c -- Rocket Arena 2 arena/team/round system, the join queue, the offhand
// grapple hook, the multi-arena observer camera, and the player skin system.

#include "g_local.h"
#include "arena.h"
#include "gbucket.h"

extern int	votetries_setting;		// real definition + initializer in maploop.c
qboolean	allow_grapple;
qboolean	broken;

arena_t		arenas[MAX_ARENAS];
int			num_arenas;
qboolean	idmap;

team_t		*teams[MAX_TEAMS];

motd_t		motd;
cvar_t		*admincode;

char		*teamskins[MAX_ARENA_SKINS] =
{
	"red", "blue", "green", "yellow", "cyan", "magenta", "grey"
};

char		*vwepmodels[4] =
{
	"tris.md2", "w_", "weapons/", "v_"
};

qboolean	teamskins_precachem[MAX_ARENA_SKINS];
qboolean	teamskins_precachef[MAX_ARENA_SKINS];
qboolean	teamskins_precachecw[MAX_ARENA_SKINS];
qboolean	teamskins_precachecb[MAX_ARENA_SKINS];

char		*omode_descriptions[4] =
{
	"Normal", "Free Flying", "Trackcam", "In Eyes"
};

// owned by p_hud.c
extern char	*dm_statusbar;

// stock SDK helpers that have no cross-file prototype anywhere else
char		*va (char *format, ...);
float		PlayersRangeFromSpot (edict_t *spot);
void		ClientUserinfoChanged (edict_t *ent, char *userinfo);

// the vanilla teleporter touch function (g_misc.c) is reused directly by
// SP_trigger_teleport below
void		teleporter_touch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf);

// the visible-weapon projection helper normally lives static in p_weapon.c;
// it is used here (and by the grapple below) to find a muzzle point
void		P_ProjectSource (gclient_t *client, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result);
void		Weapon_Generic (edict_t *ent, int FRAME_ACTIVATE_LAST, int FRAME_FIRE_LAST, int FRAME_IDLE_LAST, int FRAME_DEACTIVATE_LAST, int *pause_frames, int *fire_frames, void (*fire)(edict_t *ent));

// same-team damage check -- lives alongside T_Damage in g_combat.c
qboolean	CheckTeamDamage (edict_t *targ, edict_t *attacker);

// physics helper (g_phys.c) needed by the grapple's hang state
void		SV_AddGravity (edict_t *ent);

// arena.cfg parsing (maploop.c)
void		load_config (int numarenas);
void		set_config (int first, int last);
void		load_motd (void);

// the generic menu engine (menu.c) and RA2's own menus built on it (ra2menus.c)
void		show_observer_menu (edict_t *ent);
void		show_arena_menu (edict_t *ent);
void		show_teamconfirm_menu (edict_t *ent);

// the GameSpy stats-server pipeline: a whole separate subsystem (bucket/
// hashtable backed) that shipped only in the x86/Alpha builds -- see
// stats.c for the connection/protocol/game-management side of it.
// NewStatsPlayer/ValidatePlayer/set_server_bucket_info specifically are
// implemented here rather than in stats.c because their real addresses
// cluster tightly with arena.c's own functions in gamei386.so, not with
// the rest of the stats subsystem.
void
NewStatsPlayer (void *arenastats, edict_t *ent, int flags)
{
	void	*player;

	if (!arenastats || !ent->client)
		return;

	player = NewPlayer (arenastats);
	if (!player)
		return;

	BucketSet (player, "name", bt_string, ent->client->pers.netname);
	BucketSet (player, "team", bt_int, &ent->client->teamnum);
	BucketSet (player, "flags", bt_int, &flags);
}

qboolean
ValidatePlayer (void *arenastats, int clientnum)
{
	if (!arenastats)
		return false;

	return (clientnum >= 0 && clientnum < game.maxclients);
}


/*
==============================================================================

	JOIN QUEUE

	A simple intrusive doubly linked list of clients, threaded through
	client->arena_next / client->arena_prev.  The very same list shape is
	used both for "players waiting for a slot in an arena" (arena_t.waitqueue)
	and for "players on a team's roster" (team_t.members) -- a client can only
	ever be in one such list at a time.

==============================================================================
*/

void add_to_queue (gclient_t *cl, gclient_t **head)
{
	gclient_t	*p;

	if (!*head)
	{
		*head = cl;
		cl->arena_next = NULL;
		cl->arena_prev = NULL;
		return;
	}

	for (p = *head; p->arena_next; p = p->arena_next)
		;

	p->arena_next = cl;
	cl->arena_prev = p;
	cl->arena_next = NULL;
}

gclient_t *remove_from_queue (gclient_t *cl, gclient_t **head)
{
	if (!cl)
	{
		cl = *head;
		if (!cl)
			return NULL;
	}

	if (cl->arena_prev)
		cl->arena_prev->arena_next = cl->arena_next;
	else
		*head = cl->arena_next;

	if (cl->arena_next)
		cl->arena_next->arena_prev = cl->arena_prev;

	cl->arena_next = NULL;
	cl->arena_prev = NULL;

	return cl;
}

void add_to_front_queue (gclient_t *cl, gclient_t **head)
{
	if (cl)
		remove_from_queue (cl, head);

	cl->arena_next = *head;
	cl->arena_prev = NULL;

	if (*head)
		(*head)->arena_prev = cl;

	*head = cl;
}

int count_queue (gclient_t *head)
{
	int			count;
	gclient_t	*p;

	count = 0;
	for (p = head; p; p = p->arena_next)
		count++;

	return count;
}

int count_players_queue (gclient_t *head)
{
	int			count;
	gclient_t	*p;

	count = 0;
	for (p = head; p; p = p->arena_next)
		if (p->inarena)
			count++;

	return count;
}


/*
==============================================================================

	PER-ROUND STATE / LOADOUT

==============================================================================
*/

// applies a round-state (spectating/dead/alive) to every currently active
// member of every team assigned to an arena -- gates whether they can
// currently take damage
void set_damage (int arenanum, int state)
{
	team_t		*t;
	gclient_t	*cl;

	for (t = arenas[arenanum].teamlist; t; t = t->next)
		for (cl = t->members; cl; cl = cl->arena_next)
			if (cl->inarena)
				cl->fightstate = state;
}

// resets a player's weapons and ammo to whatever the arena's settings allow
void give_ammo (edict_t *ent)
{
	static int		weaponflags[9] =
	{
		RW_BFG, RW_SHOTGUN, RW_SUPERSHOTGUN, RW_MACHINEGUN, RW_CHAINGUN,
		RW_GRENADELAUNCHER, RW_RAILGUN, RW_HYPERBLASTER, RW_ROCKETLAUNCHER
	};
	static char		*weaponnames[9] =
	{
		"weapon_bfg", "weapon_shotgun", "weapon_supershotgun", "weapon_machinegun",
		"weapon_chaingun", "weapon_grenadelauncher", "weapon_railgun",
		"weapon_hyperblaster", "weapon_rocketlauncher"
	};
	arena_t		*arena;
	gitem_t		*it;
	qboolean	gotweapon;
	int			i;

	arena = &arenas[ent->client->arenanum];

	ent->max_health = arena->health ? arena->health : 100;

	gotweapon = false;
	for (i = 8; i >= 0; i--)
	{
		if (!(arena->weapons & weaponflags[i]))
			continue;

		it = FindItemByClassname (weaponnames[i]);
		if (!it)
			continue;

		ent->client->pers.inventory[ITEM_INDEX(it)] = 1;

		if (!gotweapon)
		{
			ent->client->pers.weapon = it;
			ent->client->pers.selected_item = ITEM_INDEX(it);
			gotweapon = true;
		}
	}

	if (!gotweapon)
	{
		it = FindItemByClassname ("weapon_blaster");
		ent->client->pers.weapon = it;
		ent->client->pers.selected_item = ITEM_INDEX(it);
	}

	it = FindItemByClassname ("ammo_shells");
	if (it)
		ent->client->pers.inventory[ITEM_INDEX(it)] = arena->shells;

	it = FindItemByClassname ("ammo_bullets");
	if (it)
		ent->client->pers.inventory[ITEM_INDEX(it)] = arena->bullets;

	it = FindItemByClassname ("ammo_slugs");
	if (it)
		ent->client->pers.inventory[ITEM_INDEX(it)] = arena->slugs;

	it = FindItemByClassname ("ammo_grenades");
	if (it)
		ent->client->pers.inventory[ITEM_INDEX(it)] = arena->grenades;

	it = FindItemByClassname ("ammo_rockets");
	if (it)
		ent->client->pers.inventory[ITEM_INDEX(it)] = arena->rockets;

	it = FindItemByClassname ("ammo_cells");
	if (it)
		ent->client->pers.inventory[ITEM_INDEX(it)] = arena->cells;

	it = FindItemByClassname ("item_armor_body");
	if (it)
		ent->client->pers.inventory[ITEM_INDEX(it)] = arena->armor;

	if (allow_grapple)
	{
		it = FindItem ("Grapple");
		if (it)
			ent->client->pers.inventory[ITEM_INDEX(it)] = 1;
	}
}


/*
==============================================================================

	TEAMS

==============================================================================
*/

team_t *add_to_team (edict_t *ent, char *teamname)
{
	int		i;
	team_t	*t;

	for (i = 0; i < MAX_TEAMS; i++)
	{
		t = teams[i];
		if (!t)
			continue;
		if (strcmp (t->name, teamname))
			continue;

		// found an existing team by that name -- try to join it
		if (t->arenanum)
		{
			if ((arenas[t->arenanum].playersperteam &&
				 t->nummembers >= arenas[t->arenanum].playersperteam) ||
				arenas[t->arenanum].locked)
				return NULL;

			if (t->fighting)
				NewStatsPlayer (arenas[t->arenanum].statsptr, ent, 0);
		}

		if (ent)
		{
			add_to_queue (ent->client, &t->members);
			t->nummembers++;
			ent->client->teamnum = i;

			if (t->skin != -1)
				setteamskin (ent, ent->client->pers.userinfo, t->skin);

			gi.bprintf (PRINT_MEDIUM, "%s has been added to team %d (%s)\n",
				ent->client->pers.netname, i, teamname);
		}

		return t;
	}

	// no team with that name yet -- create one
	for (i = 0; i < MAX_TEAMS; i++)
		if (!teams[i])
			break;

	if (i == MAX_TEAMS)
	{
		gi.error ("team malloc failed!\n");
		return NULL;
	}

	t = gi.TagMalloc (sizeof (team_t), TAG_LEVEL);
	if (!t)
	{
		gi.dprintf ("Ateam malloc failed!\n");
		return NULL;
	}

	memset (t, 0, sizeof (*t));
	t->name = teamname;
	t->teamnum = i;
	t->skin = -1;
	t->side = -1;
	teams[i] = t;

	if (!ent)
	{
		t->locked = true;
		return t;
	}

	add_to_queue (ent->client, &t->members);
	t->nummembers++;
	ent->client->teamnum = i;

	gi.bprintf (PRINT_MEDIUM, "%s has created team number %d (%s)\n",
		ent->client->pers.netname, i, teamname);

	return t;
}

void remove_from_team (edict_t *ent)
{
	int			teamnum;
	team_t		*t;

	teamnum = ent->client->teamnum;
	if (teamnum < 0)
		return;

	t = teams[teamnum];
	if (!t || !t->name)
	{
		gi.dprintf ("ERROR in remove_from_team -- please e-mail crt\n");
		return;
	}

	gi.bprintf (PRINT_MEDIUM, "%s has been removed from team %d (%s)\n",
		ent->client->pers.netname, teamnum, t->name);

	if (t->fighting && arenas[ent->client->arenanum].statsptr)
		RemovePlayer (arenas[ent->client->arenanum].statsptr, ent - g_edicts);

	remove_from_queue (ent->client, &t->members);
	t->nummembers--;

	check_teams (ent->client->arenanum);

	ent->client->teamnum = -1;
}


/*
==============================================================================

	ARENA SPAWN POINTS

	info_player_deathmatch (and friends) are shared by every arena on the
	map; a spawn point's "style" field is stamped with the arena number it
	belongs to, unless idmap is set, in which case every spawn point works
	for every arena.

==============================================================================
*/

edict_t *SelectRandomArenaSpawnPoint (char *classname, int arenanum, int side)
{
	edict_t		*spot;
	int			count;
	int			selection;

	count = 0;
	spot = NULL;
	while ((spot = G_Find (spot, FOFS(classname), classname)) != NULL)
		if (spot->style == arenanum || idmap)
			count++;

	if (!count)
		return NULL;

	selection = rand () % count;

	if (side)
	{
		selection &= ~1;
		if (side == 1)
		{
			selection++;
			if (selection >= count)
				selection = 1;
		}
	}

	spot = NULL;
	do
	{
		spot = G_Find (spot, FOFS(classname), classname);
		if (spot->style == arenanum || idmap)
			selection--;
	} while (selection >= 0);

	return spot;
}

edict_t *SelectFarthestArenaSpawnPoint (char *classname, int arenanum)
{
	edict_t		*spot, *bestspot;
	float		bestdistance, dist;
	int			count;
	int			selection;

	bestspot = NULL;
	bestdistance = 50;
	spot = NULL;
	while ((spot = G_Find (spot, FOFS(classname), classname)) != NULL)
	{
		if (!(spot->style == arenanum || idmap))
			continue;

		dist = PlayersRangeFromSpot (spot);
		if (dist > bestdistance)
		{
			bestspot = spot;
			bestdistance = dist;
		}
	}

	if (bestspot)
		return bestspot;

	// nobody is safely far away from anybody -- just grab a random valid spot
	count = 0;
	spot = NULL;
	while ((spot = G_Find (spot, FOFS(classname), classname)) != NULL)
		if (spot->style == arenanum || idmap)
			count++;

	if (!count)
		return NULL;

	selection = rand () % count;

	spot = NULL;
	do
	{
		spot = G_Find (spot, FOFS(classname), classname);
		if (spot->style == arenanum || idmap)
			selection--;
	} while (selection >= 0);

	return spot;
}


/*
==============================================================================

	OBSERVER CAMERA

	Three flavours of spectating: OMODE_FREEFLY (plain noclip, no target),
	OMODE_TRACKCAM (a vanilla-style chase cam locked behind a tracked
	player) and OMODE_INEYES (a floating camera hovering just off a tracked
	player's shoulder).  track_next/track_prev/track_change pick who is
	being watched; track_think/eyecam_think update the camera every frame.

==============================================================================
*/

void track_SetStats (edict_t *ent)
{
	edict_t	*target;

	target = ent->client->track_target;

	memcpy (ent->client->ps.stats, target->client->ps.stats,
		sizeof (ent->client->ps.stats));

	CTFSetIDView (ent);
}

void eyecam_think (edict_t *ent)
{
	edict_t		*target;
	vec3_t		forward;
	vec3_t		dest;
	int			i;

	target = ent->client->track_target;
	if (!target || target->client->fightstate != FIGHT_ALIVE)
	{
		track_next (ent);
		return;
	}

	AngleVectors (target->client->v_angle, forward, NULL, NULL);
	VectorScale (forward, 20, forward);

	VectorAdd (target->s.origin, forward, dest);
	dest[2] += 16;

	VectorCopy (dest, ent->s.origin);
	VectorCopy (dest, ent->s.old_origin);

	VectorClear (ent->velocity);

	VectorCopy (target->client->v_angle, ent->client->v_angle);
	VectorCopy (target->client->v_angle, ent->client->cam_angle);

	for (i = 0; i < 3; i++)
		ent->client->ps.pmove.delta_angles[i] =
			ANGLE2SHORT (target->client->v_angle[i] - ent->s.angles[i]);

	gi.linkentity (ent);

	track_SetStats (ent);
}

void track_think (edict_t *ent)
{
	edict_t		*target;
	vec3_t		mins, maxs;
	vec3_t		forward;
	vec3_t		o, goal;
	trace_t		tr;
	int			i;

	target = ent->client->track_target;
	if (!target || target->client->fightstate != FIGHT_ALIVE)
	{
		track_next (ent);
		return;
	}

	VectorSet (mins, -16, -16, -24);
	VectorSet (maxs, 16, 16, 32);

	AngleVectors (target->client->v_angle, forward, NULL, NULL);
	VectorNormalize (forward);
	VectorMA (target->s.origin, -30, forward, o);

	gi.unlinkentity (ent);
	tr = gi.trace (target->s.origin, mins, maxs, o, target, MASK_SOLID);
	gi.linkentity (ent);

	VectorCopy (tr.endpos, goal);

	VectorCopy (goal, ent->s.origin);
	VectorCopy (goal, ent->s.old_origin);

	VectorCopy (target->client->v_angle, ent->client->v_angle);
	VectorClear (ent->s.angles);

	for (i = 0; i < 3; i++)
		ent->client->ps.pmove.delta_angles[i] =
			ANGLE2SHORT (target->client->v_angle[i] - ent->s.angles[i]);

	gi.linkentity (ent);

	track_SetStats (ent);
}

void track_change (edict_t *ent, int dir)
{
	edict_t		*target;
	edict_t		*best;
	int			i, start;
	qboolean	wrapped;

	target = ent->client->track_target;
	if (!target)
		target = &g_edicts[1];

	start = target - g_edicts;
	i = start;
	wrapped = false;
	best = NULL;

	do
	{
		i += dir;
		if (i > (int) maxclients->value)
			i = 1;
		else if (i < 1)
			i = (int) maxclients->value;

		if (i == start)
		{
			wrapped = true;
			break;
		}

		target = &g_edicts[i];
		if (!target->inuse || !target->client)
			continue;
		if (target->client->fightstate != FIGHT_ALIVE)
			continue;
		if (target->client->arenanum != ent->client->arenanum &&
			!arenas[target->client->arenanum].idarena)
			continue;
		if (!arenas[target->client->arenanum].teamplay ||
			target->client->teamnum == ent->client->teamnum)
		{
			if (target->solid != SOLID_NOT)
			{
				best = target;
				break;
			}
		}
	} while (!wrapped);

	if (!best)
	{
		if (wrapped)
		{
			ent->client->omode = ent->client->lastomode;
			move_to_arena (ent, ent->client->arenanum, 2);
		}
		return;
	}

	ent->client->track_target = best;

	gi.cprintf (best, PRINT_HIGH, "%s is now viewing you\n", ent->client->pers.netname);
}

void track_next (edict_t *ent)
{
	track_change (ent, 1);
}

void track_prev (edict_t *ent)
{
	track_change (ent, -1);
}

void SetObserverMode (edict_t *ent)
{
	switch (ent->client->omode)
	{
	case OMODE_NORMAL:
		ent->movetype = MOVETYPE_WALK;
		ent->solid = SOLID_BBOX;
		ent->clipmask = MASK_PLAYERSOLID;
		ent->svflags &= ~SVF_NOCLIENT;
		ent->client->track_target = NULL;
		break;

	case OMODE_FREEFLY:
		ent->movetype = MOVETYPE_NOCLIP;
		ent->solid = SOLID_NOT;
		ent->clipmask = 0;
		ent->svflags |= SVF_NOCLIENT;
		ent->client->track_target = NULL;
		break;

	case OMODE_TRACKCAM:
	case OMODE_INEYES:
		ent->movetype = MOVETYPE_NOCLIP;
		ent->solid = SOLID_NOT;
		ent->clipmask = 0;
		ent->svflags |= SVF_NOCLIENT;
		break;
	}

	if (ent->client->omode == OMODE_TRACKCAM || ent->client->omode == OMODE_INEYES)
	{
		VectorClear (ent->velocity);
		VectorCopy (ent->s.angles, ent->client->v_angle);
		VectorCopy (ent->s.angles, ent->client->cam_angle);

		if (!ent->client->track_target ||
			ent->client->track_target->client->fightstate != FIGHT_ALIVE)
			track_change (ent, 1);
	}
}


/*
==============================================================================

	MOVING PLAYERS AROUND ARENAS

==============================================================================
*/

void move_to_arena (edict_t *ent, int arenanum, int mode)
{
	arena_t		*arena;
	edict_t		*spot;
	int			side;
	int			i;

	if (ent->client->zbotscore)
	{
		gi.dprintf ("\n%s IS A ZBOT %d\n", ent->client->pers.netname, ent->client->zbotscore);
		gi.centerprintf (ent, "The server seems to think you\nare a bot\n");
	}

	arena = &arenas[arenanum];

	if (mode != 0)
	{
		spot = SelectFarthestArenaSpawnPoint (
			arena->active ? "info_player_deathmatch" : "misc_teleporter_dest", arenanum);

		if (arenanum != 0)
		{
			if (ent->client->arenanum == 0)
			{
				ent->client->arenanum = arenanum;
				show_observer_menu (ent);
				ent->client->arenanum = arenanum;
				return;
			}
		}
		else
		{
			ent->client->track_target = NULL;
			if (ent->client->teamnum != -1)
				show_arena_menu (ent);

			ent->client->arenanum = arenanum;
			return;
		}

		ent->client->arenanum = arenanum;
		return;
	}

	ent->client->arenanum = arenanum;
	ClientUserinfoChanged (ent, ent->client->pers.userinfo);

	side = -1;
	if (ent->client->teamnum != -1 && arena->teamplay)
	{
		team_t	*t;

		t = teams[ent->client->teamnum];
		side = (t->side == arena->sidepick) ? 1 : 2;
	}

	spot = SelectRandomArenaSpawnPoint ("info_player_deathmatch", arenanum, side);
	if (!spot)
		spot = SelectFarthestArenaSpawnPoint ("info_player_deathmatch", arenanum);
	if (!spot)
	{
		gi.bprintf (PRINT_HIGH, "no dest found\n");
		return;
	}

	gi.unlinkentity (ent);

	VectorCopy (spot->s.origin, ent->s.origin);
	VectorCopy (spot->s.origin, ent->s.old_origin);
	ent->s.origin[2] += 10;

	VectorClear (ent->velocity);

	ent->client->ps.pmove.pm_flags = 0;

	for (i = 0; i < 3; i++)
		ent->client->ps.pmove.delta_angles[i] =
			ANGLE2SHORT (spot->s.angles[i] - ent->client->resp.cmd_angles[i]);

	VectorClear (ent->s.angles);
	VectorClear (ent->client->v_angle);
	VectorClear (ent->client->cam_angle);

	ent->s.event = EV_PLAYER_TELEPORT;

	KillBox (ent);

	if (mode == 0)
	{
		if (arena->active && ent->client->omode == OMODE_NORMAL)
			ent->client->omode = OMODE_FREEFLY;
	}

	if (arena->teamplay && mode != 2)
		ent->client->omode = OMODE_INEYES;
	else if (!arena->active)
		ent->client->omode = OMODE_NORMAL;

	SetObserverMode (ent);
	gi.linkentity (ent);

	if (level.time < arena->proposetime && !ent->client->zbotscore)
	{
		gi.centerprintf (ent, "%s", va ("Settings changes have been proposed by %s",
			arena->proposer ? arena->proposer->client->pers.netname : "someone"));
		stuffcmd (ent, "play misc/pc_up.wav\n");
	}
}

void ChangeOMode (edict_t *ent)
{
	if (!ent->client->fightstate)
	{
		if (ent->client->omode != OMODE_TRACKCAM && ent->client->omode != OMODE_INEYES)
			ent->client->lastomode = ent->client->omode;

		ent->client->omode = (ent->client->omode + 1) & 3;

		gi.cprintf (ent, PRINT_HIGH, "Switched Observer Mode to: %s\n",
			omode_descriptions[ent->client->omode]);
	}

	move_to_arena (ent, ent->client->arenanum, 1);
}


/*
==============================================================================

	PLAYER SKINS

	Every team gets one of MAX_ARENA_SKINS colours; setteamskin() rewrites a
	player's "skin" userinfo key to <bodymodel>/<teamcolour> while keeping
	whichever base body model (male/female/crakhor/cyborg) they picked.

==============================================================================
*/

int getfreeskin (int arenanum)
{
	qboolean	used[MAX_ARENA_SKINS];
	int			i;
	team_t		*t;

	memset (used, 0, sizeof (used));

	for (i = 0; i < MAX_TEAMS; i++)
	{
		t = teams[i];
		if (!t)
			continue;
		if (t->arenanum != arenanum)
			continue;
		if (t->skin == -1)
			continue;

		used[t->skin] = true;
	}

	for (i = 0; i < MAX_ARENA_SKINS; i++)
		if (!used[i])
			return i;

	return rand () % MAX_ARENA_SKINS;
}

char *mylcase (char *s)
{
	char	*p;

	for (p = s; *p; p++)
		if (*p >= 'A' && *p <= 'Z')
			*p += 'a' - 'A';

	return s;
}

qboolean checkvwepmodel (char *s)
{
	int		i;

	for (i = 0; i < 4; i++)
		if (strstr (s, vwepmodels[i]))
			return true;

	return false;
}

void setteamskin (edict_t *ent, char *userinfo, int skinnum)
{
	char	*val;
	char	*model;
	int		pnum;

	pnum = ent - g_edicts;
	val = Info_ValueForKey (userinfo, "skin");

	if (val[0] == 'f')
		model = "female";
	else if (val[0] == 'c' && val[1] == 'r')
		model = "crakhor";
	else if (val[0] == 'c' && val[1] == 'y')
		model = "cyborg";
	else
		model = "male";

	if (strcmp (val, va ("%s/%s", model, teamskins[skinnum])))
	{
		gi.configstring (CS_PLAYERSKINS + pnum,
			va ("%s\\%s/%s", ent->client->pers.netname, model, teamskins[skinnum]));

		Info_RemoveKey (userinfo, "skin");
		strcat (userinfo, va ("\\skin\\%s/%s", model, teamskins[skinnum]));

		stuffcmd (ent, va ("setinfo skin %s/%s\n", model, teamskins[skinnum]));
	}
}


/*
==============================================================================

	FILLING ARENAS

==============================================================================
*/

void SendTeamToArena (team_t *team, int arenanum, qboolean fighting, qboolean announce)
{
	arena_t		*arena;
	gclient_t	*cl;
	edict_t		*ent;

	arena = &arenas[arenanum];

	team->arenanum = arenanum;

	if (team->skin == -1)
		team->skin = getfreeskin (arenanum);

	if (!team->next && !team->prev && arena->teamlist != team)
	{
		team->next = arena->teamlist;
		if (arena->teamlist)
			arena->teamlist->prev = team;
		arena->teamlist = team;
	}

	for (cl = team->members; cl; cl = cl->arena_next)
	{
		ent = &g_edicts[(cl - game.clients) + 1];

		cl->arenanum = arenanum;
		cl->inarena = true;

		setteamskin (ent, ent->client->pers.userinfo, team->skin);
		give_ammo (ent);

		if (fighting)
			move_to_arena (ent, arenanum, 0);

		if (announce)
			NewStatsPlayer (arena->statsptr, ent, 0);
	}
}

void AddtoArena (edict_t *ent, int arenanum, int teamnum)
{
	arena_t	*arena;
	team_t	*t;

	arena = &arenas[arenanum];

	if (arena->locked)
	{
		gi.centerprintf (ent, "%s", va ("Arena %d is locked", arenanum));
		return;
	}

	if (arena->maxteams && teamnum >= arena->maxteams)
	{
		gi.centerprintf (ent, "%s", va ("Arena %d only supports %d teams", arenanum, arena->maxteams));
		return;
	}

	if ((arena->minping && ent->client->ping < arena->minping) ||
		(arena->maxping && ent->client->ping > arena->maxping))
	{
		gi.centerprintf (ent, "%s", va ("Your ping does not meet arena %d's requirements", arenanum));
		return;
	}

	if (arena->teamplay && arena->active)
	{
		// round already under way -- get in line for the next one
		add_to_queue (ent->client, &arena->waitqueue);
		gi.centerprintf (ent, "That arena is fighting right now\nYou have been placed in the wait queue");
		return;
	}

	if (arena->teamplay)
	{
		show_teamconfirm_menu (ent);
		return;
	}

	t = teams[teamnum];
	if (!t)
		return;

	SendTeamToArena (t, arenanum, arena->active, true);
}

void check_teams (int arenanum)
{
	arena_t		*arena;
	team_t		*t;
	gclient_t	*cl;

	arena = &arenas[arenanum];

	for (t = arena->teamlist; t; t = t->next)
		if (!t->members)
			gi.bprintf (PRINT_HIGH, "%s is now empty\n", t->name);

	// pull waiting players in once the arena has stopped fighting
	while (arena->waitqueue && !arena->active)
	{
		cl = remove_from_queue (NULL, &arena->waitqueue);
		if (cl->teamnum != -1)
			AddtoArena (&g_edicts[(cl - game.clients) + 1], arenanum, cl->teamnum);
	}

	set_config (1, arenanum);
}


/*
==============================================================================

	PLAYER INIT

==============================================================================
*/

void init_player (edict_t *ent)
{
	ent->client->arenanum = 0;
	ent->client->teamnum = -1;
	ent->client->inarena = false;
	ent->client->fightstate = FIGHT_SPECTATING;
	ent->client->omode = OMODE_FREEFLY;
	ent->client->lastomode = OMODE_FREEFLY;
	ent->client->track_target = NULL;
	ent->client->spawn_recheck = 0;
	ent->client->zbotscore = 0;
	ent->client->arena_next = NULL;
	ent->client->arena_prev = NULL;

	CTFPlayerResetGrapple (ent);

	move_to_arena (ent, 0, 1);
}

void reinit_player (edict_t *ent)
{
	ent->client->fightstate = FIGHT_SPECTATING;
	ent->client->track_target = NULL;
}


/*
==============================================================================

	MESSAGING HELPERS

==============================================================================
*/

void show_stringc (edict_t *ent, char *s)
{
	gi.centerprintf (ent, "%s", s);
}

void show_string (edict_t *ent, char *s)
{
	gi.WriteByte (svc_layout);
	gi.WriteString (s);
	gi.unicast (ent, true);
}

void stuffcmd (edict_t *ent, char *s)
{
	gi.WriteByte (svc_stufftext);
	gi.WriteString (s);
	gi.unicast (ent, true);
}

void send_sound_to_arena (int arenanum, int soundindex)
{
	int		i;
	edict_t	*e;

	for (i = 1; i <= maxclients->value; i++)
	{
		e = &g_edicts[i];
		if (!e->inuse || !e->client)
			continue;
		if (e->client->arenanum != arenanum)
			continue;

		gi.sound (e, CHAN_AUTO, soundindex, 1, ATTN_NONE, 0);
	}
}

void send_configstring (int num)
{
	gi.configstring (num, dm_statusbar);
}

void show_countdown (int arenanum)
{
	arena_t	*arena;
	char	*msg;
	int		remaining;
	int		i;
	edict_t	*e;

	arena = &arenas[arenanum];

	remaining = (int) (arena->statetime - level.time);
	if (remaining < 0)
		remaining = 0;

	if (remaining > 0)
		msg = va ("xv 96 yv 76 string2 \"%d\"", remaining);
	else
		msg = "xv 80 yv 76 string2 \"FIGHT!\"";

	for (i = 1; i <= maxclients->value; i++)
	{
		e = &g_edicts[i];
		if (!e->inuse || !e->client)
			continue;
		if (e->client->arenanum != arenanum)
			continue;

		show_string (e, msg);
	}

	if (!remaining)
		send_sound_to_arena (arenanum, gi.soundindex ("world/klaxon2.wav"));
}

void show_rank (edict_t *ent)
{
	gi.cprintf (ent, PRINT_HIGH, "Team: %d\n", ent->client->teamnum);
}


/*
==============================================================================

	ROUND FLOW

==============================================================================
*/

qboolean check_for_teams (int arenanum)
{
	arena_t	*arena;
	team_t	*t;
	int		count;

	arena = &arenas[arenanum];

	count = 0;
	for (t = arena->teamlist; t; t = t->next)
		count++;

	if (count < arena->numteams)
		return false;

	for (t = arena->teamlist; t; t = t->next)
		if (!t->members)
			return false;

	return true;
}

void fill_arena (int arenanum)
{
	arena_t	*arena;
	team_t	*t, *first;
	int		count;

	arena = &arenas[arenanum];
	arena->sidepick = rand () & 1;

	arena->vs[0] = 0;
	count = 0;
	first = NULL;

	while (arena->teamlist)
	{
		t = arena->teamlist;
		arena->teamlist = t->next;
		if (t->next)
			t->next->prev = NULL;
		t->next = NULL;
		t->prev = NULL;

		if (!first)
			first = t;
		else if (t->skin == first->skin)
		{
			t->skin = (count + 1) % MAX_ARENA_SKINS;
			gi.dprintf ("Skin conflict in arena %d\n", arenanum);
		}

		SendTeamToArena (t, arenanum, false, true);

		if (count)
			strcat (arena->vs, " vs ");
		strcat (arena->vs, t->name);

		if (arena->maxteams == 1)
			t->arenanum = 0;

		t->fighting = true;

		count++;
	}

	if (!count)
	{
		gi.dprintf ("Team left during multi-round match\n");
		return;
	}

	gi.dprintf ("%d: %s\n", arenanum, arena->vs);
}

int fight_done (int arenanum)
{
	team_t		*t;
	gclient_t	*cl;
	int			winner;

	winner = -1;

	for (t = arenas[arenanum].teamlist; t; t = t->next)
		for (cl = t->members; cl; cl = cl->arena_next)
		{
			if (cl->fightstate != FIGHT_ALIVE)
				continue;

			if (winner == -1)
				winner = cl->teamnum;
			else if (cl->teamnum != winner)
				return -2;
		}

	return winner;
}


/*
==============================================================================

	IDENTIFICATION / HUD

==============================================================================
*/

// unlike id's CTF version (which sweeps every connected player looking for
// whoever the ent is facing), RA2 only ever needs to know whether the
// observer is currently looking roughly at their own track target
void CTFSetIDView (edict_t *ent)
{
	trace_t		tr;
	vec3_t		forward, end;
	edict_t		*target;

	ent->client->ps.stats[STAT_CTF_ID_VIEW] = 0;

	if (ent->client->fightstate)
		return;

	target = ent->client->track_target;
	if (!target)
		return;

	AngleVectors (ent->client->cam_angle, forward, NULL, NULL);
	VectorScale (forward, 1024, forward);
	VectorAdd (ent->s.origin, forward, end);

	tr = gi.trace (ent->s.origin, NULL, NULL, end, ent, MASK_SOLID);

	if (tr.fraction < 1.0 && tr.ent && tr.ent->client && tr.ent->solid != SOLID_NOT)
		ent->client->ps.stats[STAT_CTF_ID_VIEW] = CS_PLAYERSKINS + (tr.ent - g_edicts);
}

void UpdateStatusBars (int arenanum)
{
	arena_t	*arena;
	team_t	*t;
	char	string[1400];
	int		i;
	edict_t	*e;

	arena = &arenas[arenanum];

	string[0] = 0;
	i = 0;
	for (t = arena->teamlist; t; t = t->next, i++)
		strcat (string, va ("xv %d yv 0 string \"%s\" ", i * 80, t->name));

	for (i = 1; i <= maxclients->value; i++)
	{
		e = &g_edicts[i];
		if (!e->inuse || !e->client)
			continue;
		if (e->client->arenanum != arenanum)
			continue;

		show_string (e, string);
	}
}

void check_telefrag (int arenanum)
{
	int			i;
	edict_t		*e;
	trace_t		tr;
	vec3_t		mins, maxs;
	vec3_t		forward;

	VectorSet (mins, -16, -16, -24);
	VectorSet (maxs, 16, 16, 32);

	for (i = 1; i <= maxclients->value; i++)
	{
		e = &g_edicts[i];

		if (!e->inuse || !e->client)
			continue;
		if (e->client->arenanum != arenanum)
			continue;
		if (!e->client->inarena)
			continue;
		if (!e->client->spawn_recheck)
			continue;
		if (level.framenum < e->client->spawn_recheck)
			continue;

		tr = gi.trace (e->s.origin, mins, maxs, e->s.origin, e, MASK_PLAYERSOLID);

		if (tr.startsolid)
		{
			e->solid = SOLID_NOT;
			e->client->spawn_recheck = level.framenum + rand () % 360;

			AngleVectors (e->client->v_angle, forward, NULL, NULL);
			VectorScale (forward, -600, forward);
			VectorAdd (e->velocity, forward, e->velocity);
		}
		else
		{
			e->solid = SOLID_BBOX;
			e->client->spawn_recheck = 0;

			gi.unlinkentity (e);
			KillBox (e);
			gi.linkentity (e);
		}
	}
}


/*
==============================================================================

	SETTINGS VOTING

==============================================================================
*/

void start_voting (int arenanum)
{
	arena_t	*arena;

	arena = &arenas[arenanum];

	arena->votes_yes = 0;
	arena->votes_no = 0;
	arena->votetries++;

	gi.dprintf ("Starting Voting in Arena %d with %d voters\n",
		arenanum, arena->teamlist ? count_players_queue (arena->teamlist->members) : 0);

	if (arena->proposer)
		stuffcmd (arena->proposer, "play misc/pc_up.wav\n");
}

void check_voting (int arenanum)
{
	arena_t	*arena;

	arena = &arenas[arenanum];

	if (!arena->proposetime)
		return;

	if (level.time < arena->proposetime)
		return;

	if (arena->votes_no >= arena->votes_yes)
	{
		gi.bprintf (PRINT_HIGH, "Changes Failed! Yes votes: %d No votes: %d\n",
			arena->votes_yes, arena->votes_no);

		if (votetries_setting && arena->votetries >= votetries_setting)
			arena->locked = true;
	}
	else
	{
		arena->votetries = 0;
	}

	arena->proposetime = 0;
	arena->proposer = NULL;
}


/*
==============================================================================

	MAIN ARENA LOOP

==============================================================================
*/

void arena_think (int arenanum)
{
	arena_t	*arena;
	int		winner;

	arena = &arenas[arenanum];

	check_teams (arenanum);
	check_voting (arenanum);
	check_telefrag (arenanum);

	switch (arena->state)
	{
	case ASTATE_WARMUP:
		if (!check_for_teams (arenanum))
			return;

		arena->state = ASTATE_COUNTDOWN;
		arena->statetime = level.time + 3;
		break;

	case ASTATE_COUNTDOWN:
		show_countdown (arenanum);

		if (level.time < arena->statetime)
			return;

		fill_arena (arenanum);
		set_damage (arenanum, FIGHT_ALIVE);
		arena->active = true;
		arena->round = 1;
		arena->state = ASTATE_FIGHTING;
		break;

	case ASTATE_FIGHTING:
		// "broken" is a safety valve an admin can raise to freeze HUD
		// updates server-wide if the layout code is misbehaving
		if (!broken)
			UpdateStatusBars (arenanum);

		winner = fight_done (arenanum);
		if (winner == -2)
			return;

		set_damage (arenanum, FIGHT_SPECTATING);
		arena->state = ASTATE_ROUNDEND;
		arena->statetime = level.time + 3;
		break;

	case ASTATE_ROUNDEND:
		if (level.time < arena->statetime)
			return;

		if (!check_for_teams (arenanum))
		{
			arena->active = false;
			arena->round = 0;
			arena->state = ASTATE_WARMUP;
			return;
		}

		arena->state = ASTATE_RESULTS;
		break;

	case ASTATE_INTERMISSION:
		if (level.time < arena->statetime)
			return;

		arena->state = ASTATE_NEXTROUND;
		break;

	case ASTATE_RESULTS:
		UpdateStatusBars (arenanum);

		if (arena->round >= arena->rounds)
		{
			arena->active = false;
			arena->round = 0;
			arena->state = ASTATE_WARMUP;
			return;
		}

		arena->state = ASTATE_INTERMISSION;
		arena->statetime = level.time + 5;
		break;

	case ASTATE_NEXTROUND:
		arena->round++;
		fill_arena (arenanum);
		set_damage (arenanum, FIGHT_ALIVE);
		arena->state = ASTATE_COUNTDOWN;
		arena->statetime = level.time + 3;
		break;
	}
}

void multi_arena_think (void)
{
	int		i;

	if (level.intermissiontime)
		return;

	i = level.framenum % (num_arenas * 2);
	if (i & 1)
		return;

	arena_think (i / 2 + 1);
}

void arena_init (edict_t *ent)
{
	int		i;
	team_t	*t;

	if (!ent)
		return;

	memset (teams, 0, sizeof (teams));
	memset (arenas, 0, sizeof (arenas));

	admincode = gi.cvar ("admincode", "", 0);

	num_arenas = ent->style;
	if (num_arenas <= 0)
	{
		num_arenas = 1;
		idmap = true;
	}
	else
	{
		idmap = false;
	}

	load_config (num_arenas + 1);
	set_config (1, num_arenas);

	for (i = 0; i < num_arenas; i++)
	{
		arenas[i].state = ASTATE_WARMUP;
		arenas[i].numteams = 2;
		arenas[i].active = idmap;

		if (!SelectFarthestArenaSpawnPoint ("misc_teleporter_dest", i))
		{
			gi.dprintf ("Setting arena %d to idarena mode\n", i);
			arenas[i].idarena = true;
		}

		if (idmap && arenas[i].teamplay)
		{
			t = add_to_team (NULL, va ("#%d Pickup Red", i));
			t->side = 0;
			SendTeamToArena (t, i, true, true);
			arenas[i].pickupteam[0] = t;

			t = add_to_team (NULL, va ("#%d Pickup Blue", i));
			t->side = 1;
			SendTeamToArena (t, i, true, true);
			arenas[i].pickupteam[1] = t;

			arenas[i].maxteams = 2;
			arenas[i].playersperteam = 128;
		}
	}

	load_motd ();
}


/*
==============================================================================

	MAP ENTITIES

==============================================================================
*/

void SP_trigger_teleport (edict_t *ent)
{
	ent->touch = teleporter_touch;
	ent->movetype = MOVETYPE_NONE;
	ent->svflags |= SVF_NOCLIENT;
	ent->solid = SOLID_TRIGGER;
	ent->use = NULL;
	gi.setmodel (ent, ent->model);
	gi.linkentity (ent);
}

void SP_func_illusionary (edict_t *ent)
{
	ent->movetype = MOVETYPE_NONE;
	ent->solid = SOLID_NOT;
	gi.setmodel (ent, ent->model);
	gi.linkentity (ent);
}

void SP_info_teleport_destination (edict_t *ent)
{
}


/*
==============================================================================

	OFFHAND GRAPPLE HOOK

	Ported close to verbatim from id Software's original CTF grapple
	(g_ctf.c); RA2's disassembly matches it almost byte for byte, right
	down to the sound effect paths, so only the surrounding RA2 naming
	(ctf_grapple/ctf_grapplestate rather than a CTF team) differs.

==============================================================================
*/

void CTFPlayerResetGrapple (edict_t *ent)
{
	if (ent->client && ent->client->ctf_grapple)
		CTFResetGrapple (ent->client->ctf_grapple);
}

// self is the grapple hook itself, not the player
void CTFResetGrapple (edict_t *self)
{
	if (self->owner->client->ctf_grapple)
	{
		float		volume = 1.0;
		gclient_t	*cl;

		if (self->owner->client->silencer_shots)
			volume = 0.2;

		gi.sound (self->owner, CHAN_RELIABLE + CHAN_WEAPON,
			gi.soundindex ("weapons/grapple/grreset.wav"), volume, ATTN_NORM, 0);

		cl = self->owner->client;
		cl->ctf_grapple = NULL;
		cl->ctf_grapplereleasetime = level.time;
		cl->ctf_grapplestate = CTF_GRAPPLE_STATE_FLY;
		cl->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;

		G_FreeEdict (self);
	}
}

void CTFGrappleTouch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	float	volume = 1.0;

	if (other == self->owner)
		return;

	if (self->owner->client->ctf_grapplestate != CTF_GRAPPLE_STATE_FLY)
		return;

	if (surf && (surf->flags & SURF_SKY))
	{
		CTFResetGrapple (self);
		return;
	}

	VectorCopy (vec3_origin, self->velocity);

	PlayerNoise (self->owner, self->s.origin, PNOISE_IMPACT);

	if (other->takedamage)
	{
		T_Damage (other, self, self->owner, self->velocity, self->s.origin,
			plane->normal, self->dmg, 1, 0, MOD_GRAPPLE);
		CTFResetGrapple (self);
		return;
	}

	self->owner->client->ctf_grapplestate = CTF_GRAPPLE_STATE_PULL;
	self->enemy = other;

	self->solid = SOLID_NOT;

	if (self->owner->client->silencer_shots)
		volume = 0.2;

	gi.sound (self->owner, CHAN_RELIABLE + CHAN_WEAPON,
		gi.soundindex ("weapons/grapple/grpull.wav"), volume, ATTN_NORM, 0);
	gi.sound (self, CHAN_WEAPON,
		gi.soundindex ("weapons/grapple/grhit.wav"), volume, ATTN_NORM, 0);

	gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_SPARKS);
	gi.WritePosition (self->s.origin);
	if (!plane)
		gi.WriteDir (vec3_origin);
	else
		gi.WriteDir (plane->normal);
	gi.multicast (self->s.origin, MULTICAST_PVS);
}

// draws the cable between the grapple and the player holding it
void CTFGrappleDrawCable (edict_t *self)
{
	vec3_t	offset, start, end, f, r;
	vec3_t	dir;
	float	distance;

	AngleVectors (self->owner->client->v_angle, f, r, NULL);
	VectorSet (offset, 16, 16, self->owner->viewheight - 8);
	P_ProjectSource (self->owner->client, self->owner->s.origin, offset, f, r, start);

	VectorSubtract (start, self->owner->s.origin, offset);

	VectorSubtract (start, self->s.origin, dir);
	distance = VectorLength (dir);
	if (distance < 64)
		return;

	VectorCopy (self->s.origin, end);

	gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_GRAPPLE_CABLE);
	gi.WriteShort (self->owner - g_edicts);
	gi.WritePosition (self->owner->s.origin);
	gi.WritePosition (end);
	gi.WritePosition (offset);
	gi.multicast (self->s.origin, MULTICAST_PVS);
}

// pulls the player toward the grapple, or the grapple toward whatever it's stuck to
void CTFGrapplePull (edict_t *self)
{
	vec3_t	hookdir, v;
	float	vlen;

	if (self->owner->client->pers.weapon != FindItem ("Grapple") &&
		!self->owner->client->newweapon &&
		self->owner->client->weaponstate != WEAPON_FIRING &&
		self->owner->client->weaponstate != WEAPON_ACTIVATING)
	{
		CTFResetGrapple (self);
		return;
	}

	if (self->enemy)
	{
		if (self->enemy->solid == SOLID_NOT)
		{
			CTFResetGrapple (self);
			return;
		}

		if (self->enemy->solid == SOLID_BBOX)
		{
			VectorScale (self->enemy->size, 0.5, v);
			VectorAdd (v, self->enemy->s.origin, v);
			VectorAdd (v, self->enemy->mins, self->s.origin);
			gi.linkentity (self);
		}
		else
			VectorCopy (self->enemy->velocity, self->velocity);

		if (self->enemy->takedamage && !CheckTeamDamage (self->enemy, self->owner))
		{
			float	volume = 1.0;

			if (self->owner->client->silencer_shots)
				volume = 0.2;

			T_Damage (self->enemy, self, self->owner, self->velocity, self->s.origin,
				vec3_origin, 1, 1, 0, MOD_GRAPPLE);
			gi.sound (self, CHAN_WEAPON,
				gi.soundindex ("weapons/grapple/grhurt.wav"), volume, ATTN_NORM, 0);
		}

		if (self->enemy->deadflag)
		{
			CTFResetGrapple (self);
			return;
		}
	}

	CTFGrappleDrawCable (self);

	if (self->owner->client->ctf_grapplestate > CTF_GRAPPLE_STATE_FLY)
	{
		vec3_t	forward, up;

		AngleVectors (self->owner->client->v_angle, forward, NULL, up);
		VectorCopy (self->owner->s.origin, v);
		v[2] += self->owner->viewheight;
		VectorSubtract (self->s.origin, v, hookdir);

		vlen = VectorLength (hookdir);

		if (self->owner->client->ctf_grapplestate == CTF_GRAPPLE_STATE_PULL && vlen < 64)
		{
			float	volume = 1.0;

			if (self->owner->client->silencer_shots)
				volume = 0.2;

			self->owner->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
			gi.sound (self->owner, CHAN_RELIABLE + CHAN_WEAPON,
				gi.soundindex ("weapons/grapple/grhang.wav"), volume, ATTN_NORM, 0);
			self->owner->client->ctf_grapplestate = CTF_GRAPPLE_STATE_HANG;
		}

		VectorNormalize (hookdir);
		VectorScale (hookdir, CTF_GRAPPLE_PULL_SPEED, hookdir);
		VectorCopy (hookdir, self->owner->velocity);
		SV_AddGravity (self->owner);
	}
}

void CTFFireGrapple (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int effect)
{
	edict_t	*grapple;
	trace_t	tr;

	VectorNormalize (dir);

	grapple = G_Spawn ();
	VectorCopy (start, grapple->s.origin);
	VectorCopy (start, grapple->s.old_origin);
	vectoangles (dir, grapple->s.angles);
	VectorScale (dir, speed, grapple->velocity);
	grapple->movetype = MOVETYPE_FLYMISSILE;
	grapple->clipmask = MASK_SHOT;
	grapple->solid = SOLID_BBOX;
	grapple->s.effects |= effect;
	VectorClear (grapple->mins);
	VectorClear (grapple->maxs);
	grapple->s.modelindex = gi.modelindex ("models/weapons/grapple/hook/tris.md2");
	grapple->owner = self;
	grapple->touch = CTFGrappleTouch;
	grapple->dmg = damage;
	self->client->ctf_grapple = grapple;
	self->client->ctf_grapplestate = CTF_GRAPPLE_STATE_FLY;
	gi.linkentity (grapple);

	tr = gi.trace (self->s.origin, NULL, NULL, grapple->s.origin, grapple, MASK_SHOT);
	if (tr.fraction < 1.0)
	{
		VectorMA (grapple->s.origin, -10, dir, grapple->s.origin);
		grapple->touch (grapple, tr.ent, NULL, NULL);
	}
}

void CTFGrappleFire (edict_t *ent, vec3_t g_offset, int damage, int effect)
{
	vec3_t	forward, right;
	vec3_t	start;
	vec3_t	offset;
	float	volume = 1.0;

	if (ent->client->ctf_grapplestate > CTF_GRAPPLE_STATE_FLY)
		return;

	AngleVectors (ent->client->v_angle, forward, right, NULL);
	VectorSet (offset, 24, 8, ent->viewheight - 8 + 2);
	VectorAdd (offset, g_offset, offset);
	P_ProjectSource (ent->client, ent->s.origin, offset, forward, right, start);

	VectorScale (forward, -2, ent->client->kick_origin);
	ent->client->kick_angles[0] = -1;

	if (ent->client->silencer_shots)
		volume = 0.2;

	gi.sound (ent, CHAN_RELIABLE + CHAN_WEAPON,
		gi.soundindex ("weapons/grapple/grfire.wav"), volume, ATTN_NORM, 0);
	CTFFireGrapple (ent, start, forward, damage, CTF_GRAPPLE_SPEED, effect);

	PlayerNoise (ent, start, PNOISE_WEAPON);
}

void CTFWeapon_Grapple_Fire (edict_t *ent)
{
	int		damage;

	damage = 10;
	CTFGrappleFire (ent, vec3_origin, damage, 0);
	ent->client->ps.gunframe++;
}

void CTFWeapon_Grapple (edict_t *ent)
{
	static int	pause_frames[] = { 10, 18, 27, 0 };
	static int	fire_frames[] = { 6, 0 };
	int			prevstate;

	if ((ent->client->buttons & BUTTON_ATTACK) &&
		ent->client->weaponstate == WEAPON_FIRING &&
		ent->client->ctf_grapple)
		ent->client->ps.gunframe = 9;

	if (!(ent->client->buttons & BUTTON_ATTACK) && ent->client->ctf_grapple)
	{
		CTFResetGrapple (ent->client->ctf_grapple);
		if (ent->client->weaponstate == WEAPON_FIRING)
			ent->client->weaponstate = WEAPON_READY;
	}

	if (ent->client->newweapon &&
		ent->client->ctf_grapplestate > CTF_GRAPPLE_STATE_FLY &&
		ent->client->weaponstate == WEAPON_FIRING)
	{
		ent->client->weaponstate = WEAPON_DROPPING;
		ent->client->ps.gunframe = 32;
	}

	prevstate = ent->client->weaponstate;
	Weapon_Generic (ent, 5, 9, 31, 36, pause_frames, fire_frames, CTFWeapon_Grapple_Fire);

	if (prevstate == WEAPON_ACTIVATING &&
		ent->client->weaponstate == WEAPON_READY &&
		ent->client->ctf_grapplestate > CTF_GRAPPLE_STATE_FLY)
	{
		if (!(ent->client->buttons & BUTTON_ATTACK))
			ent->client->ps.gunframe = 9;
		else
			ent->client->ps.gunframe = 5;
		ent->client->weaponstate = WEAPON_FIRING;
	}
}
