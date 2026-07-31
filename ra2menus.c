#include "g_local.h"
#include "arena.h"

char	*get_next_map (char *current);		// maploop.c

void	Cmd_arenaadmin_f (edict_t *ent, int mode);
void	menu_centerprint (edict_t *ent, char *message);

/*
==============
StringForProtect
==============
*/
char *
StringForProtect (int protect)
{
	switch (protect)
	{
	case 1:
		return "Dont damage team";
	case 2:
		return "Damage self not team";
	default:
		return "Damage all";
	}
}

/*
==============
NumForProtect
==============
*/
int
NumForProtect (char *s)
{
	if (!strcmp (s, "Damage all"))
		return 0;
	if (!strcmp (s, "Dont damage team"))
		return 1;
	if (!strcmp (s, "Damage self not team"))
		return 2;

	return 0;
}

/*
==============
menuDoNothing
==============
*/
int
menuDoNothing (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	return 2;
}

/*
==============
menuLeaveArena
==============
*/
int
menuLeaveArena (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	team_t	*team;

	team = teams[ent->client->teamnum];

	if (arenas[team->arenanum].state != ASTATE_COUNTDOWN
		&& arenas[team->arenanum].state != ASTATE_ROUNDEND
		&& ent->takedamage)
	{
		menu_centerprint (ent, "Sorry, you cannot leave the arena\nduring a match");
		return 2;
	}

	remove_from_queue (ent->client, &team->members);
	SendTeamToArena (team, 0, 1, 1);

	return 0;
}

/*
==============
menuAddtoArena
==============
*/
int
menuAddtoArena (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	menuitem_t	*it;

	it = item->data;

	AddtoArena (ent, it->num, ent->client->teamnum);

	return 0;
}

/*
==============
menuLeaveTeamAr
==============
*/
int
menuLeaveTeamAr (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	team_t	*team;

	team = teams[ent->client->teamnum];

	if (arenas[team->arenanum].state != ASTATE_COUNTDOWN
		&& arenas[team->arenanum].state != ASTATE_ROUNDEND
		&& ent->takedamage)
	{
		menu_centerprint (ent, "Sorry, you cannot leave the arena\nduring a match");
		return 2;
	}

	remove_from_team (ent);
	move_to_arena (ent, 0, 1);
	init_player (ent);

	return 0;
}

/*
==============
menuLeaveTeam
==============
*/
int
menuLeaveTeam (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	remove_from_team (ent);
	init_player (ent);

	return 0;
}

/*
==============
menuStepInOutofLine
==============
*/
int
menuStepInOutofLine (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	team_t	*team;
	int		arenanum;
	int		wasinline;

	arenanum = ent->client->arenanum;
	team = teams[ent->client->teamnum];
	wasinline = (team->fighting == 0);

	if (arenas[arenanum].state != ASTATE_COUNTDOWN
		&& arenas[arenanum].state != ASTATE_ROUNDEND
		&& ent->takedamage)
	{
		menu_centerprint (ent, "Sorry, you cannot leave the arena\nduring a match");
		return 2;
	}

	remove_from_queue (ent->client, &team->members);
	SendTeamToArena (team, 0, 1, 1);

	AddtoArena (ent, arenanum, ent->client->teamnum);

	return wasinline;
}

/*
==============
getarenaname

Looks for an info_player_intermission entity flagged for this arena
(its "count" field) and returns its "message".  Falls back to a
generic "Arena Number N" if none was placed in the map.
==============
*/
char *
getarenaname (int arenanum)
{
	edict_t	*e;

	e = NULL;
	while ((e = G_Find (e, FOFS (classname), "info_player_intermission")) != NULL)
	{
		if (e->count == arenanum)
			return e->message;
	}

	return va ("Arena Number %d", arenanum);
}

/*
==============
menuChangeOMode
==============
*/
int
menuChangeOMode (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	ChangeOMode (ent);

	return 2;
}

/*
==============
menuShowSettingsPropose
==============
*/
int
menuShowSettingsPropose (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	arena_t	*arena;
	int		remaining;

	arena = &arenas[ent->client->arenanum];

	if (level.time < arena->proposetime)
	{
		remaining = (int) (arena->proposetime - level.time);

		if (remaining > 29)
			menu_centerprint (ent, "Voting is in progress.\nPlease wait");
		else
			menu_centerprint (ent, va ("Voting is in progress.\nPlease wait %d seconds", remaining));

		return 2;
	}

	if (ent->client->votes == 0)
	{
		menu_centerprint (ent, va ("Sorry, you cannot propose any more changes.\nYou have already proposed %d times\n", votetries_setting));
		return 2;
	}

	ent->client->votes--;
	Cmd_arenaadmin_f (ent, 1);

	return 2;
}

/*
==============
menuShowSettingsVote
==============
*/
int
menuShowSettingsVote (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	arena_t	*arena;

	arena = &arenas[ent->client->arenanum];

	if (level.time >= arena->proposetime)
	{
		menu_centerprint (ent, "No changes have been proposed");
		return 2;
	}

	if (ent->client->voted)
	{
		menu_centerprint (ent, "You have already voted");
		return 2;
	}

	Cmd_arenaadmin_f (ent, 2);

	return 2;
}

/*
==============
show_observer_menu
==============
*/
int
show_observer_menu (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	qmenu_t	*m;
	team_t	*team;
	arena_t	*arena;
	char	*label;

	m = CreateQMenu (ent, "Observer Options");

	arena = &arenas[ent->client->arenanum];

	if (!arena->locked)
	{
		AddMenuItem (m, "Change Arena Settings", NULL, -1, menuShowSettingsPropose);
		AddMenuItem (m, "Vote on Changes", NULL, -1, menuShowSettingsVote);
		AddMenuItem (m, "", NULL, -1, NULL);
	}

	if (!arena->locked)
	{
		team = teams[ent->client->teamnum];
		label = va ("Step %s Line", team->fighting ? "out of" : "into");
		AddMenuItem (m, label, NULL, -1, menuStepInOutofLine);
		AddMenuItem (m, "", NULL, -1, NULL);
	}

	AddMenuItem (m, "Leave Team", NULL, -1, menuLeaveTeamAr);

	if (!arena->locked)
		AddMenuItem (m, "Leave Arena", NULL, -1, menuLeaveArena);

	FinishMenu (ent, m, 0);

	return 0;
}

/*
==============
show_arena_menu
==============
*/
void
show_arena_menu (edict_t *ent)
{
	qmenu_t	*m;
	arena_t	*arena;
	char	*name;
	char	*value;
	int		count;
	int		i;

	m = CreateQMenu (ent, "Choose Your Arena");

	for (i = 1; i <= num_arenas; i++)
	{
		arena = &arenas[i];
		name = getarenaname (i);

		if (arena->idarena)
		{
			value = " (PT)";
		}
		else
		{
			count = count_queue (arena->pickupteam[0]->members) + count_queue (arena->pickupteam[1]->members);
			value = va (" T:%d", count);
		}

		AddMenuItem (m, name, value, i, menuAddtoArena);
	}

	AddMenuItem (m, "", NULL, -1, NULL);
	AddMenuItem (m, "Leave Team", NULL, -1, menuLeaveTeam);

	FinishMenu (ent, m, 1);
}

/*
==============
menuAddtoTeam
==============
*/
int
menuAddtoTeam (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	menuitem_t	*it;
	team_t		*team;

	it = item->data;

	if (!add_to_team (ent, it->text))
	{
		menu_centerprint (ent, "That team is already in an arena\nand full or\nthe arena is locked");
		return 2;
	}

	team = teams[ent->client->teamnum];

	if (team->arenanum)
	{
		ent->client->inarena = false;
		ent->takedamage = 0;
		move_to_arena (ent, team->arenanum, 1);
	}
	else
	{
		show_arena_menu (ent);
	}

	return 0;
}

/*
==============
menuNewTeam
==============
*/
int
menuNewTeam (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	char	name[100];
	int		i;

	Com_sprintf (name, 100, "%s's Team", ent->client->pers.netname);

	for (i = 0; i <= 255; i++)
	{
		if (!teams[i])
			break;

		if (!strcmp (teams[i]->name, name))
		{
			strcat (name, "!");
			i = -1;
		}
	}

	add_to_team (ent, name);
	show_arena_menu (ent);

	return 0;
}

/*
==============
menuRefreshTeamList
==============
*/
int
menuRefreshTeamList (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	qmenu_t	*m;
	int		i;

	m = CreateQMenu (ent, "Choose your team");
	AddMenuItem (m, "Start New Team", NULL, -1, menuNewTeam);

	for (i = 0; i <= 255; i++)
	{
		if (teams[i])
			AddMenuItem (m, teams[i]->name, va (" Players: %d", count_queue (teams[i]->members)), -1, menuAddtoTeam);
	}

	AddMenuItem (m, "Refresh List", NULL, -1, menuRefreshTeamList);
	AddMenuItem (m, "", NULL, -1, NULL);
	AddMenuItem (m, "Confused? try /cmd menuhelp", NULL, -1, NULL);

	FinishMenu (ent, m, 1);

	return 2;
}

/*
==============
menuChangeValueAZ

"AZ" -- Allow Zero: clamps its floor at 0.
==============
*/
int
menuChangeValueAZ (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	menuitem_t	*it;

	it = item->data;

	if (arg)
		it->num++;
	else
		it->num--;

	if (it->num < 0)
		it->num = 0;

	return 1;
}

/*
==============
menuChangeValue10AZ
==============
*/
int
menuChangeValue10AZ (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	menuitem_t	*it;

	it = item->data;

	if (arg)
		it->num += 10;
	else
		it->num -= 10;

	if (it->num < 0)
		it->num = 0;

	return 1;
}

/*
==============
menuChangeValue50AZ
==============
*/
int
menuChangeValue50AZ (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	menuitem_t	*it;

	it = item->data;

	if (arg)
		it->num += 50;
	else
		it->num -= 50;

	if (it->num < 0)
		it->num = 0;

	return 1;
}

/*
==============
menuChangeValue50

No "AZ" -- floors at the step size instead of zero.
==============
*/
int
menuChangeValue50 (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	menuitem_t	*it;

	it = item->data;

	if (arg)
		it->num += 50;
	else
		it->num -= 50;

	if (it->num <= 0)
		it->num = 50;

	return 1;
}

/*
==============
menuChangeValue
==============
*/
int
menuChangeValue (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	menuitem_t	*it;

	it = item->data;

	if (arg)
		it->num++;
	else
		it->num--;

	if (it->num == 0)
		it->num = 1;

	return 1;
}

/*
==============
menuChangeValue10
==============
*/
int
menuChangeValue10 (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	menuitem_t	*it;

	it = item->data;

	if (arg)
		it->num += 10;
	else
		it->num -= 10;

	if (it->num <= 0)
		it->num = 10;

	return 1;
}

/*
==============
menuChangeYesNo
==============
*/
int
menuChangeYesNo (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	menuitem_t	*it;

	it = item->data;

	if (it->value[0] == 'Y')
		it->value = "NO ";
	else
		it->value = "YES";

	return 1;
}

/*
==============
menuChangeProtect

Cycles the tri-state "Damage all" / "Dont damage team" /
"Damage self not team" setting.
==============
*/
int
menuChangeProtect (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	menuitem_t	*it;
	int			protect;

	it = item->data;
	protect = NumForProtect (it->value);

	if (arg)
		protect++;
	else
		protect--;

	if (protect < 0)
		protect = 2;
	if (protect > 2)
		protect = 0;

	strcpy (it->value, StringForProtect (protect));

	return 1;
}

/*
==============
menuChangeMap
==============
*/
int
menuChangeMap (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	menuitem_t	*it;

	it = item->data;
	strcpy (it->value, get_next_map (it->value));

	return 1;
}

/*
==============
cvar_setvalue
==============
*/
void
cvar_setvalue (char *name, int value)
{
	char	buf[256];

	sprintf (buf, "%d", value);
	gi.cvar_set (name, buf);
}

/*
==============
menuApplyAdmin

"Apply" button for the server admin menu -- commits the fraglimit,
timelimit and mapname fields the player edited.
==============
*/
int
menuApplyAdmin (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	menuinfo_t	*info;
	qmenu_t		*node;
	menuitem_t	*it;
	edict_t		*e;
	char		*map;

	info = menu->data;
	map = NULL;

	for (node = info->items; node != NULL; node = node->next)
	{
		it = node->data;

		if (!Q_stricmp (it->text, "Fraglimit:        "))
			cvar_setvalue ("fraglimit", it->num);
		else if (!Q_stricmp (it->text, "Timelimit:        "))
			cvar_setvalue ("timelimit", it->num);
		else if (!Q_stricmp (it->text, "Mapname:          "))
			map = it->value;
	}

	e = G_Spawn ();
	e->classname = "target_changelevel";
	e->map = gi.TagMalloc (strlen (map) + 1, TAG_LEVEL);
	strcpy (e->map, map);

	BeginIntermission (e);

	return 0;
}

/*
==============
menuCancel
==============
*/
int
menuCancel (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	return 0;
}

/*
==============
Cmd_menuhelp_f
==============
*/
void
Cmd_menuhelp_f (edict_t *ent)
{
	gi.cprintf (ent, PRINT_HIGH, "H| Use invprev and invnext ([ and ])\nH| to navigate the menu\nH| invuse (ENTER) selects\nH| inven (TAB) toggles it on/off\n");
}

/*
==============
Cmd_admin_f

Raw "admin <code>" client command -- if the typed code matches the
admincode cvar, brings up the fraglimit/timelimit/mapname menu.  No
per-player "is admin" state is kept; the code is re-checked every time.
==============
*/
void
Cmd_admin_f (edict_t *ent)
{
	int			code;
	qmenu_t		*m;
	menuinfo_t	*info;
	menuitem_t	*it;

	if (admincode->value == 0)
		return;

	code = atoi (gi.argv (1));

	if ((float) code != admincode->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "Sorry, incorrect admin code\n");
		return;
	}

	m = CreateQMenu (ent, "Admin Menu");

	AddMenuItem (m, "Fraglimit:        ", NULL, (int) fraglimit->value, menuChangeValue10AZ);
	AddMenuItem (m, "Timelimit:        ", NULL, (int) timelimit->value, menuChangeValue10AZ);
	AddMenuItem (m, "Mapname:          ", "                                ", -1, menuChangeMap);

	info = m->data;
	it = info->items->data;
	strcpy (it->value, level.mapname);

	AddMenuItem (m, "", NULL, -1, NULL);
	AddMenuItem (m, "Apply", NULL, -1, menuApplyAdmin);
	AddMenuItem (m, "Cancel", NULL, -1, menuCancel);

	FinishMenu (ent, m, 1);
}

/*
==============
menuApplyArenaAdmin

"Propose"/"Apply" callback for the arena admin menu -- walks the
currently displayed menu's fields by name and stages each one into
arenas[n].proposed, then kicks off (or continues) the vote.
==============
*/
int
menuApplyArenaAdmin (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	menuinfo_t			*info;
	qmenu_t				*node;
	menuitem_t			*it;
	menuitem_t			*refitem;
	arena_t				*arena;
	arena_settings_t	*prop;
	int					arenanum;
	int					weapons;
	int					remaining;
	int					i;

	arena = NULL;
	prop = NULL;
	arenanum = 0;
	refitem = item->data;

	info = menu->data;
	if (info->items == NULL)
		return 0;

	for (node = info->items; node != NULL; node = node->next)
	{
		it = node->data;

		if (!Q_stricmp (it->text, "Arena:                 "))
		{
			arenanum = it->num;
			arena = &arenas[arenanum];
			prop = &arena->proposed;

			if (refitem->text[0] == 'A')
			{
				if (level.time < arena->proposetime)
				{
					remaining = (int) (arena->proposetime - level.time);
					menu_centerprint (ent, va ("Voting is in progress.\nPlease wait %d seconds", remaining));
					return 2;
				}

				prop->playersperteam = arena->playersperteam;
				prop->rounds = arena->rounds;
				prop->weapons = arena->weapons;
				prop->armor = arena->armor;
				prop->health = arena->health;
				prop->minping = arena->minping;
				prop->maxping = arena->maxping;
				prop->armorprotect = arena->armorprotect;
				prop->healthprotect = arena->healthprotect;
				prop->fallingdamage = arena->fallingdamage;
				prop->locked = arena->locked;
				prop->competition = arena->competition;
				prop->scorebydamage = arena->scorebydamage;

				start_voting (arenanum);
				arena->votes_yes++;
				ent->client->voted = true;
			}

			prop->pending = true;

			weapons = 0;
			for (i = 0; i < 9; i++)
				if (!arena->weaponlock[i] && (arena->weapons & weapon_vals[i]))
					weapons |= weapon_vals[i];
			prop->weapons = weapons;

			continue;
		}

		if (!Q_stricmp (it->text, "Players per team:      "))
		{
			prop->playersperteam = it->num;
		}
		else if (!Q_stricmp (it->text, "Initial Armor:         "))
		{
			prop->armor = it->num;
		}
		else if (!Q_stricmp (it->text, "Initial Health:        "))
		{
			prop->health = it->num;
		}
		else if (!Q_stricmp (it->text, "Minimum Ping:          "))
		{
			prop->minping = it->num;
		}
		else if (!Q_stricmp (it->text, "Maximum Ping:          "))
		{
			prop->maxping = it->num;
		}
		else if (!Q_stricmp (it->text, "Rounds:                "))
		{
			prop->rounds = it->num | 1;
		}
		else if (!Q_stricmp (it->text, "Allow Shotgun:         "))
		{
			if (it->value[0] == 'Y')
				prop->weapons |= weapon_vals[0];
		}
		else if (!Q_stricmp (it->text, "Allow Super Shotgun:   "))
		{
			if (it->value[0] == 'Y')
				prop->weapons |= weapon_vals[1];
		}
		else if (!Q_stricmp (it->text, "Allow Machine gun:     "))
		{
			if (it->value[0] == 'Y')
				prop->weapons |= weapon_vals[2];
		}
		else if (!Q_stricmp (it->text, "Allow Chain gun:       "))
		{
			if (it->value[0] == 'Y')
				prop->weapons |= weapon_vals[3];
		}
		else if (!Q_stricmp (it->text, "Allow Grenade Launcher:"))
		{
			if (it->value[0] == 'Y')
				prop->weapons |= weapon_vals[4];
		}
		else if (!Q_stricmp (it->text, "Allow Rocket Launcher: "))
		{
			if (it->value[0] == 'Y')
				prop->weapons |= weapon_vals[5];
		}
		else if (!Q_stricmp (it->text, "Allow Hyperblaster:    "))
		{
			if (it->value[0] == 'Y')
				prop->weapons |= weapon_vals[6];
		}
		else if (!Q_stricmp (it->text, "Allow Railgun:         "))
		{
			if (it->value[0] == 'Y')
				prop->weapons |= weapon_vals[7];
		}
		else if (!Q_stricmp (it->text, "Allow BFG10K:          "))
		{
			if (it->value[0] == 'Y')
				prop->weapons |= weapon_vals[8];
		}
		else if (!Q_stricmp (it->text, "Health: "))
		{
			prop->healthprotect = NumForProtect (it->value);
		}
		else if (!Q_stricmp (it->text, "Armor:  "))
		{
			prop->armorprotect = NumForProtect (it->value);
		}
		else if (!Q_stricmp (it->text, "Falling Damage:        "))
		{
			prop->fallingdamage = (it->value[0] == 'Y');
		}
		else if (!Q_stricmp (it->text, "Lock Arena:            "))
		{
			prop->locked = (it->value[0] == 'Y');
		}
		else if (!Q_stricmp (it->text, "Competition Mode:      "))
		{
			prop->competition = (it->value[0] == 'Y');
		}
		else if (!Q_stricmp (it->text, "Damage Scoring:        "))
		{
			prop->scorebydamage = (it->value[0] == 'Y');
		}
	}

	check_teams (arenanum);

	return 0;
}

/*
==============
menuVote
==============
*/
int
menuVote (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	menuitem_t	*it;
	arena_t		*arena;

	arena = &arenas[ent->client->arenanum];
	it = item->data;

	if (level.time >= arena->proposetime)
	{
		menu_centerprint (ent, "Sorry, voting is over");
		return 2;
	}

	if (ent->client->voted)
	{
		menu_centerprint (ent, "You have already voted");
		return 2;
	}

	if (it->value[0] == 'Y')
		arena->votes_yes++;
	else
		arena->votes_no++;

	ent->client->voted = true;

	return 0;
}

/*
==============
Cmd_arenaadmin_f

Shared entry point for the arena admin menu:
  mode 0 -- raw "arenaadmin <arenanum> <code>" client command; checked
            against admincode, all fields shown regardless of lock state
  mode 1 -- player "Change Arena Settings" propose flow; locked fields
            hidden, editing callbacks live
  mode 2 -- player "Vote on Changes" flow; only fields that differ from
            the live settings are shown, read-only, with Yes/No buttons
==============
*/
void
Cmd_arenaadmin_f (edict_t *ent, int mode)
{
	int					arenanum;
	arena_t				*arena;
	arena_settings_t	*prop;
	qmenu_t				*m;

	arenanum = 0;

	if (mode == 1)
	{
		/* propose flow -- arenanum picked up below */
	}
	else if (mode < 1)
	{
		int		code;

		if (admincode->value == 0)
			return;

		arenanum = atoi (gi.argv (1));
		code = atoi (gi.argv (2));

		if ((float) code != admincode->value)
			return;
	}
	else if (mode == 2)
	{
		arenanum = ent->client->arenanum;

		if (arenanum <= 0 || arenanum > num_arenas)
			return;

		arena = &arenas[arenanum];
		prop = &arena->proposed;

		m = CreateQMenu (ent, "Proposed Changes");
		AddMenuItem (m, "Arena:                 ", NULL, arenanum, NULL);

		if (!arena->idarena && arena->playersperteam != prop->playersperteam)
			AddMenuItem (m, "Players per team:      ", NULL, prop->playersperteam, NULL);

		if (arena->health != prop->health)
			AddMenuItem (m, "Initial Health:        ", NULL, prop->health, NULL);

		if (arena->armor != prop->armor)
			AddMenuItem (m, "Initial Armor:         ", NULL, prop->armor, NULL);

		if (arena->minping != prop->minping)
			AddMenuItem (m, "Minimum Ping:          ", NULL, prop->minping, NULL);

		if (arena->maxping != prop->maxping)
			AddMenuItem (m, "Maximum Ping:          ", NULL, prop->maxping, NULL);

		if (arena->rounds != prop->rounds)
			AddMenuItem (m, "Rounds:                ", NULL, prop->rounds, NULL);

		if ((arena->weapons & weapon_vals[0]) != (prop->weapons & weapon_vals[0]))
			AddMenuItem (m, "Allow Shotgun:         ", (prop->weapons & weapon_vals[0]) ? "YES" : "NO ", -1, NULL);
		if ((arena->weapons & weapon_vals[1]) != (prop->weapons & weapon_vals[1]))
			AddMenuItem (m, "Allow Super Shotgun:   ", (prop->weapons & weapon_vals[1]) ? "YES" : "NO ", -1, NULL);
		if ((arena->weapons & weapon_vals[2]) != (prop->weapons & weapon_vals[2]))
			AddMenuItem (m, "Allow Machine gun:     ", (prop->weapons & weapon_vals[2]) ? "YES" : "NO ", -1, NULL);
		if ((arena->weapons & weapon_vals[3]) != (prop->weapons & weapon_vals[3]))
			AddMenuItem (m, "Allow Chain gun:       ", (prop->weapons & weapon_vals[3]) ? "YES" : "NO ", -1, NULL);
		if ((arena->weapons & weapon_vals[4]) != (prop->weapons & weapon_vals[4]))
			AddMenuItem (m, "Allow Grenade Launcher:", (prop->weapons & weapon_vals[4]) ? "YES" : "NO ", -1, NULL);
		if ((arena->weapons & weapon_vals[5]) != (prop->weapons & weapon_vals[5]))
			AddMenuItem (m, "Allow Rocket Launcher: ", (prop->weapons & weapon_vals[5]) ? "YES" : "NO ", -1, NULL);
		if ((arena->weapons & weapon_vals[6]) != (prop->weapons & weapon_vals[6]))
			AddMenuItem (m, "Allow Hyperblaster:    ", (prop->weapons & weapon_vals[6]) ? "YES" : "NO ", -1, NULL);
		if ((arena->weapons & weapon_vals[7]) != (prop->weapons & weapon_vals[7]))
			AddMenuItem (m, "Allow Railgun:         ", (prop->weapons & weapon_vals[7]) ? "YES" : "NO ", -1, NULL);
		if ((arena->weapons & weapon_vals[8]) != (prop->weapons & weapon_vals[8]))
			AddMenuItem (m, "Allow BFG10K:          ", (prop->weapons & weapon_vals[8]) ? "YES" : "NO ", -1, NULL);

		if (arena->healthprotect != prop->healthprotect)
			AddMenuItem (m, "Health: ", StringForProtect (prop->healthprotect), -1, NULL);
		if (arena->armorprotect != prop->armorprotect)
			AddMenuItem (m, "Armor:  ", StringForProtect (prop->armorprotect), -1, NULL);
		if (arena->fallingdamage != prop->fallingdamage)
			AddMenuItem (m, "Falling Damage:        ", prop->fallingdamage ? "YES" : "NO ", -1, NULL);
		if (arena->competition != prop->competition)
			AddMenuItem (m, "Competition Mode:      ", prop->competition ? "YES" : "NO ", -1, NULL);
		if (arena->scorebydamage != prop->scorebydamage)
			AddMenuItem (m, "Damage Scoring:        ", prop->scorebydamage ? "YES" : "NO ", -1, NULL);

		AddMenuItem (m, "", NULL, -1, NULL);

		AddMenuItem (m, "Vote ", "Yes", -1, menuVote);
		AddMenuItem (m, "Vote ", "No", -1, menuVote);

		AddMenuItem (m, "Cancel", NULL, -1, menuCancel);
		FinishMenu (ent, m, 1);

		return;
	}
	else
	{
		return;
	}

	if (mode >= 1)
		arenanum = ent->client->arenanum;

	if (arenanum <= 0 || arenanum > num_arenas)
		return;

	arena = &arenas[arenanum];

	m = CreateQMenu (ent, "Arena Admin Menu");
	AddMenuItem (m, "Arena:                 ", NULL, arenanum, NULL);

	if (!arena->idarena)
		AddMenuItem (m, "Players per team:      ", NULL, arena->playersperteam, menuChangeValue);

	AddMenuItem (m, "Initial Health:        ", NULL, arena->health, menuChangeValue50);
	AddMenuItem (m, "Initial Armor:         ", NULL, arena->armor, menuChangeValue50);
	AddMenuItem (m, "Minimum Ping:          ", NULL, arena->minping, menuChangeValue50AZ);
	AddMenuItem (m, "Maximum Ping:          ", NULL, arena->maxping, menuChangeValue50AZ);
	AddMenuItem (m, "Rounds:                ", NULL, arena->rounds, menuChangeValue);

	AddMenuItem (m, "Allow Shotgun:         ", (arena->weapons & weapon_vals[0]) ? "YES" : "NO ", -1, menuChangeYesNo);
	AddMenuItem (m, "Allow Super Shotgun:   ", (arena->weapons & weapon_vals[1]) ? "YES" : "NO ", -1, menuChangeYesNo);
	AddMenuItem (m, "Allow Machine gun:     ", (arena->weapons & weapon_vals[2]) ? "YES" : "NO ", -1, menuChangeYesNo);
	AddMenuItem (m, "Allow Chain gun:       ", (arena->weapons & weapon_vals[3]) ? "YES" : "NO ", -1, menuChangeYesNo);
	AddMenuItem (m, "Allow Grenade Launcher:", (arena->weapons & weapon_vals[4]) ? "YES" : "NO ", -1, menuChangeYesNo);
	AddMenuItem (m, "Allow Rocket Launcher: ", (arena->weapons & weapon_vals[5]) ? "YES" : "NO ", -1, menuChangeYesNo);
	AddMenuItem (m, "Allow Hyperblaster:    ", (arena->weapons & weapon_vals[6]) ? "YES" : "NO ", -1, menuChangeYesNo);
	AddMenuItem (m, "Allow Railgun:         ", (arena->weapons & weapon_vals[7]) ? "YES" : "NO ", -1, menuChangeYesNo);
	AddMenuItem (m, "Allow BFG10K:          ", (arena->weapons & weapon_vals[8]) ? "YES" : "NO ", -1, menuChangeYesNo);

	AddMenuItem (m, "Health: ", StringForProtect (arena->healthprotect), -1, menuChangeProtect);
	AddMenuItem (m, "Armor:  ", StringForProtect (arena->armorprotect), -1, menuChangeProtect);
	AddMenuItem (m, "Falling Damage:        ", arena->fallingdamage ? "YES" : "NO ", -1, menuChangeYesNo);

	if (mode == 0)
		AddMenuItem (m, "Lock Arena:            ", arena->locked ? "YES" : "NO ", -1, menuChangeYesNo);

	AddMenuItem (m, "Competition Mode:      ", arena->competition ? "YES" : "NO ", -1, menuChangeYesNo);
	AddMenuItem (m, "Damage Scoring:        ", arena->scorebydamage ? "YES" : "NO ", -1, menuChangeYesNo);

	AddMenuItem (m, "", NULL, -1, NULL);

	if (mode == 0)
		AddMenuItem (m, "Apply", NULL, -1, menuApplyArenaAdmin);

	AddMenuItem (m, "Propose", NULL, -1, menuApplyArenaAdmin);

	AddMenuItem (m, "Cancel", NULL, -1, menuCancel);
	FinishMenu (ent, m, 1);
}

/*
==============
menuMotdContinue

"Continue" button on the message-of-the-day screen -- drops the
player straight into the team-choice menu.
==============
*/
int
menuMotdContinue (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	qmenu_t	*m;
	int		i;

	ent->client->showmenu = false;

	m = CreateQMenu (ent, "Choose your team");
	AddMenuItem (m, "Start New Team", NULL, -1, menuNewTeam);

	for (i = 0; i <= 255; i++)
	{
		if (teams[i])
			AddMenuItem (m, teams[i]->name, va (" Players: %d", count_queue (teams[i]->members)), -1, menuAddtoTeam);
	}

	AddMenuItem (m, "Refresh List", NULL, -1, menuRefreshTeamList);
	AddMenuItem (m, "", NULL, -1, NULL);
	AddMenuItem (m, "Confused? try /cmd menuhelp", NULL, -1, NULL);

	FinishMenu (ent, m, 1);

	return 0;
}

/*
==============
motd_menu

Shown once at connect.  If there's no message of the day configured,
skips straight to team selection; otherwise shows the motd text with
a "Continue" button that leads to menuMotdContinue.
==============
*/
int
motd_menu (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	qmenu_t	*m;
	motd_t	*node;
	int		i;

	if (motd.next == NULL)
	{
		ent->client->showmenu = false;

		m = CreateQMenu (ent, "Choose your team");
		AddMenuItem (m, "Start New Team", NULL, -1, menuNewTeam);

		for (i = 0; i <= 255; i++)
		{
			if (teams[i])
				AddMenuItem (m, teams[i]->name, va (" Players: %d", count_queue (teams[i]->members)), -1, menuAddtoTeam);
		}

		AddMenuItem (m, "Refresh List", NULL, -1, menuRefreshTeamList);
		AddMenuItem (m, "", NULL, -1, NULL);
		AddMenuItem (m, "Confused? try /cmd menuhelp", NULL, -1, NULL);

		FinishMenu (ent, m, 1);

		return 0;
	}

	m = CreateQMenu (ent, "Message of the Day");
	AddMenuItem (m, "---------Continue----------", NULL, -1, menuMotdContinue);

	for (node = motd.next; node; node = node->next)
		AddMenuItem (m, node->line, NULL, -1, NULL);

	FinishMenu (ent, m, 1);

	return 0;
}

/*
==============
menuNo
==============
*/
int
menuNo (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	return 0;
}

/*
==============
menuTeamConfirm
==============
*/
int
menuTeamConfirm (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	menuitem_t	*it;

	it = item->data;
	AddtoArena (ent, it->num, ent->client->teamnum);

	return 1;
}

/*
==============
show_teamconfirm_menu

Shown instead of joining directly when a team doesn't have enough
players yet, to make sure the player really wants to start the round
short-handed.
==============
*/
void
show_teamconfirm_menu (edict_t *ent, int arenanum)
{
	qmenu_t	*m;

	m = CreateQMenu (ent, "Confirmation");

	AddMenuItem (m, "You have too few players", NULL, -1, NULL);
	AddMenuItem (m, "Do you wish to continue?", NULL, -1, NULL);
	AddMenuItem (m, "", NULL, -1, NULL);

	AddMenuItem (m, "Yes, continue to arena ", NULL, arenanum, menuTeamConfirm);
	AddMenuItem (m, "No, choose another", NULL, -1, menuNo);

	FinishMenu (ent, m, 1);
}

/*
==============
menu_centerprint

Wraps a (possibly multi-line) message across several menu item lines
and pops it up as a dismissible menu, unless a menu is already up, in
which case it's just centerprinted normally.  If the player happens
to be sitting on the message-of-the-day screen, that gets dismissed
first so the new message isn't lost behind it.
==============
*/
void
menu_centerprint (edict_t *ent, char *message)
{
	qmenu_t		*m;
	menuinfo_t	*info;
	char		buf[128];
	char		*dst;
	char		*lastbreak;
	char		*src;
	int			linelen;

	if (!ent->client->showmenu)
	{
		gi.centerprintf (ent, message);
		return;
	}

	if (ent->client->menu != NULL)
	{
		info = ent->client->menu->data;

		if (!strncmp (info->title, "Message", 8))
		{
			ent->client->menuusetime = 0;
			UseMenu (ent, 1);
		}
	}

	m = CreateQMenu (ent, message);
	AddMenuItem (m, "---------Continue----------", NULL, -1, menuNo);

	src = message;
	dst = buf;
	lastbreak = NULL;
	linelen = 0;

	while (*src)
	{
		*dst = *src++;

		if (*dst == ' ' || *dst == '\n')
			lastbreak = dst;

		if (*dst == '\n')
		{
			*lastbreak = '\0';
			AddMenuItem (m, buf, NULL, -1, NULL);
			dst = buf;
			lastbreak = NULL;
			linelen = 0;
			continue;
		}

		dst++;
		linelen++;

		if (linelen > 26)
		{
			if (lastbreak != NULL)
			{
				int		remainder;

				*lastbreak = '\0';
				AddMenuItem (m, buf, NULL, -1, NULL);

				remainder = dst - (lastbreak + 1);
				memmove (buf, lastbreak + 1, remainder);
				dst = buf + remainder;
			}
			else
			{
				*dst = '\0';
				AddMenuItem (m, buf, NULL, -1, NULL);
				dst = buf;
			}

			lastbreak = NULL;
			linelen = 0;
		}
	}

	*dst = '\0';
	AddMenuItem (m, buf, NULL, -1, NULL);

	FinishMenu (ent, m, 1);
}
