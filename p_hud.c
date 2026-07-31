#include "g_local.h"
#include "arena.h"

void	Serverwide_ScoreboardMessage (edict_t *ent);
void	Arena_ScoreboardMessage (edict_t *ent);
void	Pickup_ScoreboardMessage (edict_t *ent);

/*
======================================================================

INTERMISSION

======================================================================
*/

void MoveClientToIntermission (edict_t *ent)
{
	clear_menus (ent);

	if (deathmatch->value || coop->value)
		ent->client->showscores = true;
	VectorCopy (level.intermission_origin, ent->s.origin);
	ent->client->ps.pmove.origin[0] = level.intermission_origin[0]*8;
	ent->client->ps.pmove.origin[1] = level.intermission_origin[1]*8;
	ent->client->ps.pmove.origin[2] = level.intermission_origin[2]*8;
	VectorCopy (level.intermission_angle, ent->client->ps.viewangles);
	ent->client->ps.pmove.pm_type = PM_FREEZE;
	ent->client->ps.gunindex = 0;
	ent->client->ps.blend[3] = 0;
	ent->client->ps.rdflags &= ~RDF_UNDERWATER;

	// clean up powerup info
	ent->client->quad_framenum = 0;
	ent->client->invincible_framenum = 0;
	ent->client->breather_framenum = 0;
	ent->client->enviro_framenum = 0;
	ent->client->grenade_blew_up = false;
	ent->client->grenade_time = 0;

	ent->viewheight = 0;
	ent->s.modelindex = 0;
	ent->s.modelindex2 = 0;
	ent->s.modelindex3 = 0;
	ent->s.modelindex = 0;
	ent->s.effects = 0;
	ent->s.sound = 0;
	ent->solid = SOLID_NOT;

	// add the layout

	if (deathmatch->value || coop->value)
	{
		DeathmatchScoreboardMessage (ent, NULL);
		gi.unicast (ent, true);
	}

}

void BeginIntermission (edict_t *targ)
{
	int		i, n;
	edict_t	*ent, *client;

	if (level.intermissiontime)
		return;		// already activated

	game.autosaved = false;

	// respawn any dead clients
	for (i=0 ; i<maxclients->value ; i++)
	{
		client = g_edicts + 1 + i;
		if (!client->inuse)
			continue;
		if (client->health <= 0)
			respawn(client);
	}

	level.intermissiontime = level.time;
	level.changemap = targ->map;

	if (strstr(level.changemap, "*"))
	{
		if (coop->value)
		{
			for (i=0 ; i<maxclients->value ; i++)
			{
				client = g_edicts + 1 + i;
				if (!client->inuse)
					continue;
				// strip players of all keys between units
				for (n = 0; n < MAX_ITEMS; n++)
				{
					if (itemlist[n].flags & IT_KEY)
						client->client->pers.inventory[n] = 0;
				}
			}
		}
	}
	else
	{
		if (!deathmatch->value)
		{
			level.exitintermission = 1;		// go immediately to the next level
			return;
		}
	}

	level.exitintermission = 0;

	// find an intermission spot
	ent = G_Find (NULL, FOFS(classname), "info_player_intermission");
	if (!ent)
	{	// the map creator forgot to put in an intermission point...
		ent = G_Find (NULL, FOFS(classname), "info_player_start");
		if (!ent)
			ent = G_Find (NULL, FOFS(classname), "info_player_deathmatch");
	}
	else
	{	// chose one of four spots
		i = rand() & 3;
		while (i--)
		{
			ent = G_Find (ent, FOFS(classname), "info_player_intermission");
			if (!ent)	// wrap around the list
				ent = G_Find (ent, FOFS(classname), "info_player_intermission");
		}
	}

	VectorCopy (ent->s.origin, level.intermission_origin);
	VectorCopy (ent->s.angles, level.intermission_angle);

	// move all clients to the intermission point
	for (i=0 ; i<maxclients->value ; i++)
	{
		client = g_edicts + 1 + i;
		if (!client->inuse)
			continue;
		MoveClientToIntermission (client);
	}

	// close out the online stats session for every arena that had one running
	for (i=0 ; i<=num_arenas ; i++)
	{
		if (!arenas[i].statsptr)
			continue;
		SendGameSnapShot (arenas[i].statsptr, 0, 1);
		FreeGame (arenas[i].statsptr);
		arenas[i].statsptr = 0;
	}
}


/*
==================
DeathmatchScoreboardMessage

==================
*/
void DeathmatchScoreboardMessage (edict_t *ent, edict_t *killer)
{
	char	entry[1024];
	char	string[1400];
	int		stringlength;
	int		i, j, k;
	int		sorted[MAX_CLIENTS];
	int		sortedscores[MAX_CLIENTS];
	int		score, total;
	int		picnum;
	int		x, y;
	gclient_t	*cl;
	edict_t		*cl_ent;
	char	*tag;

	// a player can ask to see the serverwide scoreboard even while
	// sitting in an arena; once picked it stays picked
	if (!ent->client->arenanum && ent->client->scoremode == 1)
		ent->client->scoremode = 2;

	if (ent->client->scoremode == 2)
	{
		Serverwide_ScoreboardMessage (ent);
		return;
	}

	if (arenas[ent->client->arenanum].active)
		Arena_ScoreboardMessage (ent);
	else
		Pickup_ScoreboardMessage (ent);
}


/*
==================
Pickup_ScoreboardMessage

Free-for-all scoreboard, scoped to the players in ent's own arena
(a "pickup" arena has no fixed teams).
==================
*/
void Pickup_ScoreboardMessage (edict_t *ent)
{
	char	entry[1024];
	char	string[1400];
	int		stringlength;
	int		i, j, k;
	int		sorted[MAX_CLIENTS];
	int		sortedscores[MAX_CLIENTS];
	int		score, total;
	int		x, y;
	gclient_t	*cl;
	edict_t		*cl_ent;
	char	*tag;

	// sort the clients in this arena by score
	total = 0;
	for (i=0 ; i<game.maxclients ; i++)
	{
		cl_ent = g_edicts + 1 + i;
		if (!cl_ent->inuse || !cl_ent->client->arenanum ||
			cl_ent->client->arenanum != ent->client->arenanum)
			continue;
		score = game.clients[i].resp.score;
		for (j=0 ; j<total ; j++)
			if (score > sortedscores[j])
				break;
		for (k=total ; k>j ; k--)
		{
			sorted[k] = sorted[k-1];
			sortedscores[k] = sortedscores[k-1];
		}
		sorted[j] = i;
		sortedscores[j] = score;
		total++;
	}

	string[0] = 0;
	stringlength = 0;

	Com_sprintf (entry, sizeof(entry), "xv 0 yv 8 string2 \"%s\" ",
		arenas[ent->client->arenanum].name);
	j = strlen(entry);
	strcpy (string + stringlength, entry);
	stringlength += j;

	if (total > 12)
		total = 12;

	for (i=0 ; i<total ; i++)
	{
		cl = &game.clients[sorted[i]];
		cl_ent = g_edicts + 1 + sorted[i];

		x = (i>=6) ? 160 : 0;
		y = 32 + 32 * (i%6);

		if (cl_ent == ent)
			tag = "tag1";
		else
			tag = NULL;
		if (tag)
		{
			Com_sprintf (entry, sizeof(entry),
				"xv %i yv %i picn %s ", x+32, y, tag);
			j = strlen(entry);
			if (stringlength + j > 1024)
				break;
			strcpy (string + stringlength, entry);
			stringlength += j;
		}

		Com_sprintf (entry, sizeof(entry),
			"client %i %i %i %i %i %i ",
			x, y, sorted[i], cl->resp.score, cl->ping, (level.framenum - cl->resp.enterframe)/600);
		j = strlen(entry);
		if (stringlength + j > 1024)
			break;
		strcpy (string + stringlength, entry);
		stringlength += j;
	}

	gi.WriteByte (svc_layout);
	gi.WriteString (string);
	gi.unicast (ent, ent->client->scoremode == 2);
}


/*
==================
Arena_ScoreboardMessage

Team scoreboard, scoped to ent's own arena - one column per team,
with the team name and score as a header.
==================
*/
void Arena_ScoreboardMessage (edict_t *ent)
{
	char	entry[1024];
	char	string[1400];
	int		stringlength;
	int		i, t, y;
	gclient_t	*cl;
	edict_t		*cl_ent;
	char	*tag;

	string[0] = 0;
	stringlength = 0;

	for (t = 0; t < 2; t++)
	{
		Com_sprintf (entry, sizeof(entry), "xv %i yv 8 string2 \"%s\" ",
			t ? 160 : 0, teams[arenas[ent->client->arenanum].teamnum[t]]->name);
		strcpy (string + stringlength, entry);
		stringlength += strlen(entry);

		y = 32;
		for (i=1 ; i<=game.maxclients ; i++)
		{
			cl_ent = g_edicts + i;
			if (!cl_ent->inuse || cl_ent->client->arenanum != ent->client->arenanum)
				continue;
			if (cl_ent->client->teamnum != arenas[ent->client->arenanum].teamnum[t])
				continue;

			cl = cl_ent->client;

			tag = (cl_ent == ent) ? "tag1" : NULL;
			if (tag)
			{
				Com_sprintf (entry, sizeof(entry),
					"xv %i yv %i picn %s ", (t?160:0)+32, y, tag);
				strcpy (string + stringlength, entry);
				stringlength += strlen(entry);
			}

			Com_sprintf (entry, sizeof(entry),
				"client %i %i %i %i %i %i ",
				t ? 160 : 0, y, cl_ent - g_edicts - 1, cl->resp.score, cl->ping,
				(level.framenum - cl->resp.enterframe)/600);
			if (stringlength + (int)strlen(entry) > 1024)
				break;
			strcpy (string + stringlength, entry);
			stringlength += strlen(entry);

			y += 32;
		}
	}

	gi.WriteByte (svc_layout);
	gi.WriteString (string);
	gi.unicast (ent, ent->client->scoremode == 2);
}


/*
==================
Serverwide_ScoreboardMessage

Overview scoreboard - one summary line per active arena on the server.
==================
*/
void Serverwide_ScoreboardMessage (edict_t *ent)
{
	char	entry[1024];
	char	string[1400];
	int		stringlength;
	int		i, y;

	string[0] = 0;
	stringlength = 0;

	y = 8;
	for (i = 1; i <= num_arenas; i++)
	{
		if (!arenas[i].active)
			continue;

		Com_sprintf (entry, sizeof(entry),
			"xv 0 yv %i string2 \"%s\" ", y, arenas[i].name);
		if (stringlength + (int)strlen(entry) > 1024)
			break;
		strcpy (string + stringlength, entry);
		stringlength += strlen(entry);

		y += 16;
	}

	gi.WriteByte (svc_layout);
	gi.WriteString (string);
	gi.unicast (ent, true);
}


/*
==================
DeathmatchScoreboard

Draw instead of help message.
Note that it isn't that hard to overflow the 1400 byte message limit!
==================
*/
void DeathmatchScoreboard (edict_t *ent)
{
	DeathmatchScoreboardMessage (ent, ent->enemy);
	gi.unicast (ent, ent->client->scoremode == 2);
}


/*
==================
Cmd_Score_f

Display the scoreboard
==================
*/
void Cmd_Score_f (edict_t *ent)
{
	ent->client->showinventory = false;
	ent->client->showhelp = false;

	if (!deathmatch->value && !coop->value)
		return;

	ent->client->showscores = true;

	// cycle: arena/pickup view -> serverwide view -> back to arena/pickup
	if (ent->client->scoremode == 2)
		ent->client->scoremode = 0;
	else if (!ent->client->arenanum)
		ent->client->scoremode = 2;
	else
		ent->client->scoremode++;

	if (!ent->client->arenanum && ent->client->scoremode == 1)
		ent->client->scoremode = 2;

	DeathmatchScoreboard (ent);
}


/*
==================
HelpComputer

Draw help computer.
==================
*/
void HelpComputer (edict_t *ent)
{
	char	string[1024];
	char	*sk;

	if (skill->value == 0)
		sk = "easy";
	else if (skill->value == 1)
		sk = "medium";
	else if (skill->value == 2)
		sk = "hard";
	else
		sk = "hard+";

	// send the layout
	Com_sprintf (string, sizeof(string),
		"xv 32 yv 8 picn help "			// background
		"xv 202 yv 12 string2 \"%s\" "		// skill
		"xv 0 yv 24 cstring2 \"%s\" "		// level name
		"xv 0 yv 54 cstring2 \"%s\" "		// help 1
		"xv 0 yv 110 cstring2 \"%s\" "		// help 2
		"xv 50 yv 164 string2 \" kills     goals    secrets\" "
		"xv 50 yv 172 string2 \"%3i/%3i     %i/%i       %i/%i\" ", 
		sk,
		level.level_name,
		game.helpmessage1,
		game.helpmessage2,
		level.killed_monsters, level.total_monsters, 
		level.found_goals, level.total_goals,
		level.found_secrets, level.total_secrets);

	gi.WriteByte (svc_layout);
	gi.WriteString (string);
	gi.unicast (ent, true);
}


/*
==================
Cmd_Help_f

Display the current help message
==================
*/
void Cmd_Help_f (edict_t *ent)
{
	// this is for backwards compatability
	if (deathmatch->value)
	{
		Cmd_Score_f (ent);
		return;
	}

	ent->client->showinventory = false;
	ent->client->showscores = false;

	if (ent->client->showhelp && (ent->client->pers.game_helpchanged == game.helpchanged))
	{
		ent->client->showhelp = false;
		return;
	}

	ent->client->showhelp = true;
	ent->client->pers.helpchanged = 0;
	HelpComputer (ent);
}


//=======================================================================

/*
===============
G_SetStats
===============
*/
void G_SetStats (edict_t *ent)
{
	gitem_t		*item;
	int			index, cells;
	int			power_armor_type;
	char		skinicon[MAX_QPATH];
	int			image, i;

	//
	// player list icon - only show the real skin if it's one of the
	// precached team skins, otherwise fall back to a generic icon
	//
	sprintf (skinicon, "%s_i", Info_ValueForKey (ent->client->pers.userinfo, "skin"));
	image = gi.imageindex (skinicon);

	ent->client->ps.stats[STAT_SKIN_ICON] = level.unknown_icon;
	for (i = 0; i < 7; i++)
	{
		if (image == teamskins_precachem[i] || image == teamskins_precachef[i] ||
			image == teamskins_precachecw[i] || image == teamskins_precachecb[i])
		{
			ent->client->ps.stats[STAT_SKIN_ICON] = image;
			break;
		}
	}

	//
	// health
	//
	ent->client->ps.stats[STAT_HEALTH_ICON] = level.pic_health;
	ent->client->ps.stats[STAT_HEALTH] = ent->health;

	//
	// ammo
	//
	if (!ent->client->ammo_index /* || !ent->client->pers.inventory[ent->client->ammo_index] */)
	{
		ent->client->ps.stats[STAT_AMMO_ICON] = 0;
		ent->client->ps.stats[STAT_AMMO] = 0;
	}
	else
	{
		item = &itemlist[ent->client->ammo_index];
		ent->client->ps.stats[STAT_AMMO_ICON] = gi.imageindex (item->icon);
		ent->client->ps.stats[STAT_AMMO] = ent->client->pers.inventory[ent->client->ammo_index];
	}
	
	//
	// armor
	//
	power_armor_type = PowerArmorType (ent);
	if (power_armor_type)
	{
		cells = ent->client->pers.inventory[ITEM_INDEX(FindItem ("cells"))];
		if (cells == 0)
		{	// ran out of cells for power armor
			ent->flags &= ~FL_POWER_ARMOR;
			gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/power2.wav"), 1, ATTN_NORM, 0);
			power_armor_type = 0;;
		}
	}

	index = ArmorIndex (ent);
	if (power_armor_type && (!index || (level.framenum & 8) ) )
	{	// flash between power armor and other armor icon
		ent->client->ps.stats[STAT_ARMOR_ICON] = gi.imageindex ("i_powershield");
		ent->client->ps.stats[STAT_ARMOR] = cells;
	}
	else if (index)
	{
		item = GetItemByIndex (index);
		ent->client->ps.stats[STAT_ARMOR_ICON] = gi.imageindex (item->icon);
		ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.inventory[index];
	}
	else
	{
		ent->client->ps.stats[STAT_ARMOR_ICON] = 0;
		ent->client->ps.stats[STAT_ARMOR] = 0;
	}

	//
	// pickup message
	//
	if (level.time > ent->client->pickup_msg_time)
	{
		ent->client->ps.stats[STAT_PICKUP_ICON] = 0;
		ent->client->ps.stats[STAT_PICKUP_STRING] = 0;
	}

	//
	// timers
	//
	if (ent->client->quad_framenum > level.framenum)
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex ("p_quad");
		ent->client->ps.stats[STAT_TIMER] = (ent->client->quad_framenum - level.framenum)/10;
	}
	else if (ent->client->invincible_framenum > level.framenum)
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex ("p_invulnerability");
		ent->client->ps.stats[STAT_TIMER] = (ent->client->invincible_framenum - level.framenum)/10;
	}
	else if (ent->client->enviro_framenum > level.framenum)
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex ("p_envirosuit");
		ent->client->ps.stats[STAT_TIMER] = (ent->client->enviro_framenum - level.framenum)/10;
	}
	else if (ent->client->breather_framenum > level.framenum)
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex ("p_rebreather");
		ent->client->ps.stats[STAT_TIMER] = (ent->client->breather_framenum - level.framenum)/10;
	}
	else
	{
		ent->client->ps.stats[STAT_TIMER_ICON] = 0;
		ent->client->ps.stats[STAT_TIMER] = 0;
	}

	//
	// selected item
	//
	if (ent->client->pers.selected_item == -1)
		ent->client->ps.stats[STAT_SELECTED_ICON] = 0;
	else
		ent->client->ps.stats[STAT_SELECTED_ICON] = gi.imageindex (itemlist[ent->client->pers.selected_item].icon);

	ent->client->ps.stats[STAT_SELECTED_ITEM] = ent->client->pers.selected_item;

	//
	// layouts
	//
	ent->client->ps.stats[STAT_LAYOUTS] = 0;

	if (deathmatch->value)
	{
		if (ent->client->pers.health <= 0 || level.intermissiontime
			|| ent->client->showscores)
			ent->client->ps.stats[STAT_LAYOUTS] |= 1;
		if (ent->client->showinventory && ent->client->pers.health > 0)
			ent->client->ps.stats[STAT_LAYOUTS] |= 2;
	}
	else
	{
		if (ent->client->showscores || ent->client->showhelp)
			ent->client->ps.stats[STAT_LAYOUTS] |= 1;
		if (ent->client->showinventory && ent->client->pers.health > 0)
			ent->client->ps.stats[STAT_LAYOUTS] |= 2;
	}

	//
	// frags
	//
	ent->client->ps.stats[STAT_FRAGS] = ent->client->resp.score;

	//
	// help icon / current weapon if not shown
	//
	if (ent->client->pers.helpchanged && (level.framenum&8) )
		ent->client->ps.stats[STAT_HELPICON] = gi.imageindex ("i_help");
	else if ( (ent->client->pers.hand == CENTER_HANDED || ent->client->ps.fov > 91)
		&& ent->client->pers.weapon)
		ent->client->ps.stats[STAT_HELPICON] = gi.imageindex (ent->client->pers.weapon->icon);
	else
		ent->client->ps.stats[STAT_HELPICON] = 0;

	ent->client->ps.stats[STAT_SPECTATOR] = 0;

	//
	// arena join queue - how many players are waiting on each side
	//
	if (!ent->client->arenanum)
	{
		ent->client->ps.stats[STAT_QUEUE1] = 0;
		ent->client->ps.stats[STAT_QUEUE2] = 0;
		ent->client->ps.stats[STAT_SHOWQUEUE] = 0;
	}
	else if (!arenas[ent->client->arenanum].active)
	{
		ent->client->ps.stats[STAT_SHOWQUEUE] = 0;
	}
	else
	{
		// while a round is actually being played, show how many are
		// signed up to play (the roster); otherwise show the raw
		// join queue instead
		if (arenas[ent->client->arenanum].state == 2 ||
			arenas[ent->client->arenanum].state == 5 ||
			arenas[ent->client->arenanum].state == 6)
		{
			ent->client->ps.stats[STAT_QUEUE1] = count_players_queue (arenas[ent->client->arenanum].pickupteam[0]->queue);
			ent->client->ps.stats[STAT_QUEUE2] = count_players_queue (arenas[ent->client->arenanum].pickupteam[1]->queue);
		}
		else
		{
			ent->client->ps.stats[STAT_QUEUE1] = count_queue (arenas[ent->client->arenanum].pickupteam[0]->queue);
			ent->client->ps.stats[STAT_QUEUE2] = count_queue (arenas[ent->client->arenanum].pickupteam[1]->queue);
		}

		ent->client->ps.stats[STAT_QUEUE1_ICON] = game.queue_icon + 2;
		ent->client->ps.stats[STAT_QUEUE2_ICON] = game.queue_icon + 3;
		ent->client->ps.stats[STAT_SHOWQUEUE] = 1;
	}

	CTFSetIDView (ent);
}

/*
===============
G_CheckChaseStats
===============
*/
void G_CheckChaseStats (edict_t *ent)
{
	int i;
	gclient_t *cl;

	for (i = 1; i <= maxclients->value; i++) {
		cl = g_edicts[i].client;
		if (!g_edicts[i].inuse || cl->chase_target != ent)
			continue;
		memcpy(cl->ps.stats, ent->client->ps.stats, sizeof(cl->ps.stats));
		G_SetSpectatorStats(g_edicts + i);
	}
}

/*
===============
G_SetSpectatorStats
===============
*/
void G_SetSpectatorStats (edict_t *ent)
{
	gclient_t *cl = ent->client;

	if (!cl->chase_target)
		G_SetStats (ent);

	cl->ps.stats[STAT_SPECTATOR] = 1;

	// layouts are independant in spectator
	cl->ps.stats[STAT_LAYOUTS] = 0;
	if (cl->pers.health <= 0 || level.intermissiontime || cl->showscores)
		cl->ps.stats[STAT_LAYOUTS] |= 1;
	if (cl->showinventory && cl->pers.health > 0)
		cl->ps.stats[STAT_LAYOUTS] |= 2;

	if (cl->chase_target && cl->chase_target->inuse)
		cl->ps.stats[STAT_CHASE] = CS_PLAYERSKINS + 
			(cl->chase_target - g_edicts) - 1;
	else
		cl->ps.stats[STAT_CHASE] = 0;
}

