#include "g_local.h"

#define	MAXMENUITEMS	18

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x00050ca0-0x00050cc1 */
void
PrintMenuItem (menuitem_t *item)
{
	gi.bprintf (PRINT_HIGH, "  %s %s %d\n", item->text, item->value, item->num);
}

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x00050cc4-0x00050d11 */
void
PrintMenu (qmenu_t *menu)
{
	menuinfo_t	*info;
	qmenu_t		*node;
	menuitem_t	*item;

	info = (menuinfo_t *)menu->it;
	gi.bprintf (PRINT_HIGH, "%s\n", info->title);

	node = (qmenu_t *)info;
	while (node->next)
	{
		node = node->next;
		item = (menuitem_t *)node->it;
		gi.bprintf (PRINT_HIGH, "  %s %s %d\n", item->text, item->value, item->num);
	}
}

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x00050d14-0x00050d7c */
void
PrintMenuQueue (edict_t *ent)
{
	menuinfo_t	*info;
	qmenu_t		*menu, *node;
	menuitem_t	*item;

	menu = &ent->client->menuqueue;
	while (menu->next)
	{
		menu = menu->next;
		info = (menuinfo_t *)menu->it;
		gi.bprintf (PRINT_HIGH, "%s\n", info->title);

		node = (qmenu_t *)info;
		while (node->next)
		{
			node = node->next;
			item = (menuitem_t *)node->it;
			gi.bprintf (PRINT_HIGH, "  %s %s %d\n", item->text, item->value, item->num);
		}
	}
}

/* gamex86.dll 0x2001f170-0x2001f1b0 (manual-confirmed) */
/* gamei386.so 0x00050d7c-0x00050dd8 */
char *
LoPrint (char *string)
{
	int		i;

	if (!string)
		return NULL;

	for (i = 0 ; i < strlen (string) ; i++)
		if ((unsigned char)string[i] > 0x7f)
			string[i] += 0x80;

	return string;
}

/* gamex86.dll 0x2001f1b0-0x2001f1f4 (manual-confirmed) */
/* gamei386.so 0x00050dd8-0x00050e3c */
char *
HiPrint (char *string)
{
	int		i;

	if (!string)
		return NULL;

	for (i = 0 ; i < strlen (string) ; i++)
		if ((unsigned char)string[i] < 0x7f && (unsigned char)string[i] >= 0x20)
			string[i] += 0x80;

	return string;
}

/* gamex86.dll 0x2001f200-0x2001f240 (shape-matched(ratio=1.00)) */
/* gamei386.so 0x00050e3c-0x00050e72 */
void
SendMenu (edict_t *ent)
{
	gi.WriteByte (svc_configstring);
	gi.WriteShort (CS_STATUSBAR);
	gi.WriteString (ent->client->menutext);
	gi.unicast (ent, false);
}

/* gamex86.dll 0x2001f240-0x2001f2c0 (aligned) */
/* gamei386.so 0x00050e74-0x00050f13 */
void
SendStatusBar (edict_t *ent, char *string, qboolean transmit)
{
	strncpy (ent->client->menutext, string, MAXSTATUSBAR);
	ent->client->menutime = level.framenum + 1;

	if (transmit)
	{
		if (ent->client->menutime != level.framenum)
			SendMenu (ent);
		ent->client->menutime = level.framenum;
	}
	else
	{
		ent->client->menutime = level.framenum + 1;
	}
}

/* gamex86.dll 0x2001f2c0-0x2001f650 (padded) */
/* gamei386.so 0x00050f14-0x00051564 */
void
DisplayMenu (edict_t *ent)
{
	gclient_t	*cl;
	menuinfo_t	*info;
	qmenu_t		*node, *selected;
	char		*p;
	int			shown, y;
	char		string[MAXSTATUSBAR];
	char		entry[1000];

	cl = ent->client;

	if (!cl->showmenu)
	{
		if (deathmatch->value)
			SendStatusBar (ent, dm_statusbar, true);
		else
			SendStatusBar (ent, single_statusbar, true);
		return;
	}

	info = (menuinfo_t *)cl->curmenulink->it;
	selected = cl->selected;

	string[0] = 0;
	sprintf (string, "xv 32 yv 8 picn inventory ");

	p = string + strlen (string);
	sprintf (p, "xv 202 yv 12 string2 \"%s\" ", "Menu");
	p = string + strlen (string);
	sprintf (p, "xv 0 yv 24 cstring2 \"%s\" ", info->title);

	p = string + strlen (string);
	shown = count_queue ((qmenu_t *)info) - count_queue (selected);
	if (shown > MAXMENUITEMS)
	{
		node = selected;
		do
		{
			node = node->prev;
			shown--;
		} while (node != (qmenu_t *)info && (shown % MAXMENUITEMS) != 0);

		sprintf (p, "xv 50 yv 32 string2 \"(More)\" ");
	}
	else
	{
		node = (qmenu_t *)info;
		sprintf (p, "xv 50 ");
	}

	p = string + strlen (string);
	y = 32;
	shown = 0;

	for ( ;; )
	{
		if (!node->next)
			break;

		if (shown >= MAXMENUITEMS)
			break;

		node = node->next;
		y += 8;
		shown++;

		entry[0] = 0;

		if (node == selected)
		{
			strcat (entry, "\r");
			strcat (entry, LoPrint (((menuitem_t *)node->it)->text));

			if (((menuitem_t *)node->it)->value)
				strcat (entry, ((menuitem_t *)node->it)->value);
		}
		else
		{
			strcat (entry, " ");
			strcat (entry, HiPrint (((menuitem_t *)node->it)->text));

			if (((menuitem_t *)node->it)->value)
				strcat (entry, ((menuitem_t *)node->it)->value);
		}

		LoPrint (((menuitem_t *)node->it)->text);

		if (((menuitem_t *)node->it)->num >= 0)
			sprintf (entry + strlen (entry), "%d", ((menuitem_t *)node->it)->num);

		if (strlen (string) + strlen (entry) + 50 >= MAXSTATUSBAR)
			break;

		sprintf (p, "yv %d string2 \"%s\" ", y, entry);
		p = string + strlen (string);
	}

	if (shown == MAXMENUITEMS && node->next)
		sprintf (p, "yv %d string2 \"(More)\" ", y + 10);

	SendStatusBar (ent, string, false);
}

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x00051564-0x0005172f */
void
DisplaySimpMenu (edict_t *ent)
{
	gclient_t	*cl;
	menuinfo_t	*info;
	qmenu_t		*node, *selected;
	char		buf[MAXSTATUSBAR];

	cl = ent->client;

	if (!cl->showmenu)
	{
		gi.centerprintf (ent, "");
		return;
	}

	info = (menuinfo_t *)cl->curmenulink->it;
	selected = cl->selected;

	count_queue ((qmenu_t *)info);

	buf[0] = 0;
	strcat (buf, HiPrint (info->title));
	LoPrint (info->title);
	strcat (buf, "\n");

	node = (qmenu_t *)info;
	while (node->next)
	{
		node = node->next;
		strcat (buf, "\n");
		if (node == selected)
			strcat (buf, "*");

		strcat (buf, ((menuitem_t *)node->it)->text);
		if (((menuitem_t *)node->it)->value)
			strcat (buf, ((menuitem_t *)node->it)->value);
		if (((menuitem_t *)node->it)->num >= 0)
			sprintf (buf + strlen (buf), "%d", ((menuitem_t *)node->it)->num);
	}

	gi.centerprintf (ent, "%s", buf);
}

/* gamex86.dll 0x2001f650-0x2001f6d0 (aligned) */
/* gamei386.so 0x00051730-0x00051798 */
qmenu_t *
CreateQMenu (edict_t *ent, char *title)
{
	menuinfo_t	*info;
	qmenu_t		*menu;

	info = gi.TagMalloc (sizeof (*info), TAG_LEVEL);
	menu = gi.TagMalloc (sizeof (*menu), TAG_LEVEL);
	menu->it = info;

	info->title = gi.TagMalloc (strlen (title) + 1, TAG_LEVEL);
	strcpy (info->title, title);
	info->flags = 0;
	info->items = NULL;

	return menu;
}

/* gamex86.dll 0x2001f6d0-0x2001f7b0 (manual-confirmed) */
/* gamei386.so 0x00051798-0x00051869 */
qmenu_t *
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
	node->it = item;

	info = (menuinfo_t *)menu->it;
	add_to_queue (node, (qmenu_t *)info);

	return node;
}

/* gamex86.dll 0x2001f7b0-0x2001f800 (call-propagated) */
/* gamei386.so 0x0005186c-0x000518b5 */
void
FinishMenu (edict_t *ent, qmenu_t *menu, qboolean show)
{
	ent->client->curmenulink = menu;
	ent->client->selected = ((menuinfo_t *)menu->it)->items;
	ent->client->showmenu = show;

	add_to_queue (menu, &ent->client->menuqueue);

	DisplayMenu (ent);
}

/* gamex86.dll 0x2001f800-0x2001f880 (call-propagated) */
/* gamei386.so 0x000518b8-0x00051916 */
void
MenuNext (edict_t *ent)
{
	if (ent->client->selected->next)
	{
		ent->client->selected = ent->client->selected->next;

		while (ent->client->selected->next
			&& !((menuitem_t *)ent->client->selected->it)->select)
			ent->client->selected = ent->client->selected->next;
	}
	else
		ent->client->selected =
			((menuinfo_t *)ent->client->curmenulink->it)->items;

	DisplayMenu (ent);
}

/* gamex86.dll 0x2001f880-0x2001f910 (call-propagated) */
/* gamei386.so 0x00051918-0x00051984 */
void
MenuPrev (edict_t *ent)
{
	if (ent->client->selected->prev->prev)
	{
		ent->client->selected = ent->client->selected->prev;

		while (ent->client->selected->prev->prev
			&& !((menuitem_t *)ent->client->selected->it)->select)
			ent->client->selected = ent->client->selected->prev;
	}
	else
	{
		while (ent->client->selected->next)
			ent->client->selected = ent->client->selected->next;
	}

	DisplayMenu (ent);
}

/* gamex86.dll 0x2001f910-0x2001fa60 (call-propagated-reverse) */
/* gamei386.so 0x00051984-0x00051ad3 */
void
UseMenu (edict_t *ent, int arg)
{
	qmenu_t		*menu, *item, *node;
	int			result;

	if (ent->client->menuusetime + 5 > level.framenum)
		return;
	ent->client->menuusetime = level.framenum;

	menu = ent->client->curmenulink;
	item = ent->client->selected;

	if (!((menuitem_t *)item->it)->select)
		return;

	result = ((menuitem_t *)item->it)->select (ent, menu, item, arg);

	if (result)
	{
		if (result == 1)
			DisplayMenu (ent);
		return;
	}

	remove_from_queue (menu, &ent->client->menuqueue);

	node = (qmenu_t *)menu->it;
	gi.TagFree (node->it);

	while (node->next)
	{
		node = node->next;

		gi.TagFree (((menuitem_t *)node->it)->text);
		if (((menuitem_t *)node->it)->value)
			gi.TagFree (((menuitem_t *)node->it)->value);
		if (node->prev)
			gi.TagFree (node->prev);
	}

	if (node)
		gi.TagFree (node);

	gi.TagFree (menu);

	menu = &ent->client->menuqueue;
	while (menu->next)
		menu = menu->next;

	if (menu->it)
	{
		ent->client->curmenulink = menu;
		ent->client->selected = ((menuinfo_t *)ent->client->curmenulink->it)->items;
	}
	else
	{
		ent->client->curmenulink = NULL;
		ent->client->showmenu = false;
	}

	DisplayMenu (ent);
}

/* gamex86.dll 0x2001fa60-0x2001faa0 (shape-matched(ratio=0.73)) */
/* gamei386.so 0x00051ad4-0x00051b3c */
qboolean
MenuThink (edict_t *ent)
{
	gclient_t	*cl;

	cl = ent->client;

	if (cl->showmenu && !((level.framenum - cl->menutime) % 10))
	{
		SendMenu (ent);
		return true;
	}

	return false;
}

/* gamex86.dll 0x2001faa0-0x2001fad0 (shape-matched(ratio=0.86)) */
/* gamei386.so 0x00051b3c-0x00051b71 */
void
clear_menus (edict_t *ent)
{
	ent->client->showmenu = false;
	ent->client->curmenulink = NULL;
	ent->client->menuqueue.next = NULL;

	DisplayMenu (ent);
}

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x00051b74-0x00051b9e */
int
MySelect (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	gi.bprintf (PRINT_HIGH, "menu item %s selected by %s\n",
		((menuitem_t *)item->it)->text, ent->client->pers.netname);

	return 0;
}

/* gamex86.dll: no real counterpart -- confirmed dead code */
/* gamei386.so 0x00051ba0-0x00051bce */
int
MySelect2 (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
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
/* gamei386.so 0x00051bd0-0x00051c43 */
int
MySelect3 (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
	qmenu_t		*node;
	char		fragbuf[8], timebuf[8];

	node = ((menuinfo_t *)menu->it)->items;
	sprintf (fragbuf, "%d", ((menuitem_t *)node->it)->num);
	node = node->next;
	sprintf (timebuf, "%d", ((menuitem_t *)node->it)->num);

	gi.bprintf (PRINT_HIGH, "Fraglimit is now %s. Timelimit is now %s\n", fragbuf, timebuf);

	gi.cvar_set ("fraglimit", fragbuf);
	gi.cvar_set ("timelimit", timebuf);

	return 0;
}
