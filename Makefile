# Rocket Arena 2 on the Q2PRO game API.
#
# Same six configurations as the reconstruction branch, but this tree is not
# byte-matching anything: it is RA2 v2.25 with Q2PRO's entire baseq2 commit
# history replayed on top, so the flags here are chosen for a working build
# rather than to reproduce the 1999 compiler's output.

BUILD_DEBUG_DIR=debug
BUILD_RELEASE_DIR=release
BUILD_WIN32_DEBUG_DIR=debug-win32
BUILD_WIN32_RELEASE_DIR=release-win32
BUILD_WIN64_DEBUG_DIR=debug-win64
BUILD_WIN64_RELEASE_DIR=release-win64

# Q2PRO names the game library after the CPU it was built for and looks for
# game<cpu>.so / game<cpu>.dll next to the mod directory.
ARCH?=x86_64
M32?=

CC=gcc

# config.h is Q2PRO's build configuration for a standalone game library;
# shared/ holds the engine headers the game links against (shared.h, game.h,
# list.h, m_flash.h, platform.h) plus their two .c files.
INCLUDES=-I. -Ishared
BASE_CFLAGS=-DHAVE_CONFIG_H $(INCLUDES) -Dstricmp=strcasecmp -Wall $(M32)
RELEASE_CFLAGS=$(BASE_CFLAGS) -O2
DEBUG_CFLAGS=$(BASE_CFLAGS) -g -O0
LDFLAGS=-ldl -lm $(M32)

SHLIBEXT=so

SHLIBCFLAGS=-fPIC
SHLIBLDFLAGS=-shared -Wl,--no-undefined

# MinGW cross-compilers.  stricmp is native to the Windows CRT so the
# strcasecmp remap vanilla Linux needs is dropped, nothing dlopen's on
# Windows so -ldl goes too, and -lws2_32 covers the Winsock calls
# net_compat.h switches gslog.c/gstats.c to under _WIN32.
CC_WIN32?=i686-w64-mingw32-gcc
CC_WIN64?=x86_64-w64-mingw32-gcc
WIN_BASE_CFLAGS=-DHAVE_CONFIG_H $(INCLUDES) -D__USE_MINGW_ANSI_STDIO=1 -Wall
WIN_RELEASE_CFLAGS=$(WIN_BASE_CFLAGS) -O2
WIN_DEBUG_CFLAGS=$(WIN_BASE_CFLAGS) -g -O0
WIN_LDFLAGS=-lm -lws2_32 -static-libgcc
WIN_SHLIBCFLAGS=
WIN_SHLIBLDFLAGS=-shared

# game.def restricts the DLL's export table to GetGameAPI, the only entry
# point the engine looks up.  Native ELF builds don't use a .def file.
EXTRA_LINK_INPUTS?=
WIN_EXTRA_LINK_INPUTS=game.def

DO_CC=$(CC) $(CFLAGS) -o $@ -c $<
DO_SHLIB_CC=$(CC) $(CFLAGS) $(SHLIBCFLAGS) -o $@ -c $<

TARGETS=$(BUILDDIR)/game$(ARCH).$(SHLIBEXT)

build_debug:
	@-mkdir $(BUILD_DEBUG_DIR)
	$(MAKE) targets BUILDDIR=$(BUILD_DEBUG_DIR) CFLAGS="$(DEBUG_CFLAGS)"

build_release:
	@-mkdir $(BUILD_RELEASE_DIR)
	$(MAKE) targets BUILDDIR=$(BUILD_RELEASE_DIR) CFLAGS="$(RELEASE_CFLAGS)"

build_win32_debug:
	@-mkdir $(BUILD_WIN32_DEBUG_DIR)
	$(MAKE) targets BUILDDIR=$(BUILD_WIN32_DEBUG_DIR) CFLAGS="$(WIN_DEBUG_CFLAGS)" \
		CC=$(CC_WIN32) ARCH=x86 SHLIBEXT=dll SHLIBCFLAGS="$(WIN_SHLIBCFLAGS)" \
		SHLIBLDFLAGS="$(WIN_SHLIBLDFLAGS)" LDFLAGS="$(WIN_LDFLAGS)" \
		EXTRA_LINK_INPUTS="$(WIN_EXTRA_LINK_INPUTS)"

build_win32_release:
	@-mkdir $(BUILD_WIN32_RELEASE_DIR)
	$(MAKE) targets BUILDDIR=$(BUILD_WIN32_RELEASE_DIR) CFLAGS="$(WIN_RELEASE_CFLAGS)" \
		CC=$(CC_WIN32) ARCH=x86 SHLIBEXT=dll SHLIBCFLAGS="$(WIN_SHLIBCFLAGS)" \
		SHLIBLDFLAGS="$(WIN_SHLIBLDFLAGS)" LDFLAGS="$(WIN_LDFLAGS)" \
		EXTRA_LINK_INPUTS="$(WIN_EXTRA_LINK_INPUTS)"

build_win64_debug:
	@-mkdir $(BUILD_WIN64_DEBUG_DIR)
	$(MAKE) targets BUILDDIR=$(BUILD_WIN64_DEBUG_DIR) CFLAGS="$(WIN_DEBUG_CFLAGS)" \
		CC=$(CC_WIN64) ARCH=x86_64 SHLIBEXT=dll SHLIBCFLAGS="$(WIN_SHLIBCFLAGS)" \
		SHLIBLDFLAGS="$(WIN_SHLIBLDFLAGS)" LDFLAGS="$(WIN_LDFLAGS)" \
		EXTRA_LINK_INPUTS="$(WIN_EXTRA_LINK_INPUTS)"

build_win64_release:
	@-mkdir $(BUILD_WIN64_RELEASE_DIR)
	$(MAKE) targets BUILDDIR=$(BUILD_WIN64_RELEASE_DIR) CFLAGS="$(WIN_RELEASE_CFLAGS)" \
		CC=$(CC_WIN64) ARCH=x86_64 SHLIBEXT=dll SHLIBCFLAGS="$(WIN_SHLIBCFLAGS)" \
		SHLIBLDFLAGS="$(WIN_SHLIBLDFLAGS)" LDFLAGS="$(WIN_LDFLAGS)" \
		EXTRA_LINK_INPUTS="$(WIN_EXTRA_LINK_INPUTS)"

all: build_debug build_release

win32: build_win32_debug build_win32_release
win64: build_win64_debug build_win64_release
windows: win32 win64

targets: $(TARGETS)

GAME_OBJS = \
	$(BUILDDIR)/arena.o \
	$(BUILDDIR)/g_ai.o \
	$(BUILDDIR)/g_cmds.o \
	$(BUILDDIR)/g_combat.o \
	$(BUILDDIR)/g_func.o \
	$(BUILDDIR)/g_items.o \
	$(BUILDDIR)/g_main.o \
	$(BUILDDIR)/g_misc.o \
	$(BUILDDIR)/g_monster.o \
	$(BUILDDIR)/g_phys.o \
	$(BUILDDIR)/g_ptrs.o \
	$(BUILDDIR)/g_save.o \
	$(BUILDDIR)/g_spawn.o \
	$(BUILDDIR)/g_svcmds.o \
	$(BUILDDIR)/g_target.o \
	$(BUILDDIR)/g_trigger.o \
	$(BUILDDIR)/g_turret.o \
	$(BUILDDIR)/g_utils.o \
	$(BUILDDIR)/g_weapon.o \
	$(BUILDDIR)/gslog.o \
	$(BUILDDIR)/maploop.o \
	$(BUILDDIR)/menu.o \
	$(BUILDDIR)/p_client.o \
	$(BUILDDIR)/p_hud.o \
	$(BUILDDIR)/p_trail.o \
	$(BUILDDIR)/p_view.o \
	$(BUILDDIR)/p_weapon.o \
	$(BUILDDIR)/ra2stats.o \
	$(BUILDDIR)/ra2menus.o \
	$(BUILDDIR)/shared_m_flash.o \
	$(BUILDDIR)/shared_shared.o

$(BUILDDIR)/game$(ARCH).$(SHLIBEXT) : $(GAME_OBJS)
	$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(GAME_OBJS) $(EXTRA_LINK_INPUTS) $(LDFLAGS)

$(BUILDDIR)/%.o : %.c
	$(DO_SHLIB_CC)

#####

clean: clean-debug clean-release clean-win32 clean-win64

clean-debug:
	$(MAKE) clean2 BUILDDIR=$(BUILD_DEBUG_DIR)

clean-release:
	$(MAKE) clean2 BUILDDIR=$(BUILD_RELEASE_DIR)

clean-win32:
	$(MAKE) clean2 BUILDDIR=$(BUILD_WIN32_DEBUG_DIR)
	$(MAKE) clean2 BUILDDIR=$(BUILD_WIN32_RELEASE_DIR)

clean-win64:
	$(MAKE) clean2 BUILDDIR=$(BUILD_WIN64_DEBUG_DIR)
	$(MAKE) clean2 BUILDDIR=$(BUILD_WIN64_RELEASE_DIR)

clean2:
	-rm -f $(GAME_OBJS)
