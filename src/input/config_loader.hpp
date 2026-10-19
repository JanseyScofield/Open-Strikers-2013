#ifndef STRIKERS_INPUT_CONFIG_LOADER_HPP_
#define STRIKERS_INPUT_CONFIG_LOADER_HPP_

#include "input/input_state.hpp"

#include <SDL2/SDL.h>
#include <array>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace strikers::input {

/**
 * Mapeamento individual de teclas (SDL_Scancode) e controles (SDL_GameController)
 * para um jogador local.
 */
struct PlayerBindingProfile {
    int player_index{0};
    std::string profile_name{"Player 1"};
    bool enable_keyboard{true};
    bool enable_gamepad{true};
    int gamepad_device_index{0};
    float analog_deadzone{0.18f};
    float trigger_threshold{0.35f};

    /// Mapeamento de ações lógicas para uma ou mais teclas (SDL_Scancode).
    std::map<LogicalAction, std::vector<SDL_Scancode>> key_bindings;

    /// Mapeamento de ações lógicas para um ou mais botões de GameController (XInput/DualShock).
    std::map<LogicalAction, std::vector<SDL_GameControllerButton>> pad_button_bindings;

    /// Eixos analógicos primários de movimento no GameController.
    SDL_GameControllerAxis move_axis_x{SDL_CONTROLLER_AXIS_LEFTX};
    SDL_GameControllerAxis move_axis_y{SDL_CONTROLLER_AXIS_LEFTY};
};

/**
 * Configuração global de mapeamento para os 4 jogadores locais.
 */
struct InputBindingsConfig {
    std::string schema_version{"1.0.0"};
    std::array<PlayerBindingProfile, kMaxLocalPlayers> players{};
};

/**
 * Carregador e serializador do contrato JSON em `config/bindings.json`.
 */
class ConfigLoader {
public:
    /**
     * Gera o perfil padrão ergonômico de 4 jogadores (WASD + J/K/L/U/I/Espaço/Enter no P1,
     * Setas + Teclado Numérico no P2, e GameController XInput/DualShock em P1..P4).
     */
    [[nodiscard]] static InputBindingsConfig CreateDefaultConfig();

    /**
     * Carrega e valida as configurações de `config/bindings.json`.
     * Lança `std::runtime_error` caso o arquivo não exista ou apresente estrutura inválida.
     */
    [[nodiscard]] static InputBindingsConfig LoadFromFile(const std::filesystem::path& filepath);

    /**
     * Salva a configuração atual em formato JSON identado em `filepath`.
     */
    static void SaveToFile(const InputBindingsConfig& config, const std::filesystem::path& filepath);

    /// Utilitários de conversão entre LogicalAction e chaves textuais do JSON.
    [[nodiscard]] static std::string ActionToString(LogicalAction action);
    [[nodiscard]] static LogicalAction StringToAction(const std::string& name);

    /// Utilitários de conversão entre nomes de teclas/botões e enums SDL2.
    [[nodiscard]] static SDL_Scancode ParseScancode(const std::string& key_name);
    [[nodiscard]] static std::string ScancodeToString(SDL_Scancode scancode);

    [[nodiscard]] static SDL_GameControllerButton ParseControllerButton(const std::string& btn_name);
    [[nodiscard]] static std::string ControllerButtonToString(SDL_GameControllerButton button);
};

} // namespace strikers::input

#endif // STRIKERS_INPUT_CONFIG_LOADER_HPP_
