// menu.c -- generic in-game menu engine
//
// Menus are drawn by taking over CS_STATUSBAR (the same "layout program"
// language stock Quake 2 uses for the health/ammo HUD, see p_hud.c) for as
// long as a menu is up, and handing it back to the normal deathmatch/
// singleplayer statusbar program when it closes.  Navigation reuses the
// stock inventory keys -- invnext/invprev/invuse/inven map to MenuNext,
// MenuPrev, UseMenu and clear_menus.

#include "g_local.h"

#define	MAXMENUITEMS	18		// items per statusbar page before "(More)"

//
// queue_insert/queue_delete -- the doubly linked list glue shared by a
// menu's item list and a client's menu queue.  Kept private to this file;
// nothing outside menu.c ever looks at a qmenu_t's next/prev.
//

static void
queue_insert (qmenu_t *node, qmenu_t **head)
{
	node->prev = NULL;
	node->next = *head;
	if (*head)
		(*head)->prev = node;
	*head = node;
}

static void
queue_delete (qmenu_t *node, qmenu_t **head)
{
	if (node->prev)
		node->prev->next = node->next;
	else
		*head = node->next;

	if (node->next)
		node->next->prev = node->prev;
}

void
PrintMenuItem (menuitem_t *item)
{
	gi.bprintf (PRINT_HIGH, "  %s %s %d\n", item->text, item->value, item->num);
}

void
PrintMenu (qmenu_t *menu)
{
	menuinfo_t	*info;
	qmenu_t		*node;
	menuitem_t	*item;

	info = (menuinfo_t *)menu->data;
	gi.bprintf (PRINT_HIGH, "%s\n", info->title);

	for (node = info->items ; node ; node = node->next)
	{
		item = (menuitem_t *)node->data;
		gi.bprintf (PRINT_HIGH, "  %s %s %d\n", item->text, item->value, item->num);
	}
}

void
PrintMenuQueue (edict_t *ent)
{
	menuinfo_t	*info;
	qmenu_t		*menu, *node;
	menuitem_t	*item;

	for (menu = ent->client->menuqueue ; menu ; menu = menu->next)
	{
		info = (menuinfo_t *)menu->data;
		gi.bprintf (PRINT_HIGH, "%s\n", info->title);

		for (node = info->items ; node ; node = node->next)
		{
			item = (menuitem_t *)node->data;
			gi.bprintf (PRINT_HIGH, "  %s %s %d\n", item->text, item->value, item->num);
		}
	}
}

// LoPrint/HiPrint flip a string between the console font's plain and
// "hi-bit" (gold) character ranges in place, and hand the same pointer
// back.  HiPrint is used to make a menu's title stand out.

char *
LoPrint (char *string)
{
	char	*p;

	if (!string)
		return NULL;

	for (p = string ; *p ; p++)
		if ((unsigned char)*p > 0x7f)
			*p += 0x80;

	return string;
}

char *
HiPrint (char *string)
{
	char	*p;

	if (!string)
		return NULL;

	for (p = string ; *p ; p++)
		if ((unsigned char)(*p + 0xe0) <= 0x5e)
			*p += 0x80;

	return string;
}

void
SendMenu (edict_t *ent)
{
	gi.WriteByte (svc_configstring);
	gi.WriteShort (CS_STATUSBAR);
	gi.WriteString (ent->client->menutext);
	gi.unicast (ent, false);
}

// SendStatusBar copies string into the client's statusbar buffer and, if
// transmit is set, flushes it immediately with SendMenu.  menutime tracks
// the level.framenum the buffer was last flushed on so MenuThink can tell
// whether a periodic keepalive resend is due.
void
SendStatusBar (edict_t *ent, char *string, qboolean transmit)
{
	strncpy (ent->client->menutext, string, sizeof (ent->client->menutext));

	if (transmit)
	{
		SendMenu (ent);
		ent->client->menutime = level.framenum;
	}
	else
	{
		ent->client->menutime = level.framenum + 1;
	}
}

void
DisplayMenu (edict_t *ent)
{
	gclient_t	*cl;
	menuinfo_t	*info;
	menuitem_t	*item;
	qmenu_t		*node;
	char		buf[MAXMENUTEXT];
	char		text[256];
	char		row[300];
	int			selected, page, y, shown;

	cl = ent->client;

	if (!cl->showmenu)
	{
		SendStatusBar (ent, deathmatch->value ? dm_statusbar : single_statusbar, true);
		return;
	}

	// flush whatever DisplayMenu built up last time it ran before spending
	// time building this frame's version
	SendStatusBar (ent, cl->menutext, true);

	info = (menuinfo_t *)cl->menu->data;

	// find which page the highlighted item falls on
	selected = 0;
	for (node = info->items ; node ; node = node->next, selected++)
		if (node == cl->menuitem)
			break;
	page = selected / MAXMENUITEMS;

	strcpy (buf, "xv 32 yv 8 picn inventory ");

	sprintf (row, "xv 202 yv 12 string2 \"%s\" ", HiPrint (info->title));
	strcat (buf, row);

	node = info->items;
	for (selected = 0 ; node && selected < page * MAXMENUITEMS ; selected++)
		node = node->next;

	strcat (buf, "xv 0 ");
	y = 24;

	for (shown = 0 ; node && shown < MAXMENUITEMS ; node = node->next, shown++)
	{
		item = (menuitem_t *)node->data;

		strcpy (text, item->text);
		if (item->value)
			strcat (text, item->value);
		if (item->num >= 0)
			sprintf (text + strlen (text), "%d", item->num);

		sprintf (row, "yv %d string2 \"%s\" ", y, text);
		strcat (buf, row);

		y += 8;
	}

	if (shown == MAXMENUITEMS && node)
	{
		sprintf (row, "yv %d string2 \"(More)\" ", y + 10);
		strcat (buf, row);
	}

	strncpy (cl->menutext, buf, sizeof (cl->menutext));
	cl->menutime = level.framenum + 1;
}

void
DisplaySimpMenu (edict_t *ent)
{
	gclient_t	*cl;
	menuinfo_t	*info;
	menuitem_t	*item;
	qmenu_t		*node;
	char		buf[MAXMENUTEXT];
	char		num[16];

	cl = ent->client;

	if (!cl->showmenu)
	{
		gi.centerprintf (ent, "");
		return;
	}

	info = (menuinfo_t *)cl->menu->data;

	strcpy (buf, HiPrint (info->title));

	for (node = info->items ; node ; node = node->next)
	{
		strcat (buf, "\n");
		if (node == cl->menuitem)
			strcat (buf, "*");

		item = (menuitem_t *)node->data;
		strcat (buf, item->text);
		if (item->value)
			strcat (buf, item->value);
		if (item->num >= 0)
		{
			sprintf (num, "%d", item->num);
			strcat (buf, num);
		}
	}

	gi.centerprintf (ent, "%s", buf);
}

qmenu_t *
CreateQMenu (edict_t *ent, char *title)
{
	menuinfo_t	*info;
	qmenu_t		*menu;

	info = gi.TagMalloc (sizeof (*info), TAG_LEVEL);
	menu = gi.TagMalloc (sizeof (*menu), TAG_LEVEL);
	menu->data = info;

	info->title = gi.TagMalloc (strlen (title) + 1, TAG_LEVEL);
	strcpy (info->title, title);
	info->items = NULL;
	info->flags = 0;

	return menu;
}

void
AddMenuItem (qmenu_t *menu, char *text, char *value, int num, menuselect_t select)
{
	menuinfo_t	*info;
	menuitem_t	*item;
	qmenu_t		*node;

	node = gi.TagMalloc (sizeof (*node), TAG_LEVEL);
	item = gi.TagMalloc (sizeof (*item), TAG_LEVEL);

	item->text = gi.TagMalloc (strlen (text) + 1, TAG_LEVEL);
	strcpy (item->text, text);

	if (value)
	{
		item->value = gi.TagMalloc (strlen (value) + 1, TAG_LEVEL);
		strcpy (item->value, value);
	}
	else
	{
		item->value = NULL;
	}

	item->num = num;
	item->select = select;
	node->data = item;

	info = (menuinfo_t *)menu->data;
	queue_insert (node, &info->items);
}

void
FinishMenu (edict_t *ent, qmenu_t *menu, qboolean show)
{
	gclient_t	*cl;
	menuinfo_t	*info;

	cl = ent->client;
	info = (menuinfo_t *)menu->data;

	cl->menu = menu;
	cl->menuitem = info->items;
	cl->showmenu = show;

	queue_insert (menu, &cl->menuqueue);

	DisplayMenu (ent);
}

void
MenuNext (edict_t *ent)
{
	gclient_t	*cl;
	qmenu_t		*node;
	menuitem_t	*item;

	cl = ent->client;

	for ( ; ; )
	{
		node = cl->menuitem->next;
		if (!node)
			node = ((menuinfo_t *)cl->menu->data)->items;

		cl->menuitem = node;

		if (!node->next)
			break;

		item = (menuitem_t *)node->data;
		if (item->select)
			break;
	}

	DisplayMenu (ent);
}

void
MenuPrev (edict_t *ent)
{
	gclient_t	*cl;
	qmenu_t		*node;
	menuitem_t	*item;

	cl = ent->client;

	for ( ; ; )
	{
		node = cl->menuitem->prev;
		if (!node)
			for (node = cl->menuitem ; node->next ; node = node->next)
				;

		cl->menuitem = node;

		if (!node->prev)
			break;

		item = (menuitem_t *)node->data;
		if (item->select)
			break;
	}

	DisplayMenu (ent);
}

void
UseMenu (edict_t *ent, int arg)
{
	gclient_t	*cl;
	qmenu_t		*menu, *item, *node, *next, *tail;
	menuinfo_t	*info;
	menuitem_t	*it;
	int			result;

	cl = ent->client;

	if (cl->menuusetime + 5 > level.framenum)
		return;
	cl->menuusetime = level.framenum;

	menu = cl->menu;
	item = cl->menuitem;
	it = (menuitem_t *)item->data;

	if (!it->select)
		return;

	result = it->select (ent, menu, item, arg);

	if (result == 1)
	{
		DisplayMenu (ent);
		return;
	}
	if (result != 0)
		return;

	// the callback wants this menu closed -- tear it down and drop the
	// next queued menu, if any, into its place
	queue_delete (menu, &cl->menuqueue);

	info = (menuinfo_t *)menu->data;
	gi.TagFree (info->title);

	for (node = info->items ; node ; node = next)
	{
		next = node->next;
		it = (menuitem_t *)node->data;
		gi.TagFree (it->text);
		if (it->value)
			gi.TagFree (it->value);
		gi.TagFree (it);
		gi.TagFree (node);
	}

	gi.TagFree (info);
	gi.TagFree (menu);

	if (cl->menuqueue)
	{
		for (tail = cl->menuqueue ; tail->next ; tail = tail->next)
			;

		cl->menu = tail;
		cl->menuitem = ((menuinfo_t *)tail->data)->items;
	}
	else
	{
		cl->menu = NULL;
		cl->showmenu = false;
	}

	DisplayMenu (ent);
}

qboolean
MenuThink (edict_t *ent)
{
	gclient_t	*cl;

	cl = ent->client;

	if (!cl->showmenu)
		return false;

	if ((level.framenum - cl->menutime) % 10 != 0)
		return false;

	SendMenu (ent);

	return true;
}

void
clear_menus (edict_t *ent)
{
	ent->client->showmenu = false;
	ent->client->menu = NULL;
	ent->client->menuqueue = NULL;

	DisplayMenu (ent);
}

int
MySelect (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	menuitem_t	*it;

	it = (menuitem_t *)item->data;
	gi.bprintf (PRINT_HIGH, "menu item %s selected by %s\n", it->text, ent->client->pers.netname);

	return 0;
}

int
MySelect2 (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	menuitem_t	*it;

	it = (menuitem_t *)item->data;

	if (arg)
		it->num++;
	else
		it->num--;

	if (it->num == 0)
		it->num = 1;

	return 1;
}

int
MySelect3 (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	menuinfo_t	*info;
	menuitem_t	*fragitem, *timeitem;
	char		fragbuf[16], timebuf[16];

	info = (menuinfo_t *)menu->data;
	fragitem = (menuitem_t *)info->items->data;
	timeitem = (menuitem_t *)info->items->next->data;

	sprintf (fragbuf, "%d", fragitem->num);
	sprintf (timebuf, "%d", timeitem->num);

	gi.bprintf (PRINT_HIGH, "Fraglimit is now %s. Timelimit is now %s\n", fragbuf, timebuf);

	gi.cvar_set ("fraglimit", fragbuf);
	gi.cvar_set ("timelimit", timebuf);

	return 0;
}
