#ifndef STRIKERS_INPUT_MANAGER_HPP_
#define STRIKERS_INPUT_MANAGER_HPP_

#include "input/config_loader.hpp"
#include "input/input_state.hpp"

#include <SDL2/SDL.h>
#include <array>
#include <cstdint>
#include <filesystem>

namespace strikers::input {

/**
 * Implementação concreta do subsistema portátil de entrada (PC HAL - SDL2).
 * Captura eventos de teclado (`SDL_Scancode`) e controles (`SDL_GameController` / XInput / DualShock),
 * traduzindo-os para `LogicalInputState` sem vazar primitivos da SDL2 para o loop de simulação.
 */
class InputManager final : public IInputSource {
public:
    InputManager();
    explicit InputManager(InputBindingsConfig config);
    ~InputManager() override;

    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;
    InputManager(InputManager&&) = delete;
    InputManager& operator=(InputManager&&) = delete;

    /**
     * Inicializa os subsistemas de eventos, joystick e gamecontroller da SDL2
     * e abre quaisquer controles compatíveis conectados.
     */
    bool Initialize();

    /**
     * Fecha todos os handles de `SDL_GameController` abertos e libera recursos.
     */
    void Shutdown() noexcept;

    /**
     * Carrega um arquivo `bindings.json` e atualiza os perfis em tempo de execução.
     */
    void LoadBindings(const std::filesystem::path& filepath);

    /**
     * Salva os mapeamentos atuais em `filepath`.
     */
    void SaveBindings(const std::filesystem::path& filepath) const;

    /**
     * Drena a fila de eventos da SDL2 (`SDL_PollEvent`) e atualiza o snapshot do frame atual
     * para todos os 4 jogadores locais. Retorna `false` caso um evento `SDL_QUIT` seja recebido.
     */
    bool PumpEvents();

    /**
     * Implementação do contrato puro de `specs/INPUT.md`:
     * Retorna o `LogicalInputState` do jogador `player_index` (`0..3`).
     */
    [[nodiscard]] LogicalInputState Poll(int player_index) override;

    [[nodiscard]] bool QuitRequested() const noexcept { return quit_requested_; }
    [[nodiscard]] bool IsInitialized() const noexcept { return initialized_; }
    [[nodiscard]] const InputBindingsConfig& GetBindingsConfig() const noexcept { return config_; }

private:
    struct ControllerSlot {
        SDL_GameController* handle{nullptr};
        SDL_JoystickID instance_id{-1};
        std::array<bool, SDL_CONTROLLER_BUTTON_MAX> event_buttons{};
        std::array<Sint16, SDL_CONTROLLER_AXIS_MAX> event_axes{};
    };

    void OpenAvailableControllers();
    void OnControllerAdded(int device_index);
    void OnControllerRemoved(SDL_JoystickID instance_id);
    void RebuildPlayerState(int player_index);
    [[nodiscard]] static float NormalizeAxisValue(Sint16 raw_value, float deadzone) noexcept;

    InputBindingsConfig config_;
    bool initialized_{false};
    bool owned_sdl_init_{false};
    bool quit_requested_{false};

    std::array<bool, SDL_NUM_SCANCODES> event_key_state_{};
    std::array<ControllerSlot, kMaxLocalPlayers> controllers_{};
    std::array<LogicalInputState, kMaxLocalPlayers> current_states_{};
    std::array<std::uint32_t, kMaxLocalPlayers> previous_held_masks_{};
};

} // namespace strikers::input

#endif // STRIKERS_INPUT_MANAGER_HPP_
