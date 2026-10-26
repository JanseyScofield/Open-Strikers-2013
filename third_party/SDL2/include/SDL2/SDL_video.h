#ifndef SDL_VIDEO_H_
#define SDL_VIDEO_H_

#include "SDL_stdinc.h"

extern "C" {

typedef struct SDL_Window SDL_Window;
typedef void* SDL_GLContext;

#define SDL_WINDOWPOS_UNDEFINED_MASK    0x1FFF0000u
#define SDL_WINDOWPOS_UNDEFINED_DISPLAY(X) (SDL_WINDOWPOS_UNDEFINED_MASK | (X))
#define SDL_WINDOWPOS_UNDEFINED         SDL_WINDOWPOS_UNDEFINED_DISPLAY(0)

#define SDL_WINDOWPOS_CENTERED_MASK     0x2FFF0000u
#define SDL_WINDOWPOS_CENTERED_DISPLAY(X) (SDL_WINDOWPOS_CENTERED_MASK | (X))
#define SDL_WINDOWPOS_CENTERED          SDL_WINDOWPOS_CENTERED_DISPLAY(0)

typedef enum SDL_WindowFlags {
    SDL_WINDOW_FULLSCREEN         = 0x00000001u,
    SDL_WINDOW_OPENGL             = 0x00000002u,
    SDL_WINDOW_SHOWN              = 0x00000004u,
    SDL_WINDOW_HIDDEN             = 0x00000008u,
    SDL_WINDOW_BORDERLESS         = 0x00000010u,
    SDL_WINDOW_RESIZABLE          = 0x00000020u,
    SDL_WINDOW_MINIMIZED          = 0x00000040u,
    SDL_WINDOW_MAXIMIZED          = 0x00000080u,
    SDL_WINDOW_INPUT_GRABBED      = 0x00000100u,
    SDL_WINDOW_INPUT_FOCUS        = 0x00000200u,
    SDL_WINDOW_MOUSE_FOCUS        = 0x00000400u,
    SDL_WINDOW_FULLSCREEN_DESKTOP = (SDL_WINDOW_FULLSCREEN | 0x00001000u),
    SDL_WINDOW_ALLOW_HIGHDPI      = 0x00002000u
} SDL_WindowFlags;

typedef enum SDL_WindowEventID {
    SDL_WINDOWEVENT_NONE = 0,
    SDL_WINDOWEVENT_SHOWN = 1,
    SDL_WINDOWEVENT_HIDDEN = 2,
    SDL_WINDOWEVENT_EXPOSED = 3,
    SDL_WINDOWEVENT_MOVED = 4,
    SDL_WINDOWEVENT_RESIZED = 5,
    SDL_WINDOWEVENT_SIZE_CHANGED = 6,
    SDL_WINDOWEVENT_MINIMIZED = 7,
    SDL_WINDOWEVENT_MAXIMIZED = 8,
    SDL_WINDOWEVENT_RESTORED = 9,
    SDL_WINDOWEVENT_ENTER = 10,
    SDL_WINDOWEVENT_LEAVE = 11,
    SDL_WINDOWEVENT_FOCUS_GAINED = 12,
    SDL_WINDOWEVENT_FOCUS_LOST = 13,
    SDL_WINDOWEVENT_CLOSE = 14
} SDL_WindowEventID;

typedef enum SDL_GLattr {
    SDL_GL_RED_SIZE = 0,
    SDL_GL_GREEN_SIZE = 1,
    SDL_GL_BLUE_SIZE = 2,
    SDL_GL_ALPHA_SIZE = 3,
    SDL_GL_BUFFER_SIZE = 4,
    SDL_GL_DOUBLEBUFFER = 5,
    SDL_GL_DEPTH_SIZE = 6,
    SDL_GL_STENCIL_SIZE = 7,
    SDL_GL_ACCUM_RED_SIZE = 8,
    SDL_GL_ACCUM_GREEN_SIZE = 9,
    SDL_GL_ACCUM_BLUE_SIZE = 10,
    SDL_GL_ACCUM_ALPHA_SIZE = 11,
    SDL_GL_STEREO = 12,
    SDL_GL_MULTISAMPLEBUFFERS = 13,
    SDL_GL_MULTISAMPLESAMPLES = 14,
    SDL_GL_ACCELERATED_VISUAL = 15,
    SDL_GL_RETAINED_BACKING = 16,
    SDL_GL_CONTEXT_MAJOR_VERSION = 17,
    SDL_GL_CONTEXT_MINOR_VERSION = 18,
    SDL_GL_CONTEXT_EGL = 19,
    SDL_GL_CONTEXT_FLAGS = 20,
    SDL_GL_CONTEXT_PROFILE_MASK = 21,
    SDL_GL_SHARE_WITH_CURRENT_CONTEXT = 22,
    SDL_GL_FRAMEBUFFER_SRGB_CAPABLE = 23,
    SDL_GL_CONTEXT_RELEASE_BEHAVIOR = 24,
    SDL_GL_CONTEXT_RESET_NOTIFICATION = 25,
    SDL_GL_CONTEXT_NO_ERROR = 26
} SDL_GLattr;

typedef enum SDL_GLprofile {
    SDL_GL_CONTEXT_PROFILE_CORE          = 0x0001,
    SDL_GL_CONTEXT_PROFILE_COMPATIBILITY = 0x0002,
    SDL_GL_CONTEXT_PROFILE_ES            = 0x0004
} SDL_GLprofile;

SDL_Window* SDL_CreateWindow(const char* title, int x, int y, int w, int h, Uint32 flags);
void SDL_DestroyWindow(SDL_Window* window);
void SDL_SetWindowTitle(SDL_Window* window, const char* title);
void SDL_GetWindowSize(SDL_Window* window, int* w, int* h);

int SDL_GL_SetAttribute(SDL_GLattr attr, int value);
int SDL_GL_GetAttribute(SDL_GLattr attr, int* value);
void SDL_GL_ResetAttributes(void);
SDL_GLContext SDL_GL_CreateContext(SDL_Window* window);
void SDL_GL_DeleteContext(SDL_GLContext context);
int SDL_GL_MakeCurrent(SDL_Window* window, SDL_GLContext context);
int SDL_GL_SetSwapInterval(int interval);
int SDL_GL_GetSwapInterval(void);
void SDL_GL_SwapWindow(SDL_Window* window);
void SDL_GL_GetDrawableSize(SDL_Window* window, int* w, int* h);
void* SDL_GL_GetProcAddress(const char* proc);

} // extern "C"

#endif // SDL_VIDEO_H_
