# Rocket Arena 2 on the Q2PRO game API

This branch (`feature/q2pro-port`) is Rocket Arena 2 v2.25 with every commit
Q2PRO has made to id's `baseq2` game source — 188 of them — replayed on top of
it. That brings in the modern game API, frame-number timers, the rewritten
savegame system, and twenty years of accumulated crash and overflow fixes,
while keeping RA2's cvars, commands, spawn classnames and arena logic intact.

**It is not the reconstruction.** The byte-exact reconstruction lives on
`main-github`; this tree has been reformatted and restructured and no longer
matches the original binaries. Don't use it for address matching.

* [doc/q2pro-port.md](doc/q2pro-port.md) — how the replay was done, what was
  carried across by hand, and what was checked.

Build it the same way as the reconstruction: `make` for native, `make windows`
for the MinGW cross builds.

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
