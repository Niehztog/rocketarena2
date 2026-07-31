// menu.h -- generic in-game menu engine, built on the stock statusbar/layout
// program language (see gi.WriteByte(svc_configstring)/CS_STATUSBAR usage in
// menu.c).  Content-free: everything here is reusable by any menu, arena-
// specific or not.

#ifndef _MENU_H
#define _MENU_H

// a qmenu_t is a generic doubly-linked queue node.  The same node shape is
// reused for three different queues: the per-client queue of pending/active
// menus (gclient_t->menuqueue), the list of items hanging off a menu
// (menuinfo_t->items), and the handle CreateQMenu/AddMenuItem hand back to
// the caller (which is itself just a queue node whose data points at the
// real payload).  menu.c and its callers otherwise only ever touch ->data.
typedef struct qmenu_s
{
	void			*data;
	struct qmenu_s	*next;
	struct qmenu_s	*prev;
} qmenu_t;

// a select callback runs when an item is chosen with invuse (see UseMenu).
// Return 0 to close the menu, 1 to leave it up and just redraw it (e.g.
// after changing a value in place), or any other value if the callback
// already took care of redrawing/replacing the menu itself.
typedef int (*menuselect_t) (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg);

// the payload of an item node (item->data).  value and the trailing number
// are both optional -- value is NULL, num is negative -- and are appended
// after text when the item is drawn.  An item with no select callback is a
// plain label: MenuNext/MenuPrev skip over it.
typedef struct
{
	char			*text;
	char			*value;
	int				num;
	menuselect_t	select;
} menuitem_t;

// the payload of a menu node (menu->data, i.e. what CreateQMenu allocates).
typedef struct
{
	char	*title;
	qmenu_t	*items;
	int		flags;
} menuinfo_t;

// gi.WriteString buffer -- see gclient_t->menutext. Sized to land the
// confirmed-by-disassembly damage_armor anchor (gclient_t+5092) exactly;
// the original 1400 guess had no direct evidence behind it either, so
// this isn't a regression in confidence, just a different unverified
// guess constrained by harder evidence found elsewhere in the struct.
#define	MAXMENUTEXT	1356

//
// menu.c
//
void		PrintMenuItem (menuitem_t *item);
void		PrintMenu (qmenu_t *menu);
void		PrintMenuQueue (edict_t *ent);
char		*LoPrint (char *string);
char		*HiPrint (char *string);
void		SendMenu (edict_t *ent);
void		SendStatusBar (edict_t *ent, char *string, qboolean transmit);
void		DisplayMenu (edict_t *ent);
void		DisplaySimpMenu (edict_t *ent);
qmenu_t		*CreateQMenu (edict_t *ent, char *title);
void		AddMenuItem (qmenu_t *menu, char *text, char *value, int num, menuselect_t select);
void		FinishMenu (edict_t *ent, qmenu_t *menu, qboolean show);
void		MenuNext (edict_t *ent);
void		MenuPrev (edict_t *ent);
void		UseMenu (edict_t *ent, int arg);
qboolean	MenuThink (edict_t *ent);
void		clear_menus (edict_t *ent);
int			MySelect (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg);
int			MySelect2 (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg);
int			MySelect3 (edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg);

#endif // _MENU_H
