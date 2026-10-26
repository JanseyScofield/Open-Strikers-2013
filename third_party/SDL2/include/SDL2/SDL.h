#ifndef SDL_H_
#define SDL_H_

#include "SDL_stdinc.h"
#include "SDL_scancode.h"
#include "SDL_gamecontroller.h"
#include "SDL_video.h"
#include "SDL_events.h"

extern "C" {

#define SDL_INIT_TIMER          0x00000001u
#define SDL_INIT_AUDIO          0x00000010u
#define SDL_INIT_VIDEO          0x00000020u
#define SDL_INIT_JOYSTICK       0x00000200u
#define SDL_INIT_HAPTIC         0x00001000u
#define SDL_INIT_GAMECONTROLLER 0x00002000u
#define SDL_INIT_EVENTS         0x00004000u
#define SDL_INIT_SENSOR         0x00008000u
#define SDL_INIT_NOPARACHUTE    0x00100000u
#define SDL_INIT_EVERYTHING ( \
    SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS | \
    SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMECONTROLLER | SDL_INIT_SENSOR \
)

#define SDL_HINT_VIDEODRIVER "SDL_VIDEODRIVER"
#define SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS "SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS"

typedef struct SDL_version {
    Uint8 major;
    Uint8 minor;
    Uint8 patch;
} SDL_version;

int SDL_Init(Uint32 flags);
int SDL_InitSubSystem(Uint32 flags);
void SDL_QuitSubSystem(Uint32 flags);
Uint32 SDL_WasInit(Uint32 flags);
void SDL_Quit(void);

const char* SDL_GetError(void);
void SDL_ClearError(void);
SDL_bool SDL_SetHint(const char* name, const char* value);
void SDL_GetVersion(SDL_version* ver);
void SDL_Delay(Uint32 ms);
Uint32 SDL_GetTicks(void);
Uint64 SDL_GetTicks64(void);

} // extern "C"

#endif // SDL_H_
