Rocket Arena 2 on the Q2PRO game API
====================================

This branch (`q2pro-enhancements`) is Rocket Arena 2 v2.25 with the whole of
Q2PRO's baseq2 commit history replayed on top of it, and the dead GameSpy
stats subsystem replaced with a local one.

It is **not** the reconstruction. The reconstruction lives on
[`main`](https://github.com/Niehztog/rocketarena2/tree/main), where 722 of 730
functions still assemble byte-for-byte to the original `gamex86.dll`. That
property is gone here by design: this tree has been reformatted, retyped,
restructured and bug-fixed. Do not use it for address matching, and do not
compare it against the original binaries.

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

  This one has a consequence the first version of it missed: RA2 placed a body
  at the spawn entity, so the entity *was* where the body stood, and every
  distance the spawn selectors measure was taken from the entity. The clip
  moves a body up to 64 units below it. Both selectors therefore measure from
  `spawn_landing()` — the same trace, run once per candidate — rather than
  from `spot->s.origin`, and `move_to_arena()` calls that same helper so the
  question the selector asks and the answer the placement gives cannot drift
  apart.

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

GameSpy
-------

RA2 v2.25 shipped with GameSpy's `gstats` SDK — `gstats.c`, `gbucket.c/.h`,
`darray.c/.h`, `hashtable.c/.h`, `md5c.c`/`md5.h`, `nonport.c/.h` — six
third-party files that accumulated per-round numbers in a tagged-value store,
authenticated with an MD5 challenge-response handshake, and uploaded the
result to `gamestats.gamespy.com` over an XOR-obfuscated wire protocol.

That host has been offline for years. The code was not idle in its absence:
every round start called `NewGame()`, which resolved the hostname, tried to
connect, and on failure wrote the round into an on-disk retry cache that
nothing ever drained. The whole subsystem is gone from this branch, along with
`ValidatePlayer()`, which read a `pid`/`pass` pair out of client userinfo to
authenticate against a GameSpy account that no longer exists.

**RA2's own local logging does not cover what it recorded.** `gslog.c`'s
StdLog (`logfile 2`) writes one line per kill, suicide, connect and
disconnect, plus map changes — but it is server-wide and arena-blind. It has
no notion of which arena an event happened in, no team names or team scores,
and no round boundaries. The GameSpy pipeline had all four, per arena and per
round.

So the numbers are kept, and written locally instead. `ra2stats.c` accumulates
the same counters the buckets did — score, deaths, suicides, and the
grenade/rocket/rail/other kill breakdown per player, score per team, plus the
arena settings and round number — and appends one JSON object per round to a
file in the game directory:

    statsfile   0 = off, 1 = on (default 1)
    statsname   file name (default ra2stats.jsonl)

One object per line, appended, never rewritten: safe against a server dying
mid-match, greppable, and readable by anything that can parse JSON. Player and
team names come straight off the network, so everything outside printable
ASCII is `\u`-escaped — a stray quote in a player name can't corrupt the file.
No new libraries are linked in; the game depends on nothing but libc and libm
on every target.

`netlog`
--------

`gslog.c` is not GameSpy code despite the `GS` prefix — the two systems are
unrelated and of different vintages. But it carried RA2's other remote logging
feature, and that is gone too.

With `logfile 2` and `netlog` set, `GSdodeathlog` sent each kill line to the
host named in the cvar as a single UDP datagram — `readsrv.txt` documents it as
`set netlog ripper.planetquake.com:21998`, and says what was on the other end:
*"Right now all I am doing with the netlog data is displaying the top 5
fraggers of the day on the top of the Rocket Arena page."* That collector has
been gone for as long as GameSpy's.

Unlike the SDK it was cheap while dead — the destination is a cvar rather than
a hardcoded host, both gates default off, and the protocol is one plain-text
datagram, so it would still work if anyone stood up a listener. It was removed
anyway, for what it cost to keep:

* Four `exit(1)` calls — in `net_open_socket`, `net_close_socket`,
  `net_connect_socket` and `net_send` — terminated the host process from inside
  the game library, and did it at the first frag rather than at startup, so a
  bad value survived boot. RA2's own shipped `server.cfg` sets `logfile 2` and
  `netlog` under the comment *"comment these two lines out if you are running
  on a LAN, or if quake2 crashes after the first frag."*
* `net_name_to_address` assigned `inet_addr`'s result and never stored it into
  `sin_addr`, which was only written on the `gethostbyname` path. A dotted-quad
  target resolved to 0.0.0.0 — in the original that reached localhost, and
  after the guard added for the port-range fix it sent nothing at all. Every
  example in RA2's own documentation is a hostname, which is why it was never
  noticed. No compiler warns, because the value *is* read.
* `netlog` is `CVAR_SERVERINFO`, so the collector's host and port were
  published to every client that asked for serverinfo.
* `global_fds` was set and cleared and never read by anything — dead in the
  real binary too, left over from the SDK's select loop.

`GSSendLine`, `net_name_to_address`, `net_send`, `net_open_socket`,
`net_close_socket`, `net_connect_socket`, `GSNetStartup`/`GSNetShutdown` and
`net_compat.h` are all deleted, along with the `netlog` cvar and the `public 0`
interlock that blanked it. That interlock was already vestigial: in the real
v2.25 a non-empty `netlog` was *also* the on-switch for the GameSpy stats
connection — `arena_init` and `arena_think` both call
`if (netlog->string[0] && !IsStatsConnected()) InitStatsConnection(port)` —
which is what `readsrv.txt`'s *"You MUST set public 1 for your netlog stats to
be accepted"* refers to. `ra2stats.c` is gated on `statsfile` instead. Dropping
the game-side `gi.cvar("public", ...)` registration changes nothing, because
Q2PRO registers `public` itself in `SV_Init`, before any game library loads.

StdLog is untouched: `logfile 2` still writes `stdlog.log` with a line per
kill, suicide, connect, disconnect and map change. Only the off-box copy is
gone, and `ra2stats.jsonl` already records more per round than the kill stream
ever carried. `-lws2_32` is dropped from the MinGW link, and the game now opens
no sockets on any target — the Windows DLL imports `KERNEL32` and `msvcrt` and
nothing else.

What is checked
---------------

* All 40 baseq2 item-list entries match Q2PRO's field for field; RA2's grapple
  is the only addition.
* All 111 RA2 spawn classnames are present, plus `monster_makron`, which
  Q2PRO added.
* All 43 RA2 cvars and all 39 RA2 client commands are present, plus
  `statsfile`/`statsname` for the local stats log.
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
produces 200 warnings and this one produces 26. No category is above its
original count. What is gone: 117 pointer-to-int casts, 17 non-exhaustive
switches, 8 ignored return values, all of the strict-aliasing and
uninitialised-use reports, and — with the SDK itself — 12 function-pointer
casts, two sequence-point violations, two out-of-bounds array subscripts and a
format overflow.

**The Makefile builds with `-Wall` now** — on the native and both MinGW flag
sets — and the tree is clean under it on gcc and clang, in all six
configurations. Clearing it took the last of what that paragraph used to call
pre-existing: the seven `if (it = FindItem(...))` assignments in `arena.c` are
parenthesised, `maploop.c`'s two `sprintf(buf, "")` calls are the `buf[0] = 0`
they meant, and the two `strncpy`-then-terminate pairs in `arena.c`/`p_hud.c`
are `Q_strlcpy`, which is what the rest of the tree uses and what stops
`-Wstringop-truncation` firing on the MinGW builds.

`barrel_touch`'s two dead locals are gone, with a note in their place. baseq2's
version ends with `M_walkmove(self, vectoyaw(v), 20 * ratio * FRAMETIME)`, which
is what makes a barrel shift when you walk into it; RA2 ships no monster
movement code — `M_walkmove` is declared in `g_local.h` and defined nowhere, and
the library links with `-Wl,--no-undefined` — so the call cannot be kept, and
the two values it consumed are not computed.

`-Wextra` is still not gated on: it reports unused parameters and
signed/unsigned comparisons throughout code this branch does not change.

Checked and clean
-----------------

The timer-unit class that Q2PRO's Ground Zero pack and the OSP Tourney port both
carried does not exist here, and this is where that is recorded so the next
person does not have to re-derive it. `SV_RunThink` compares `int nextthink`
against `level.framenum`; there is no `nextthink = level.time` site anywhere;
and no `_framenum` field is fed from, or compared against, seconds. The three
`level.framenum + 0.5f / FRAMETIME` sites are a different spelling of
`0.5f * BASE_FRAMERATE`, not a lost scale factor. Allocators pair within each
file — `g_main.c`'s `strdup`/`free` is `EndDMLevel`'s and is Q2PRO's own — and
the menu handles are `TAG_LEVEL`, so the engine reclaims them at level change
and `PutClientInServer`'s memset leaves no dangling pointer behind.

Address annotations
-------------------

The `/* gamex86.dll 0x... */` comments above most functions are left in place,
but they describe the **reconstruction**, not this tree. They are accurate on
`main` and meaningless here. Treat them as a cross-reference to the
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

The reconstruction on `main` deliberately keeps RA2's original bugs, including
its security holes -- reproducing a 1999 binary byte-for-byte means reproducing
what it got wrong. This branch does not. Q2PRO's fixes to the shared code came
across with everything else, and nine calls that handed player-controlled text
to a `printf`-family function as its *format* argument now pass it as a `%s`
operand instead: `gslog.c`'s kill log reached `fprintf` with a line built from
two player names, so a player named `%n%n%n` controlled the format string.
Removing the GameSpy SDK and `netlog` removes every line in the tree that
opened a socket: nothing here contacts the network, and there is no
configuration in which it will.

RA2's own game logic has been reviewed for memory-safety and correctness
defects since the port landed, and fifteen were fixed. Three of them stopped
the branch working at all: `game.maxclients` was never initialised, so the
client array was allocated for zero entries; the `qboolean` -> `bool` retype
shrank `arena_settings_t` from 168 to 96 bytes while `ra2menus.c` still punned
the block as `int[42]`, putting four writes outside `arena_t`; and four
`bool[7]` arrays were written through a stale `extern int[]` in another
translation unit, 21 bytes out of bounds apiece on every map load. None of the
three was visible to the compiler.

A NULL `attacker` no longer reaches an unguarded read, and this one is worth
naming because it is inherited rather than introduced. Nothing in id's code ever
assigns `activator` on a `func_door`, so `door_blocked()` reverses through
`door_go_up(ent, ent->activator)` carrying none, and a `func_clock` that is not
START_OFF never has one either. `G_UseTargets` hands what it was given to every
target it fires, and a `target_explosion` among them uses it as the attacker of
its radius damage. baseq2 survives that by coincidence: its single read of
`attacker` sits behind a `DAMAGE_RADIUS` test that `T_RadiusDamage` always
passes. RA2 added a read in front of that one, twelve lines into `T_Damage` and
behind no condition at all, so *any* damage event carrying no attacker took the
server down. `T_Damage` now normalises once at the boundary --
`if (!attacker) attacker = world` -- rather than guarding each read: the reads
grow with every feature added to that function and the boundary does not, and
`world->client` is NULL, so every `attacker->client` test downstream still
answers what "no attacker" meant. The two reads the same NULL reaches *above*
`T_Damage`, `G_UseTargets`'s `activator->svflags` and `trigger_key_use`'s
`activator->client`, are guarded where they read it. Reported against six donors
at once by the sibling Colosseum tree, which hit it in production.

That review was not exhaustive, so treat this as materially safer than the
reconstruction rather than as audited. Two things it said when it was written
are no longer true: the tree *is* run against a live `q2proded` now, by the
play-test harness, and the vanilla files no longer all keep their original
behaviour -- `g_combat.c`, `g_utils.c` and `g_trigger.c` carry the NULL-attacker
fix above, and `g_utils.c` the `menu_centerprint` guard before it.
Known-unchanged: `shared_shared.c` is vendored verbatim and not pruned.
