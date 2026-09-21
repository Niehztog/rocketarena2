# Rocket Arena 2 — Q2PRO enhancements

This branch (`q2pro-enhancements`) takes Rocket Arena 2 v2.25 and brings it up
to date, in two steps.

**Every commit Q2PRO has made to id's `baseq2` game source is replayed on top
of it** — 188 of them, from the 3.20 import to current master. That brings in
the modern game API, frame-number timers, the rewritten savegame system,
protocol extensions, and twenty years of accumulated crash, overflow and
out-of-bounds fixes. RA2's own code comes through intact: 41 of its 43 cvars,
all 39 client commands, all 111 spawn classnames and the grapple.

**Both of RA2's remote logging features are gone.** The GameSpy stats SDK was
six third-party files that uploaded per-round statistics to
`gamestats.gamespy.com`, offline for years — and still paid for a DNS lookup
and a connect attempt at the start of every round. The numbers it collected are
now written locally instead, one JSON object per round, by a small module that
links nothing beyond libc.

RA2's own `netlog` streamed each kill line by UDP to a collector that has been
gone just as long, so it went too — that is the missing pair of cvars, `netlog`
itself and the `public` registration that existed only to gate it. With them go
the four `exit(1)` calls that could terminate a running server from inside the
game library, at the first frag rather than at startup. `logfile 2` still
writes `stdlog.log` locally. Nothing in the tree opens a socket now, on any
target.

* [doc/q2pro-port.md](doc/q2pro-port.md) — how the replay was done, what was
  carried across by hand, what replaced GameSpy, why `netlog` went with it, and
  what was checked.

Build it the same way as the reconstruction: `make` for native, `make windows`
for the MinGW cross builds. All six configurations build clean.

**This is not the reconstruction.** The byte-exact reconstruction lives on
[`main`](https://github.com/Niehztog/rocketarena2/tree/main). This tree has been
reformatted, restructured and bug-fixed, and no longer matches the original
binaries — don't use it for address matching.

---

# Rocket Arena 2 — Source Reconstruction

This project reconstructs the source code of Rocket Arena 2, a 1999 Quake 2 deathmatch mod whose original source was never released, using evidence recovered from the mod's own shipped binaries. The reconstruction is nearly complete, builds cleanly, and is ready to play. See the [wiki](https://github.com/Niehztog/rocketarena2/wiki) for the full history, methodology, and documentation.

## Security

This reconstruction faithfully preserves the original code, including its security vulnerabilities - (see the [wiki](https://github.com/Niehztog/rocketarena2/wiki/Deliberately-Kept-Original-Bugs) for specifics). This repository is meant as a historical archive and a base for future forks, not for use on a live server unless those issues are patched first.

## See also
* [OpenRA2](https://github.com/packetflinger/openra2) - An open source remake of the Rocket Arena mod for Quake 2
* [RA3 1.76 Decompilation](https://github.com/inolen/ra3_176_decomp) - Decompilation of the Rocket Arena 3 qagamei386.so binary from RA3 1.76

## Contact

Questions or comments: niehz.tog@gmail.com
