#ifndef _MENU_H
#define _MENU_H

typedef struct qmenu_s {
    void            *it;
    struct qmenu_s  *next;
    struct qmenu_s  *prev;
} qmenu_t;

typedef int (*menuselect_t)(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg);

typedef struct {
    char            *text;
    char            *value;
    int             num;
    menuselect_t    select;
} menuitem_t;

typedef struct {
    char    *title;
    qmenu_t *items;
    int     flags;
} menuinfo_t;

#define MAXSTATUSBAR    1400
#define MAXMENUTEXT     MAXSTATUSBAR

void        add_to_queue(qmenu_t *node, qmenu_t *head);
qmenu_t     *remove_from_queue(qmenu_t *node, qmenu_t *head);
void        add_to_front_queue(qmenu_t *node, qmenu_t *head);

int         count_queue(qmenu_t *head);

char        *LoPrint(char *string);
char        *HiPrint(char *string);
void        SendMenu(edict_t *ent);
void        SendStatusBar(edict_t *ent, const char *string, bool transmit);
void        DisplayMenu(edict_t *ent);
qmenu_t     *CreateQMenu(edict_t *ent, char *title);
qmenu_t     *AddMenuItem(qmenu_t *menu, char *text, char *value, int num, menuselect_t select);
void        FinishMenu(edict_t *ent, qmenu_t *menu, bool show);
void        MenuNext(edict_t *ent);
void        MenuPrev(edict_t *ent);
void        UseMenu(edict_t *ent, int arg);
bool    MenuThink(edict_t *ent);
void        clear_menus(edict_t *ent);
void        close_menus(edict_t *ent);

#endif // _MENU_H
