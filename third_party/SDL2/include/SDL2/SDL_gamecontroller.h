#ifndef SDL_GAMECONTROLLER_H_
#define SDL_GAMECONTROLLER_H_

#include "SDL_stdinc.h"

extern "C" {

typedef Sint32 SDL_JoystickID;
struct _SDL_Joystick;
typedef struct _SDL_Joystick SDL_Joystick;

struct _SDL_GameController;
typedef struct _SDL_GameController SDL_GameController;

typedef enum SDL_GameControllerAxis {
    SDL_CONTROLLER_AXIS_INVALID = -1,
    SDL_CONTROLLER_AXIS_LEFTX = 0,
    SDL_CONTROLLER_AXIS_LEFTY = 1,
    SDL_CONTROLLER_AXIS_RIGHTX = 2,
    SDL_CONTROLLER_AXIS_RIGHTY = 3,
    SDL_CONTROLLER_AXIS_TRIGGERLEFT = 4,
    SDL_CONTROLLER_AXIS_TRIGGERRIGHT = 5,
    SDL_CONTROLLER_AXIS_MAX = 6
} SDL_GameControllerAxis;

typedef enum SDL_GameControllerButton {
    SDL_CONTROLLER_BUTTON_INVALID = -1,
    SDL_CONTROLLER_BUTTON_A = 0,
    SDL_CONTROLLER_BUTTON_B = 1,
    SDL_CONTROLLER_BUTTON_X = 2,
    SDL_CONTROLLER_BUTTON_Y = 3,
    SDL_CONTROLLER_BUTTON_BACK = 4,
    SDL_CONTROLLER_BUTTON_GUIDE = 5,
    SDL_CONTROLLER_BUTTON_START = 6,
    SDL_CONTROLLER_BUTTON_LEFTSTICK = 7,
    SDL_CONTROLLER_BUTTON_RIGHTSTICK = 8,
    SDL_CONTROLLER_BUTTON_LEFTSHOULDER = 9,
    SDL_CONTROLLER_BUTTON_RIGHTSHOULDER = 10,
    SDL_CONTROLLER_BUTTON_DPAD_UP = 11,
    SDL_CONTROLLER_BUTTON_DPAD_DOWN = 12,
    SDL_CONTROLLER_BUTTON_DPAD_LEFT = 13,
    SDL_CONTROLLER_BUTTON_DPAD_RIGHT = 14,
    SDL_CONTROLLER_BUTTON_MISC1 = 15,
    SDL_CONTROLLER_BUTTON_PADDLE1 = 16,
    SDL_CONTROLLER_BUTTON_PADDLE2 = 17,
    SDL_CONTROLLER_BUTTON_PADDLE3 = 18,
    SDL_CONTROLLER_BUTTON_PADDLE4 = 19,
    SDL_CONTROLLER_BUTTON_TOUCHPAD = 20,
    SDL_CONTROLLER_BUTTON_MAX = 21
} SDL_GameControllerButton;

int SDL_NumJoysticks(void);
SDL_JoystickID SDL_JoystickInstanceID(SDL_Joystick* joystick);

SDL_bool SDL_IsGameController(int joystick_index);
const char* SDL_GameControllerNameForIndex(int joystick_index);
SDL_GameController* SDL_GameControllerOpen(int joystick_index);
SDL_GameController* SDL_GameControllerFromInstanceID(SDL_JoystickID joyid);
SDL_GameController* SDL_GameControllerFromPlayerIndex(int player_index);
const char* SDL_GameControllerName(SDL_GameController* gamecontroller);
int SDL_GameControllerGetPlayerIndex(SDL_GameController* gamecontroller);
SDL_bool SDL_GameControllerGetAttached(SDL_GameController* gamecontroller);
SDL_Joystick* SDL_GameControllerGetJoystick(SDL_GameController* gamecontroller);
void SDL_GameControllerUpdate(void);

SDL_GameControllerAxis SDL_GameControllerGetAxisFromString(const char* str);
const char* SDL_GameControllerGetStringForAxis(SDL_GameControllerAxis axis);
Sint16 SDL_GameControllerGetAxis(SDL_GameController* gamecontroller, SDL_GameControllerAxis axis);

SDL_GameControllerButton SDL_GameControllerGetButtonFromString(const char* str);
const char* SDL_GameControllerGetStringForButton(SDL_GameControllerButton button);
Uint8 SDL_GameControllerGetButton(SDL_GameController* gamecontroller, SDL_GameControllerButton button);

void SDL_GameControllerClose(SDL_GameController* gamecontroller);

} // extern "C"

#endif // SDL_GAMECONTROLLER_H_
