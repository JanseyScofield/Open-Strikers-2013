#ifndef STRIKERS_RENDER_WINDOW_HPP_
#define STRIKERS_RENDER_WINDOW_HPP_

#include <SDL2/SDL.h>
#include <string>
#include <string_view>

namespace strikers::render {

/**
 * Parâmetros de configuração da janela OpenGL (padrão 1280x720 16:9 com VSync).
 */
struct WindowConfig {
    std::string title{"Open Strikers 2013 - Graphics & Physics Sandbox"};
    int width{1280};
    int height{720};
    bool vsync{true};
    bool resizable{true};
};

/**
 * Gerenciador RAII da janela SDL2 e do contexto OpenGL.
 */
class Window {
public:
    Window() = default;
    explicit Window(WindowConfig config);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    /**
     * Inicializa o subsistema de vídeo da SDL2, cria a janela OpenGL 1280x720
     * e ativa VSync (quando disponível no driver).
     */
    bool Initialize();

    /**
     * Destrói o contexto OpenGL e a janela SDL2 de forma limpa.
     */
    void Shutdown() noexcept;

    /**
     * Atualiza o viewport OpenGL (caso a janela tenha sido redimensionada)
     * e limpa os buffers de cor e profundidade.
     */
    void BeginFrame();

    /**
     * Realiza o swap de buffers OpenGL (`SDL_GL_SwapWindow`).
     */
    void EndFrame() noexcept;

    void SetTitle(std::string_view title);

    [[nodiscard]] int GetWidth() const noexcept { return width_; }
    [[nodiscard]] int GetHeight() const noexcept { return height_; }
    [[nodiscard]] float GetAspectRatio() const noexcept {
        return (height_ > 0) ? (static_cast<float>(width_) / static_cast<float>(height_)) : (16.0f / 9.0f);
    }
    [[nodiscard]] bool IsInitialized() const noexcept { return window_ != nullptr; }
    [[nodiscard]] bool HasOpenGLContext() const noexcept { return gl_context_ != nullptr; }
    [[nodiscard]] SDL_Window* GetNativeWindow() const noexcept { return window_; }

private:
    WindowConfig config_{};
    SDL_Window* window_{nullptr};
    SDL_GLContext gl_context_{nullptr};
    int width_{1280};
    int height_{720};
    bool owned_video_subsystem_{false};
};

} // namespace strikers::render

#endif // STRIKERS_RENDER_WINDOW_HPP_
