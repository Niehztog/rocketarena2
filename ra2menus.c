#include "g_local.h"
#include "arena.h"

char	*get_next_map (char *current);

void	Cmd_arenaadmin_f (edict_t *ent, unsigned mode);
void	menu_centerprint (edict_t *ent, char *message);
int		menuNo (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg);

/* gamex86.dll 0x200293d0-0x20029400 (manual-confirmed) */
/* gamei386.so 0x00051c44-0x00051c7e */
char *
StringForProtect (int protect)
{
	switch (protect)
	{
	case 0:
		return "Damage all          ";
	case 1:
		return "Dont damage team    ";
	case 2:
		return "Damage self not team";
	default:
		return "Damage all          ";
	}
}

/* gamex86.dll 0x20029400-0x200294d0 (unpadded-prologue+size) */
/* gamei386.so 0x00051c80-0x00051cd7 */
int
NumForProtect (char *s)
{
	if (!strcmp (s, "Damage all          "))
		return 0;
	if (!strcmp (s, "Dont damage team    "))
		return 1;
	if (!strcmp (s, "Damage self not team"))
		return 2;

	return 0;
}

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x00051cd8-0x00051cde */
int
menuDoNothing (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	return 2;
}

/* gamex86.dll 0x200294d0-0x20029560 (manual-confirmed) */
/* gamei386.so 0x00051ce0-0x00051d80 */
int
menuLeaveArena (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	int		*state;

	state = &arenas[((team_t *)teams[ent->client->resp.teamnum].it)->arenanum].state;

	if (*state != ASTATE_COUNTDOWN
		&& *state != ASTATE_ROUNDEND
		&& ent->takedamage)
	{
		menu_centerprint (ent, "Sorry, you cannot leave the arena\nduring a match");
		return 2;
	}

	remove_from_queue (&((team_t *)teams[ent->client->resp.teamnum].it)->arenalink, NULL);
	SendTeamToArena (&teams[ent->client->resp.teamnum], 0, 1, 1);

	return 0;
}

/* gamex86.dll 0x20029560-0x200295b3 (manual-confirmed) */
/* gamei386.so 0x00051d80-0x00051dd0 */
int
menuAddtoArena (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	qmenu_t		*node;
	int			arenanum;

	arenanum = 0;

	node = (qmenu_t *)menu->it;

	while (node->next)
	{
		arenanum++;
		node = node->next;
		if (node == item)
			break;
	}

	if (arenanum)
	{
		if (arg == 1)
			return AddtoArena (ent, arenanum, 0, 0);

		return AddtoArena (ent, arenanum, 1, 1);
	}

	return 0;
}

/* gamex86.dll 0x200295c0-0x2002963b (manual-confirmed) */
/* gamei386.so 0x00051dd0-0x00051e45 */
int
menuLeaveTeamAr (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	team_t	*team;
	int		*state;

	team = teams[ent->client->resp.teamnum].it;
	state = &arenas[team->arenanum].state;

	if (*state != ASTATE_COUNTDOWN
		&& *state != ASTATE_ROUNDEND
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

/* gamex86.dll 0x20029640-0x20029660 (shape-matched(ratio=1.00)) */
/* gamei386.so 0x00051e48-0x00051e60 */
int
menuLeaveTeam (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	remove_from_team (ent);
	init_player (ent);

	return 0;
}

/* gamex86.dll 0x20029660-0x200296b9 (manual-confirmed) */
/* gamei386.so 0x00051e60-0x00051f2d */
int
menuStepInOutofLine (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	int		arenanum;
	int		wasinline;

	arenanum = ent->client->resp.context;
	wasinline = (((team_t *)teams[ent->client->resp.teamnum].it)->outofline == 0);

	if (!menuLeaveArena (ent, NULL, NULL, 0))
		return AddtoArena (ent, arenanum, 1, wasinline);

	return 2;
}

/* gamex86.dll 0x200296c0-0x20029714 (padded+majority+size-corrected) */
/* gamei386.so 0x00051f30-0x00051f70 */
char *
getarenaname (int arenanum)
{
	edict_t	*spot=NULL;

	while ((spot = G_Find (spot, FOFS(classname), "info_player_intermission")) != NULL)
		if (spot->arena == arenanum)
			return spot->message;

	return va ("Arena Number %d", arenanum);
}

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x00051f70-0x00051f83 */
int
menuChangeOMode (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	ChangeOMode (ent);

	return 2;
}

/* gamex86.dll 0x20029720-0x200297e0 (padded+majority) */
/* gamei386.so 0x00051f84-0x000520a4 */
int
menuShowSettingsPropose (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	if (arenas[ent->client->resp.context].proposetime > level.time)
	{
		if ((int) (arenas[ent->client->resp.context].proposetime - level.time) < 30)
			menu_centerprint (ent, va ("Voting is in progress.\nPlease wait %d seconds",
				(int) (arenas[ent->client->resp.context].proposetime - level.time)));
		else
			menu_centerprint (ent, "Voting is in progress.\nPlease wait");

		return 2;
	}

	if (ent->client->resp.votes == 0)
	{
		menu_centerprint (ent, va ("Sorry, you cannot propose any more changes.\nYou have already proposed %d times\n", votetries_setting));
		return 2;
	}

	ent->client->resp.votes--;
	Cmd_arenaadmin_f (ent, 1);

	return 2;
}

/* gamex86.dll 0x200297e0-0x20029860 (padded+majority) */
/* gamei386.so 0x000520a4-0x00052106 */
int
menuShowSettingsVote (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{


	if (arenas[ent->client->resp.context].proposetime < level.time)
	{
		menu_centerprint (ent, "No changes have been proposed");
		return 2;
	}

	if (ent->client->resp.voted)
	{
		menu_centerprint (ent, "You have already voted");
		return 2;
	}

	Cmd_arenaadmin_f (ent, 2);

	return 2;
}

/* gamex86.dll 0x20029860-0x200299a0 (padded+majority) */
/* gamei386.so 0x00052108-0x00052241 */
void
show_observer_menu (edict_t *ent)
{
	qmenu_t	*m;

	m = CreateQMenu (ent, "Observer Options");

	if (!((team_t *)teams[ent->client->resp.teamnum].it)->outofline)
	{
		AddMenuItem (m, "Change Arena Settings", NULL, -1, menuShowSettingsPropose);
		AddMenuItem (m, "Vote on Changes", NULL, -1, menuShowSettingsVote);
		AddMenuItem (m, "", NULL, -1, NULL);
	}

	if (!arenas[ent->client->resp.context].idarena)
	{
		AddMenuItem (m, va ("Step %s Line",
			((team_t *)teams[ent->client->resp.teamnum].it)->outofline ? "into" : "out of"),
			NULL, -1, menuStepInOutofLine);
		AddMenuItem (m, "", NULL, -1, NULL);
	}

	AddMenuItem (m, "Leave Team", NULL, -1, menuLeaveTeamAr);

	if (!arenas[ent->client->resp.context].idarena)
		AddMenuItem (m, "Leave Arena", NULL, -1, menuLeaveArena);

	FinishMenu (ent, m, 0);
}

/* gamex86.dll 0x200299a0-0x20029a60 (padded+majority) */
/* gamei386.so 0x00052244-0x00052397 */
void
show_arena_menu (edict_t *ent)
{
	qmenu_t	*m;
	int		i;

	m = CreateQMenu (ent, "Choose Your Arena");

	for (i = 1; i <= num_arenas; i++)
	{
		if (arenas[i].idarena)
			AddMenuItem (m, getarenaname (i), " (PT)", -1, menuAddtoArena);
		else
			AddMenuItem (m, getarenaname (i), " T:",
				count_queue (&arenas[i].waitingteams) + count_queue (&arenas[i].activeteams),
				menuAddtoArena);
	}

	AddMenuItem (m, "", NULL, -1, NULL);
	AddMenuItem (m, "Leave Team", NULL, -1, menuLeaveTeam);

	FinishMenu (ent, m, 1);
}

/* gamex86.dll 0x20029a60-0x20029b00 (padded) */
/* gamei386.so 0x00052398-0x0005242c */
int
menuAddtoTeam (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	if (add_to_team (ent, ((menuitem_t *)item->it)->text))
	{
		if (!((team_t *)teams[ent->client->resp.teamnum].it)->arenanum)
			show_arena_menu (ent);
		else
		{
			ent->client->resp.fightstate = FIGHT_SPECTATING;
			ent->takedamage = 0;
			move_to_arena (ent, ((team_t *)teams[ent->client->resp.teamnum].it)->arenanum, 1);
		}

		return 0;
	}

	menu_centerprint (ent, "That team is already in an arena\nand full or\nthe arena is locked");

	return 2;
}

/* gamex86.dll 0x20029b00-0x20029bc0 (padded) */
/* gamei386.so 0x0005242c-0x000524b7 */
int
menuNewTeam (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	char	*name;
	int		i = 0;

	name = gi.TagMalloc (100, TAG_LEVEL);

	Com_sprintf (name, 100, "%s's Team", ent->client->pers.netname);

	while (i < 256)
	{
		if (teams[i].it && !strcmp (((team_t *)teams[i].it)->name, name))
		{
			strcat (name, "!");
			i = 0;
			continue;
		}

		i++;
	}

	add_to_team (ent, name);
	show_arena_menu (ent);

	return 0;
}

/* gamex86.dll 0x20029bc0-0x20029c80 (shape-matched(ratio=0.82)) */
/* gamei386.so 0x000524b8-0x000525b6 */
int
menuRefreshTeamList (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	qmenu_t	*m;
	int		i;

	m = CreateQMenu (ent, "Choose your team");
	AddMenuItem (m, "Start New Team", NULL, -1, menuNewTeam);

	for (i = 0; i < MAX_TEAMS; i++)
	{
		if (teams[i].it)
			AddMenuItem (m, ((team_t *)teams[i].it)->name, " Players: ", count_queue (&teams[i]), menuAddtoTeam);
	}

	AddMenuItem (m, "Refresh List", NULL, -1, menuRefreshTeamList);
	AddMenuItem (m, "", NULL, -1, NULL);
	AddMenuItem (m, "Confused? try /cmd menuhelp", NULL, -1, NULL);

	FinishMenu (ent, m, 1);

	return 2;
}

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x000525b8-0x000525e6 */
int
menuChangeValueAZ (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	if (arg)
		((menuitem_t *)item->it)->num++;
	else
		((menuitem_t *)item->it)->num--;

	if (((menuitem_t *)item->it)->num < 0)
		((menuitem_t *)item->it)->num = 0;

	return 1;
}

/* gamex86.dll 0x20029c80-0x20029cb2 (manual-confirmed) */
/* gamei386.so 0x000525e8-0x00052617 */
int
menuChangeValue10AZ (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	if (arg)
		((menuitem_t *)item->it)->num += 10;
	else
		((menuitem_t *)item->it)->num -= 10;

	if (((menuitem_t *)item->it)->num < 0)
		((menuitem_t *)item->it)->num = 0;

	return 1;
}

/* gamex86.dll 0x20029cc0-0x20029cf2 (manual-confirmed) */
/* gamei386.so 0x00052618-0x00052647 */
int
menuChangeValue50AZ (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	if (arg)
		((menuitem_t *)item->it)->num += 50;
	else
		((menuitem_t *)item->it)->num -= 50;

	if (((menuitem_t *)item->it)->num < 0)
		((menuitem_t *)item->it)->num = 0;

	return 1;
}

/* gamex86.dll 0x20029d00-0x20029d34 (manual-confirmed) */
/* gamei386.so 0x00052648-0x00052677 */
int
menuChangeValue50 (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	if (arg)
		((menuitem_t *)item->it)->num += 50;
	else
		((menuitem_t *)item->it)->num -= 50;

	if (((menuitem_t *)item->it)->num <= 0)
		((menuitem_t *)item->it)->num = 50;

	return 1;
}

/* gamex86.dll 0x20029d40-0x20029d6e (manual-confirmed) */
/* gamei386.so 0x00052678-0x000526a6 */
int
menuChangeValue (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	if (arg)
		((menuitem_t *)item->it)->num++;
	else
		((menuitem_t *)item->it)->num--;

	if (((menuitem_t *)item->it)->num == 0)
		((menuitem_t *)item->it)->num = 1;

	return 1;
}

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x000526a8-0x000526d7 */
int
menuChangeValue10 (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	if (arg)
		((menuitem_t *)item->it)->num += 10;
	else
		((menuitem_t *)item->it)->num -= 10;

	if (((menuitem_t *)item->it)->num <= 0)
		((menuitem_t *)item->it)->num = 10;

	return 1;
}

/* gamex86.dll 0x20029d70-0x20029d9a (manual-confirmed) */
/* gamei386.so 0x000526d8-0x000526fd */
int
menuChangeYesNo (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	if (((menuitem_t *)item->it)->value[0] == 'Y')
		strcpy (((menuitem_t *)item->it)->value, "NO ");
	else
		strcpy (((menuitem_t *)item->it)->value, "YES");

	return 1;
}

/* gamex86.dll 0x20029da0-0x20029e10 (shape-matched(ratio=0.78)) */
/* gamei386.so 0x00052700-0x000527c1 */
int
menuChangeProtect (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	int			protect;

	protect = NumForProtect (((menuitem_t *)item->it)->value);

	if (arg)
		protect++;
	else
		protect--;

	if (protect < 0)
		protect = 2;
	if (protect > 2)
		protect = 0;

	strcpy (((menuitem_t *)item->it)->value, StringForProtect (protect));

	return 1;
}

/* gamex86.dll 0x20029e10-0x20029e50 (aligned) */
/* gamei386.so 0x000527c4-0x000527ea */
int
menuChangeMap (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	strcpy (((menuitem_t *)item->it)->value,
		get_next_map (((menuitem_t *)item->it)->value));

	return 1;
}

/* gamex86.dll 0x20029e50-0x20029e90 (shape-matched(ratio=1.00)) */
/* gamei386.so 0x000527ec-0x00052827 */
void
cvar_setvalue (char *name, int value)
{
	char	buf[256];

	sprintf (buf, "%d", value);
	gi.cvar_set (name, buf);
}

/* gamex86.dll 0x20029e90-0x20029f80 (padded+majority+collision-resolved) */
/* gamei386.so 0x00052828-0x00052926 */
int
menuApplyAdmin (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	qmenu_t		*node;
	menuitem_t	*it;
	edict_t		*e;
	char		*map;

	node = (qmenu_t *)menu->it;

	while (node->next)
	{
		node = node->next;
		it = node->it;

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

/* gamex86.dll 0x2002b270-0x2002b280 (manual-confirmed) */
/* gamei386.so 0x00052928-0x0005292b */
int
menuCancel (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	return 0;
}

/* gamex86.dll 0x20029f80-0x20029fa0 (padded) */
/* gamei386.so 0x0005292c-0x00052943 */
void
Cmd_menuhelp_f (edict_t *ent)
{
	gi.cprintf (ent, PRINT_HIGH, "H| Use invprev and invnext ([ and ])\nH| to navigate the menu\nH| invuse (ENTER) selects\nH| inven (TAB) toggles it on/off\n");
}

/* gamex86.dll 0x20029fa0-0x2002a0e0 (padded+majority) */
/* gamei386.so 0x00052944-0x00052ab4 */
void
Cmd_admin_f (edict_t *ent)
{
	int			code;
	qmenu_t		*m, *mi;

	if (admincode->value == 0)
		return;

	code = atoi (gi.argv (1));

	if ((float) code == admincode->value)
	{
		m = CreateQMenu (ent, "Admin Menu");

		AddMenuItem (m, "Fraglimit:        ", NULL, (int) fraglimit->value, menuChangeValue10AZ);
		AddMenuItem (m, "Timelimit:        ", NULL, (int) timelimit->value, menuChangeValue10AZ);
		mi = AddMenuItem (m, "Mapname:          ",
			"                                ", -1, menuChangeMap);
		strcpy (((menuitem_t *)mi->it)->value, level.mapname);

		AddMenuItem (m, "", NULL, -1, NULL);
		AddMenuItem (m, "Apply", NULL, -1, menuApplyAdmin);
		AddMenuItem (m, "Cancel", NULL, -1, menuCancel);

		FinishMenu (ent, m, 1);
	}
	else
		gi.cprintf (ent, PRINT_HIGH, "Sorry, incorrect admin code\n");
}

/* gamex86.dll 0x2002a0e0-0x2002a6b0 (manual-confirmed) */
/* gamei386.so 0x00052ab4-0x000530e1 */
int
menuApplyArenaAdmin (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	qmenu_t		*node;
	menuitem_t	*it;
	int			*settings;
	int			arenanum;
	int			weapons;

	node = (qmenu_t *)menu->it;

	while (node->next)
	{
		node = node->next;
		it = node->it;

		if (!Q_stricmp (it->text, "Arena:                 "))
		{
			arenanum = it->num;

			if (((menuitem_t *)item->it)->text[0] == 'A')
			{
				settings = &arenas[arenanum].playersperteam;
			}
			else
			{
				if (arenas[arenanum].proposetime > level.time)
				{
					menu_centerprint (ent,
						va ("Voting is in progress.\nPlease wait %d seconds",
						(int) (arenas[ent->client->resp.context].proposetime - level.time)));
					return 2;
				}

				memcpy (&arenas[arenanum].proposed, &arenas[arenanum].playersperteam,
					sizeof (arena_settings_t));
				settings = &arenas[arenanum].proposed.playersperteam;
				start_voting (ent, arenanum);
				arenas[arenanum].votes_yes++;
				ent->client->resp.voted = true;
			}

			settings[41] = 1;
			weapons = 0;

			if (!settings[28])
				weapons |= (settings[2] & weapon_vals[0]) ? weapon_vals[0] : 0;
			if (!settings[29])
				weapons |= (settings[2] & weapon_vals[1]) ? weapon_vals[1] : 0;
			if (!settings[30])
				weapons |= (settings[2] & weapon_vals[2]) ? weapon_vals[2] : 0;
			if (!settings[31])
				weapons |= (settings[2] & weapon_vals[3]) ? weapon_vals[3] : 0;
			if (!settings[32])
				weapons |= (settings[2] & weapon_vals[4]) ? weapon_vals[4] : 0;
			if (!settings[33])
				weapons |= (settings[2] & weapon_vals[5]) ? weapon_vals[5] : 0;
			if (!settings[34])
				weapons |= (settings[2] & weapon_vals[6]) ? weapon_vals[6] : 0;
			if (!settings[35])
				weapons |= (settings[2] & weapon_vals[7]) ? weapon_vals[7] : 0;
			if (!settings[36])
				weapons |= (settings[2] & weapon_vals[8]) ? weapon_vals[8] : 0;

			settings[2] = weapons;
		}
		else if (!Q_stricmp (it->text, "Players per team:      "))
		{
			settings[0] = it->num;
		}
		else if (!Q_stricmp (it->text, "Initial Health:        "))
		{
			settings[4] = it->num;
		}
		else if (!Q_stricmp (it->text, "Initial Armor:         "))
		{
			settings[3] = it->num;
		}
		else if (!Q_stricmp (it->text, "Minimum Ping:          "))
		{
			settings[5] = it->num;
		}
		else if (!Q_stricmp (it->text, "Maximum Ping:          "))
		{
			settings[6] = it->num;
		}
		else if (!Q_stricmp (it->text, "Rounds:                "))
		{
			settings[1] = (it->num / 2) * 2 + 1;
		}
		else if (!Q_stricmp (it->text, "Allow Shotgun:         "))
		{
			settings[2] |= (it->value[0] == 'Y') ? weapon_vals[0] : 0;
		}
		else if (!Q_stricmp (it->text, "Allow Super Shotgun:   "))
		{
			settings[2] |= (it->value[0] == 'Y') ? weapon_vals[1] : 0;
		}
		else if (!Q_stricmp (it->text, "Allow Machine gun:     "))
		{
			settings[2] |= (it->value[0] == 'Y') ? weapon_vals[2] : 0;
		}
		else if (!Q_stricmp (it->text, "Allow Chain gun:       "))
		{
			settings[2] |= (it->value[0] == 'Y') ? weapon_vals[3] : 0;
		}
		else if (!Q_stricmp (it->text, "Allow Grenade Launcher:"))
		{
			settings[2] |= (it->value[0] == 'Y') ? weapon_vals[4] : 0;
		}
		else if (!Q_stricmp (it->text, "Allow Rocket Launcher: "))
		{
			settings[2] |= (it->value[0] == 'Y') ? weapon_vals[5] : 0;
		}
		else if (!Q_stricmp (it->text, "Allow Hyperblaster:    "))
		{
			settings[2] |= (it->value[0] == 'Y') ? weapon_vals[6] : 0;
		}
		else if (!Q_stricmp (it->text, "Allow Railgun:         "))
		{
			settings[2] |= (it->value[0] == 'Y') ? weapon_vals[7] : 0;
		}
		else if (!Q_stricmp (it->text, "Allow BFG10K:          "))
		{
			settings[2] |= (it->value[0] == 'Y') ? weapon_vals[8] : 0;
		}
		else if (!Q_stricmp (it->text, "Health: "))
		{
			settings[17] = NumForProtect (it->value);
		}
		else if (!Q_stricmp (it->text, "Armor:  "))
		{
			settings[16] = NumForProtect (it->value);
		}
		else if (!Q_stricmp (it->text, "Falling Damage:        "))
		{
			settings[18] = (it->value[0] == 'Y');
		}
		else if (!Q_stricmp (it->text, "Lock Arena:            "))
		{
			settings[38] = (it->value[0] == 'Y');
		}
		else if (!Q_stricmp (it->text, "Competition Mode:      "))
		{
			settings[39] = (it->value[0] == 'Y');
		}
		else if (!Q_stricmp (it->text, "Damage Scoring:        "))
		{
			settings[40] = (it->value[0] == 'Y');
		}
	}

	check_teams (arenanum);

	return 0;
}

/* gamex86.dll 0x2002a6b0-0x2002a760 (padded+majority) */
/* gamei386.so 0x000530e4-0x0005317e */
int
menuVote (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	if (arenas[ent->client->resp.context].proposetime < level.time)
	{
		menu_centerprint (ent, "Sorry, voting is over");
		return 2;
	}

	if (ent->client->resp.voted)
	{
		menu_centerprint (ent, "You have already voted");
		return 2;
	}

	if (((menuitem_t *)item->it)->value[0] == 'Y')
		arenas[ent->client->resp.context].votes_yes++;
	else
		arenas[ent->client->resp.context].votes_no++;

	ent->client->resp.voted = true;

	return 0;
}

/* gamex86.dll 0x2002a760-0x2002b1c0 (manual-confirmed) */
/* gamei386.so 0x00053180-0x00053c89 */
void
Cmd_arenaadmin_f (edict_t *ent, unsigned mode)
{
	qmenu_t			*m;
	menuselect_t	changevalue;
	menuselect_t	changevalue50;
	menuselect_t	changevalue50az;
	menuselect_t	changeyesno;
	menuselect_t	changeprotect;
	int				*vals;
	int				*live;
	int				arenanum = 0;
	int				code;

	switch (mode)
	{
	case 0:
		if (admincode->value == 0)
			return;

		code = atoi (gi.argv (1));
		arenanum = atoi (gi.argv (2));

		if ((float) code != admincode->value)
			return;

		if (!arenanum)
		{
	case 1:
			arenanum = ent->client->resp.context;
		}

		if (arenanum < 1 || arenanum > num_arenas)
			return;

		changevalue = menuChangeValue;
		changevalue50 = menuChangeValue50;
		changevalue50az = menuChangeValue50AZ;
		changeyesno = menuChangeYesNo;
		changeprotect = menuChangeProtect;

		vals = &arenas[arenanum].playersperteam;
		break;

	case 2:
		arenanum = ent->client->resp.context;

		if (arenanum < 1 || arenanum > num_arenas)
			return;

		changevalue50 = NULL;
		changevalue50az = NULL;
		changeprotect = NULL;
		changeyesno = NULL;
		changevalue = NULL;

		vals = (int *) &arenas[arenanum].proposetime + 1;
		live = &arenas[arenanum].playersperteam;

		m = CreateQMenu (ent, "Proposed Changes");
		AddMenuItem (m, "Arena:                 ", NULL, arenanum, NULL);

		if (arenas[arenanum].idarena != 1 && vals[0] != live[0])
			AddMenuItem (m, "Players per team:      ", NULL, vals[0], NULL);
		if (vals[4] != live[4])
			AddMenuItem (m, "Initial Health:        ", NULL, vals[4],changevalue50);
		if (vals[3] != live[3])
			AddMenuItem (m, "Initial Armor:         ", NULL, vals[3],changevalue50az);
		if (arenas[arenanum].idarena != 1)
		{
			if (vals[5] != live[5])
				AddMenuItem (m, "Minimum Ping:          ", NULL, vals[5],changevalue50az);
			if (vals[6] != live[6])
				AddMenuItem (m, "Maximum Ping:          ", NULL, vals[6],changevalue50az);
		}
		if (vals[1] != live[1])
			AddMenuItem (m, "Rounds:                ", NULL, vals[1],changevalue);
		if ((vals[2] & weapon_vals[0]) != (live[2] & weapon_vals[0]))
			AddMenuItem (m, "Allow Shotgun:         ", (vals[2] & weapon_vals[0]) ? "YES" : "NO ", -1,changeyesno);
		if ((vals[2] & weapon_vals[1]) != (live[2] & weapon_vals[1]))
			AddMenuItem (m, "Allow Super Shotgun:   ", (vals[2] & weapon_vals[1]) ? "YES" : "NO ", -1,changeyesno);
		if ((vals[2] & weapon_vals[2]) != (live[2] & weapon_vals[2]))
			AddMenuItem (m, "Allow Machine gun:     ", (vals[2] & weapon_vals[2]) ? "YES" : "NO ", -1,changeyesno);
		if ((vals[2] & weapon_vals[3]) != (live[2] & weapon_vals[3]))
			AddMenuItem (m, "Allow Chain gun:       ", (vals[2] & weapon_vals[3]) ? "YES" : "NO ", -1,changeyesno);
		if ((vals[2] & weapon_vals[4]) != (live[2] & weapon_vals[4]))
			AddMenuItem (m, "Allow Grenade Launcher:", (vals[2] & weapon_vals[4]) ? "YES" : "NO ", -1,changeyesno);
		if ((vals[2] & weapon_vals[5]) != (live[2] & weapon_vals[5]))
			AddMenuItem (m, "Allow Rocket Launcher: ", (vals[2] & weapon_vals[5]) ? "YES" : "NO ", -1,changeyesno);
		if ((vals[2] & weapon_vals[6]) != (live[2] & weapon_vals[6]))
			AddMenuItem (m, "Allow Hyperblaster:    ", (vals[2] & weapon_vals[6]) ? "YES" : "NO ", -1,changeyesno);
		if ((vals[2] & weapon_vals[7]) != (live[2] & weapon_vals[7]))
			AddMenuItem (m, "Allow Railgun:         ", (vals[2] & weapon_vals[7]) ? "YES" : "NO ", -1,changeyesno);
		if ((vals[2] & weapon_vals[8]) != (live[2] & weapon_vals[8]))
			AddMenuItem (m, "Allow BFG10K:          ", (vals[2] & weapon_vals[8]) ? "YES" : "NO ", -1,changeyesno);
		if (vals[17] != live[17])
			AddMenuItem (m, "Health: ", StringForProtect (vals[17]), -1,changeprotect);
		if (vals[16] != live[16])
			AddMenuItem (m, "Armor:  ", StringForProtect (vals[16]), -1,changeprotect);
		if (vals[18] != live[18])
			AddMenuItem (m, "Falling Damage:        ", vals[18] ? "YES" : "NO ", -1,changeyesno);
		if (vals[39] != live[39])
			AddMenuItem (m, "Competition Mode:      ", vals[39] ? "YES" : "NO ", -1,changeyesno);
		if (vals[40] != live[40])
			AddMenuItem (m, "Damage Scoring:        ", vals[40] ? "YES" : "NO ", -1,changeyesno);

		AddMenuItem (m, "", NULL, -1, NULL);
		break;

	}

	if (mode != 2)
	{
		m = CreateQMenu (ent, "Arena Admin Menu");
		AddMenuItem (m, "Arena:                 ", NULL, arenanum, NULL);

		if (arenas[arenanum].idarena != 1)
		{
			if (!mode || vals[23])
				AddMenuItem (m, "Players per team:      ", NULL, vals[0], changevalue);
		}

		if (!mode || vals[20])
			AddMenuItem (m, "Initial Health:        ", NULL, vals[4], changevalue50);
		if (!mode || vals[19])
			AddMenuItem (m, "Initial Armor:         ", NULL, vals[3], changevalue50az);

		if (arenas[arenanum].idarena != 1)
		{
			if (!mode || vals[21])
				AddMenuItem (m, "Minimum Ping:          ", NULL, vals[5], changevalue50az);
			if (!mode || vals[22])
				AddMenuItem (m, "Maximum Ping:          ", NULL, vals[6], changevalue50az);
		}

		if (!mode || vals[24])
			AddMenuItem (m, "Rounds:                ", NULL, vals[1], changevalue);
		if (!mode || vals[28])
			AddMenuItem (m, "Allow Shotgun:         ", (vals[2] & weapon_vals[0]) ? "YES" : "NO ", -1, changeyesno);
		if (!mode || vals[29])
			AddMenuItem (m, "Allow Super Shotgun:   ", (vals[2] & weapon_vals[1]) ? "YES" : "NO ", -1, changeyesno);
		if (!mode || vals[30])
			AddMenuItem (m, "Allow Machine gun:     ", (vals[2] & weapon_vals[2]) ? "YES" : "NO ", -1, changeyesno);
		if (!mode || vals[31])
			AddMenuItem (m, "Allow Chain gun:       ", (vals[2] & weapon_vals[3]) ? "YES" : "NO ", -1, changeyesno);
		if (!mode || vals[32])
			AddMenuItem (m, "Allow Grenade Launcher:", (vals[2] & weapon_vals[4]) ? "YES" : "NO ", -1, changeyesno);
		if (!mode || vals[33])
			AddMenuItem (m, "Allow Rocket Launcher: ", (vals[2] & weapon_vals[5]) ? "YES" : "NO ", -1, changeyesno);
		if (!mode || vals[34])
			AddMenuItem (m, "Allow Hyperblaster:    ", (vals[2] & weapon_vals[6]) ? "YES" : "NO ", -1, changeyesno);
		if (!mode || vals[35])
			AddMenuItem (m, "Allow Railgun:         ", (vals[2] & weapon_vals[7]) ? "YES" : "NO ", -1, changeyesno);
		if (!mode || vals[36])
			AddMenuItem (m, "Allow BFG10K:          ", (vals[2] & weapon_vals[8]) ? "YES" : "NO ", -1, changeyesno);
		if (!mode || vals[27])
			AddMenuItem (m, "Health: ", StringForProtect (vals[17]), -1, changeprotect);
		if (!mode || vals[26])
			AddMenuItem (m, "Armor:  ", StringForProtect (vals[16]), -1, changeprotect);
		if (!mode || vals[37])
			AddMenuItem (m, "Falling Damage:        ", vals[18] ? "YES" : "NO ", -1, changeyesno);
		if (!mode)
			AddMenuItem (m, "Lock Arena:            ", vals[38] ? "YES" : "NO ", -1, changeyesno);
		AddMenuItem (m, "Competition Mode:      ", vals[39] ? "YES" : "NO ", -1, changeyesno);
		AddMenuItem (m, "Damage Scoring:        ", vals[40] ? "YES" : "NO ", -1, changeyesno);
		AddMenuItem (m, "", NULL, -1, NULL);
	}

	switch (mode)
	{
	case 0:
		AddMenuItem (m, "Apply", NULL, -1, menuApplyArenaAdmin);
	case 1:
		AddMenuItem (m, "Propose", NULL, -1, menuApplyArenaAdmin);
		break;
	case 2:
		AddMenuItem (m, "Vote ", "Yes", -1, menuVote);
		AddMenuItem (m, "Vote ", "No", -1, menuVote);
		break;
	}

	AddMenuItem (m, "Cancel", NULL, -1, menuCancel);
	FinishMenu (ent, m, 1);
}

/* gamex86.dll 0x2002b1c0-0x2002b1f0 (manual-confirmed) */
/* gamei386.so 0x00053c8c-0x00053d9b */
int
menuMotdContinue (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	ent->client->pers.showmotd = false;
	menuRefreshTeamList (ent, NULL, NULL, 0);

	return 0;
}

/* gamex86.dll 0x2002b1f0-0x2002b270 (manual-confirmed) */
/* gamei386.so 0x00053d9c-0x00053f20 */
void
motd_menu (edict_t *ent)
{
	qmenu_t	*m;
	motd_t	*node;

	if (motd.next == NULL)
	{
		menuMotdContinue (ent, NULL, NULL, 0);
		return;
	}

	m = CreateQMenu (ent, "Message of the Day");
	AddMenuItem (m, "---------Continue----------", NULL, -1, menuMotdContinue);

	node = &motd;
	while (node->next)
	{
		node = node->next;
		AddMenuItem (m, node->line, NULL, -1, NULL);
	}

	FinishMenu (ent, m, 1);
}

/* gamex86.dll 0x2002b270-0x2002b280 (bracketed) */
/* gamei386.so 0x00053f20-0x00053f23 */
int
menuNo (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	return 0;
}

/* gamex86.dll 0x2002b280-0x2002b2b0 (bracketed) */
/* gamei386.so 0x00053f24-0x00053f45 */
int
menuTeamConfirm (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	AddtoArena (ent, ((menuitem_t *)item->it)->num, 1, 0);

	return 1;
}

/* gamex86.dll 0x2002b2b0-0x2002b340 (padded) */
/* gamei386.so 0x00053f48-0x00053fd0 */
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

/* gamex86.dll 0x2002b340-0x2002b4e0 (padded+majority) */
/* gamei386.so 0x00053fd0-0x00054142 */
void
menu_centerprint (edict_t *ent, char *message)
{
	qmenu_t		*m;
	menuinfo_t	*info;
	char		*dst;
	char		*src;
	char		*line;
	char		*lastspace;
	int			linelen;
	char		c;
	char		buf[2048];

	src = message;
	dst = buf;
	lastspace = NULL;
	line = buf;
	linelen = 0;

	if (!ent->client->showmenu)
	{
		gi.centerprintf (ent, message);
		return;
	}

	m = ent->client->curmenulink;
	if (m != NULL)
	{
		info = m->it;

		if (!strcmp (info->title, "Message"))
		{
			ent->client->menuusetime = 0;
			UseMenu (ent, 1);
		}
	}

	m = CreateQMenu (ent, "Message");
	AddMenuItem (m, "---------Continue----------", NULL, -1, menuNo);

	while ((c = *src++) != 0)
	{
		*dst++ = c;
		linelen++;

		if (c == ' ' || c == '\n')
		{
			lastspace = dst - 1;
			*lastspace = ' ';
		}

		if (linelen >= 27)
		{
			if (lastspace)
				*lastspace = '\0';
			else
				*dst = '\0';

			AddMenuItem (m, line, NULL, -1, NULL);
			linelen -= strlen (line);

			if (lastspace)
				line = lastspace + 1;
			else
				line = dst;
		}
	}

	*dst = '\0';
	AddMenuItem (m, line, NULL, -1, NULL);

	FinishMenu (ent, m, 1);
}
