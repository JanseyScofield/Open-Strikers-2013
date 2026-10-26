#include "render/window.hpp"

#include <GL/gl.h>
#include <iostream>

namespace strikers::render {

Window::Window(WindowConfig config)
    : config_(std::move(config)),
      width_(config_.width),
      height_(config_.height) {}

Window::~Window() {
    Shutdown();
}

bool Window::Initialize() {
    if (window_ != nullptr) {
        return true;
    }

    if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0u) {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
            // Fallback automático para driver dummy caso executado em servidor sem display X11/Wayland
            SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
            if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
                std::cerr << "[Window] Falha ao inicializar SDL_INIT_VIDEO: " << SDL_GetError() << "\n";
                return false;
            }
        }
        owned_video_subsystem_ = true;
    }

    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

    Uint32 win_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
    if (config_.resizable) {
        win_flags |= SDL_WINDOW_RESIZABLE;
    }

    window_ = SDL_CreateWindow(
        config_.title.c_str(),
        static_cast<int>(SDL_WINDOWPOS_CENTERED),
        static_cast<int>(SDL_WINDOWPOS_CENTERED),
        config_.width,
        config_.height,
        win_flags
    );

    if (window_ == nullptr) {
        // Fallback sem a flag OPENGL se estiver no driver dummy puro
        win_flags &= ~static_cast<Uint32>(SDL_WINDOW_OPENGL);
        window_ = SDL_CreateWindow(
            config_.title.c_str(),
            static_cast<int>(SDL_WINDOWPOS_CENTERED),
            static_cast<int>(SDL_WINDOWPOS_CENTERED),
            config_.width,
            config_.height,
            win_flags
        );
        if (window_ == nullptr) {
            std::cerr << "[Window] Falha em SDL_CreateWindow: " << SDL_GetError() << "\n";
            return false;
        }
    }

    gl_context_ = SDL_GL_CreateContext(window_);
    if (gl_context_ != nullptr) {
        SDL_GL_MakeCurrent(window_, gl_context_);
        if (config_.vsync) {
            if (SDL_GL_SetSwapInterval(1) != 0) {
                SDL_ClearError();
            }
        } else {
            SDL_GL_SetSwapInterval(0);
        }

        SDL_GL_GetDrawableSize(window_, &width_, &height_);
        glViewport(0, 0, width_, height_);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_LINE_SMOOTH);
    } else {
        SDL_ClearError();
    }

    return true;
}

void Window::Shutdown() noexcept {
    if (gl_context_ != nullptr) {
        SDL_GL_DeleteContext(gl_context_);
        gl_context_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    if (owned_video_subsystem_) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        owned_video_subsystem_ = false;
    }
}

void Window::BeginFrame() {
    if (window_ == nullptr || gl_context_ == nullptr) {
        return;
    }

    int draw_w = width_;
    int draw_h = height_;
    SDL_GL_GetDrawableSize(window_, &draw_w, &draw_h);
    if (draw_w > 0 && draw_h > 0 && (draw_w != width_ || draw_h != height_)) {
        width_ = draw_w;
        height_ = draw_h;
    }

    glViewport(0, 0, width_, height_);
    glClearColor(0.08f, 0.12f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Window::EndFrame() noexcept {
    if (window_ != nullptr && gl_context_ != nullptr) {
        SDL_GL_SwapWindow(window_);
    }
}

void Window::SetTitle(std::string_view title) {
    config_.title = std::string(title);
    if (window_ != nullptr) {
        SDL_SetWindowTitle(window_, config_.title.c_str());
    }
}

} // namespace strikers::render
