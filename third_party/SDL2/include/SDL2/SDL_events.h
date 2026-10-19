#ifndef SDL_EVENTS_H_
#define SDL_EVENTS_H_

#include "SDL_stdinc.h"
#include "SDL_scancode.h"
#include "SDL_gamecontroller.h"

extern "C" {

#define SDL_RELEASED 0
#define SDL_PRESSED 1

typedef Sint32 SDL_Keycode;

typedef struct SDL_Keysym {
    SDL_Scancode scancode;
    SDL_Keycode sym;
    Uint16 mod;
    Uint32 unused;
} SDL_Keysym;

typedef enum SDL_EventType {
    SDL_FIRSTEVENT = 0,
    SDL_QUIT = 0x100,

    SDL_KEYDOWN = 0x300,
    SDL_KEYUP = 0x301,

    SDL_JOYAXISMOTION = 0x600,
    SDL_JOYBALLMOTION = 0x601,
    SDL_JOYHATMOTION = 0x602,
    SDL_JOYBUTTONDOWN = 0x603,
    SDL_JOYBUTTONUP = 0x604,
    SDL_JOYDEVICEADDED = 0x605,
    SDL_JOYDEVICEREMOVED = 0x606,

    SDL_CONTROLLERAXISMOTION = 0x650,
    SDL_CONTROLLERBUTTONDOWN = 0x651,
    SDL_CONTROLLERBUTTONUP = 0x652,
    SDL_CONTROLLERDEVICEADDED = 0x653,
    SDL_CONTROLLERDEVICEREMOVED = 0x654,
    SDL_CONTROLLERDEVICEREMAPPED = 0x655,

    SDL_USEREVENT = 0x8000,
    SDL_LASTEVENT = 0xFFFF
} SDL_EventType;

typedef struct SDL_CommonEvent {
    Uint32 type;
    Uint32 timestamp;
} SDL_CommonEvent;

typedef struct SDL_QuitEvent {
    Uint32 type;
    Uint32 timestamp;
} SDL_QuitEvent;

typedef struct SDL_KeyboardEvent {
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    Uint8 state;
    Uint8 repeat;
    Uint8 padding2;
    Uint8 padding3;
    SDL_Keysym keysym;
} SDL_KeyboardEvent;

typedef struct SDL_ControllerAxisEvent {
    Uint32 type;
    Uint32 timestamp;
    SDL_JoystickID which;
    Uint8 axis;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
    Sint16 value;
    Uint16 padding4;
} SDL_ControllerAxisEvent;

typedef struct SDL_ControllerButtonEvent {
    Uint32 type;
    Uint32 timestamp;
    SDL_JoystickID which;
    Uint8 button;
    Uint8 state;
    Uint8 padding1;
    Uint8 padding2;
} SDL_ControllerButtonEvent;

typedef struct SDL_ControllerDeviceEvent {
    Uint32 type;
    Uint32 timestamp;
    Sint32 which;
} SDL_ControllerDeviceEvent;

typedef union SDL_Event {
    Uint32 type;
    SDL_CommonEvent common;
    SDL_KeyboardEvent key;
    SDL_ControllerAxisEvent caxis;
    SDL_ControllerButtonEvent cbutton;
    SDL_ControllerDeviceEvent cdevice;
    SDL_QuitEvent quit;
    Uint8 padding[56];
} SDL_Event;

void SDL_PumpEvents(void);
int SDL_PollEvent(SDL_Event* event);
int SDL_PushEvent(SDL_Event* event);

const Uint8* SDL_GetKeyboardState(int* numkeys);
const char* SDL_GetScancodeName(SDL_Scancode scancode);
SDL_Scancode SDL_GetScancodeFromName(const char* name);

} // extern "C"

#endif // SDL_EVENTS_H_
