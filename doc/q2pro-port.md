Rocket Arena 2 on the Q2PRO game API
====================================

This branch (`feature/q2pro-port`) is Rocket Arena 2 v2.25 with the whole of
Q2PRO's baseq2 commit history replayed on top of it.

It is **not** the reconstruction. The reconstruction lives on `main-github`,
where 722 of 730 functions still assemble byte-for-byte to the original
`gamex86.dll`. That property is gone here by design: this tree has been
reformatted, retyped, restructured and bug-fixed. Do not use it for
address matching, and do not compare it against the original binaries.

Why
---

RA2 v2.25 is id's 3.20 game source with a mod grafted onto it, and it carries
every bug and every rough edge that source shipped with in 1998 — unbounded
`sprintf` into fixed buffers, `char` arithmetic on network input, float timers
that drift, savegame code that writes raw pointers to disk. Q2PRO has spent
twenty-odd years fixing that same code. Replaying its history onto RA2 gets
all of it at once, rather than re-deriving each fix by hand.

Method
------

The two trees have a common ancestor — id's Quake II 3.20 game source — so
the port was done as a rebase rather than as a merge or a rewrite.

1. **Establish the common root.** Q2PRO's first game commit is a normalised
   copy of id's 3.20/3.21 source: tabs expanded, CRLF stripped, `qboolean`
   spellings unified, `Com_sprintf` renamed, and so on. That normalisation was
   reproduced exactly — the reproduction of Q2PRO's import commit is
   byte-identical to the real one — and then applied to RA2's own 3.20 base.
   Both trees now sit on the same root commit.

2. **Record RA2 as a delta.** RA2 v2.25 becomes a single commit on that root:
   the files it changed, the files it added, and the 46 baseq2 files it does
   not ship at all (the monster code, mostly).

3. **Replay.** Q2PRO's 188 game-source commits are rebased onto that delta,
   one at a time. Conflicts were resolved per commit, not in bulk.

The interesting part is step 3, because roughly a fifth of Q2PRO's commits are
*mechanical*: an astyle reformat, `qboolean` → `bool`, float timers → frame
numbers, const-ification of the item list, `.0` → `.0f` on float literals,
whitespace normalisation. Applying those as diffs would only reach the files
baseq2 and RA2 have in common — `arena.c`, `maploop.c`, `ra2menus.c` and the
rest of RA2's own code would have been left behind in the old style, and the
result would not have compiled once the shared headers changed underneath it.

So mechanical commits were replayed by **re-running the transformation** over
the whole tree instead. A file byte-identical to Q2PRO's is taken verbatim;
everything else is transformed locally with the same tool and the same
settings Q2PRO used. RA2's own sources therefore get the same treatment as the
shared ones.

The exception is the vendored third-party layer — `gstats.c`, `gbucket.c`,
`hashtable.c`, `darray.c`, `nonport.c`, `md5c.c` and their headers, which come
from the GameSpy SDK and RA2's own utility code. Those keep their upstream
formatting; they are still compiled with the same warnings and still had the
API changes applied, they just were not reformatted.

Deviations
----------

A handful of Q2PRO changes had no site in RA2, or a *different* site. Those
were carried across by hand:

* **Flood protection.** Q2PRO added a cvar-driven flood check at the top of
  `Cmd_Say_f`. RA2 had already replaced that spot with its own hardcoded spam
  counter. Both are kept — Q2PRO's runs first.

* **Spawn floor-clip.** Q2PRO added a trace in `PutClientInServer()` that
  drops a spawning player onto the floor instead of leaving them 10 units
  above the spawn point. RA2's `PutClientInServer()` does not place the player
  at all; `move_to_arena()` does. The trace went there instead. (RA2 had
  already declared unused `mins`/`maxs` locals in that function, which suggests
  it meant to do something similar.)

* **Statusbar.** Q2PRO concatenates `dm_statusbar` onto `single_statusbar`.
  RA2's `dm_statusbar` is a complete bar in its own right, so it is still sent
  on its own.

* **Stat slots.** RA2's private `q_shared.h` reused baseq2's `STAT_CHASE` and
  `STAT_SPECTATOR` slots. That header is gone, so the slot definitions moved to
  `g_local.h`. Q2PRO's second powerup timer needs two free slots; RA2's layout
  has 19 and 20 unused, so they land there.

* **Internal linkage.** Q2PRO gave several baseq2 functions `static` once
  nothing outside their file called them. RA2 still calls
  `InitClientPersistant`, `InitClientResp`, `PlayersRangeFromSpot`,
  `SV_AddGravity` and `Weapon_Generic` from `arena.c`, so those keep external
  linkage and are declared in `g_local.h`.

* **Orphaned statics.** The reverse also happens: RA2 removed the call sites
  for a dozen baseq2 functions Q2PRO has since made `static`
  (`SelectSpawnPoint`, `TossClientWeapon`, `Cmd_Kill_f`, `M_ReactToDamage`,
  `CheckPowerArmor` and friends). They were dead in RA2 v2.25 too. The code is
  kept and tagged `q_unused` rather than deleted.

* **`assert`.** RA2 got `<assert.h>` through its own `q_shared.h`. Q2PRO's
  `shared.h` does not include it, so `darray.c`, `hashtable.c` and `gbucket.c`
  include it directly.

* **Format strings.** Q2PRO annotates the game import table with
  `__attribute__((format))`. That immediately exposed seven places where RA2
  passes a runtime string as a format — `gi.centerprintf(e, s)` and friends —
  and two where a `void *` is printed with `%s`. All nine are fixed.

What is checked
---------------

* All 40 baseq2 item-list entries match Q2PRO's field for field; RA2's grapple
  is the only addition.
* All 111 RA2 spawn classnames are present, plus `monster_makron`, which
  Q2PRO added.
* All 43 RA2 cvars and all 39 RA2 client commands are present.
* `g_ptrs.c` regenerates byte-identical from Q2PRO's `genptr.py` over this
  tree.
* RA2 added exactly one savegame field over vanilla (`arena`); it is in the
  entity descriptor table.
* Every function RA2 v2.25 defined still exists, except the `q_shared.c` math
  and byte-order helpers (now provided by the engine's `shared.c`/`shared.h`),
  the old savegame reader/writer (rewritten), and two functions that were
  already dead in v2.25.

Warnings
--------

Compiled with `-Wall -Wextra` plus Q2PRO's own warning set, the original tree
produces 200 warnings and this one produces 43. No category is above its
original count. What is gone: 117 pointer-to-int casts, 17 non-exhaustive
switches, 8 ignored return values, and all of the strict-aliasing and
uninitialised-use reports. What remains is pre-existing RA2 code that the port
does not touch — the `if (it = FindItem(...))` idiom, the GameSpy SDK's
function-pointer casts, and a handful of dead locals.

Address annotations
-------------------

The `/* gamex86.dll 0x... */` comments above most functions are left in place,
but they describe the **reconstruction**, not this tree. They are accurate on
`main-github` and meaningless here. Treat them as a cross-reference to the
original function, nothing more.

Building
--------

Unchanged from the reconstruction — same six configurations:

    make                # native debug + release
    make windows        # win32 + win64, debug + release, via MinGW

Output is `game<cpu>.so` / `game<cpu>.dll`, which is what Q2PRO looks for.
The Windows DLLs still export only `GetGameAPI`.

`config.h` is Q2PRO's build configuration for a standalone game library, and
`shared/` holds the engine headers the game links against. Both are vendored
verbatim from Q2PRO so the ABI matches whatever engine loads the library.

Security
--------

The reconstruction deliberately keeps RA2's original bugs, including its
security holes. This branch does not: Q2PRO's fixes to the shared code came
across with everything else, and the format-string problems listed above are
fixed. RA2's *own* logic has not been audited, so this is safer than the
reconstruction but not audited-safe.
