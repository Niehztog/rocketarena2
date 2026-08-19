// Minimal q2pro build configuration for this standalone game library.
// The engine normally generates this with meson; a game library only needs
// the switches that shape the shared headers.
#define USE_CLIENT              0
#define USE_SERVER              0
#define USE_PROTOCOL_EXTENSIONS 1
#define USE_NEW_GAME_API        1
#define USE_DEBUG               0
#define USE_FPS                 0
#define USE_MD5                 0
#define USE_LITTLE_ENDIAN       1
#define VERSION                 "ra2-q2pro"
#define CPUSTRING               "x86"
#define BUILDSTRING             "portable"
