#ifndef STRIKERS_INPUT_STATE_HPP_
#define STRIKERS_INPUT_STATE_HPP_

#include <cmath>
#include <cstdint>

namespace strikers::input {

/// Número máximo de jogadores locais suportados pelo motor (Wii 4 portas / PC 4 jogadores).
inline constexpr int kMaxLocalPlayers = 4;

/**
 * Máscara de bits de ações lógicas do motor de jogo (Strikers 2013).
 * Desacoplada de códigos físicos de hardware (WPAD/PAD do Wii ou SDL_Scancode/SDL_GameController).
 */
enum class LogicalAction : std::uint32_t {
    None            = 0u,

    // Ações Principais de Partida
    PassSwitch      = 1u << 0,  ///< Passe (com bola) / Troca de cursor (sem bola)
    ShootSlide      = 1u << 1,  ///< Chute (carregável) / Carrinho ou Dividida (defesa)
    DribbleSpecial  = 1u << 2,  ///< Drible / Evasão / Acionamento de Keshin ou Especial
    DashSprint      = 1u << 3,  ///< Corrida / Dash consome TP/Stamina

    // Modificadores Táticos e de Hissatsu
    TacticSwitch    = 1u << 4,  ///< Troca de Tática / Acionamento de Tática Especial
    HissatsuLevel   = 1u << 5,  ///< Seleção de Nível de Técnica (Hissatsu Lv1/Lv2/Lv3/Co-op)

    // Ações de Interface e Fluxo
    PauseMenu       = 1u << 6,  ///< Pause / Menu de partida
    Confirm         = 1u << 7,  ///< Confirmar seleção em menus/cut-ins
    Cancel          = 1u << 8,  ///< Cancelar seleção em menus

    // Flags Direcionais Digitais (D-Pad / Teclado)
    MoveUp          = 1u << 9,
    MoveDown        = 1u << 10,
    MoveLeft        = 1u << 11,
    MoveRight       = 1u << 12
};

[[nodiscard]] constexpr std::uint32_t ToMask(LogicalAction action) noexcept {
    return static_cast<std::uint32_t>(action);
}

[[nodiscard]] constexpr std::uint32_t operator|(LogicalAction lhs, LogicalAction rhs) noexcept {
    return ToMask(lhs) | ToMask(rhs);
}

[[nodiscard]] constexpr std::uint32_t operator|(std::uint32_t lhs, LogicalAction rhs) noexcept {
    return lhs | ToMask(rhs);
}

[[nodiscard]] constexpr std::uint32_t operator&(std::uint32_t lhs, LogicalAction rhs) noexcept {
    return lhs & ToMask(rhs);
}

/**
 * Vetor direcional normalizado no intervalo [-1.0f, +1.0f].
 * Convenção de coordenadas:
 *   x: -1.0f (Esquerda) .. +1.0f (Direita)
 *   y: -1.0f (Cima)     .. +1.0f (Baixo)
 */
struct DirectionalAxis {
    float x{0.0f};
    float y{0.0f};

    [[nodiscard]] float Magnitude() const noexcept {
        return std::sqrt(x * x + y * y);
    }

    [[nodiscard]] bool IsActive(float threshold = 0.01f) const noexcept {
        return (x * x + y * y) >= (threshold * threshold);
    }
};

/**
 * Estado lógico instantâneo de entrada de um jogador em um quadro (frame) de simulação.
 */
struct LogicalInputState {
    int player_index{0};
    DirectionalAxis direction{0.0f, 0.0f};
    std::uint32_t buttons_held{0u};      ///< Bitmask de LogicalAction mantidos pressionados no frame atual
    std::uint32_t buttons_pressed{0u};   ///< Bitmask de LogicalAction com borda de subida (just pressed)
    std::uint32_t buttons_released{0u};  ///< Bitmask de LogicalAction com borda de descida (just released)
    bool controller_connected{false};    ///< Indica se há um SDL_GameController ativo para este jogador

    [[nodiscard]] constexpr bool IsHeld(LogicalAction action) const noexcept {
        return (buttons_held & ToMask(action)) != 0u;
    }

    [[nodiscard]] constexpr bool IsPressed(LogicalAction action) const noexcept {
        return (buttons_pressed & ToMask(action)) != 0u;
    }

    [[nodiscard]] constexpr bool IsReleased(LogicalAction action) const noexcept {
        return (buttons_released & ToMask(action)) != 0u;
    }
};

/**
 * Contrato puro de abstração de entrada exigido por specs/INPUT.md (Seção 4).
 * Nenhuma referência a tipos da SDL2 ou registradores do Wii vaza para o domínio.
 */
class IInputSource {
public:
    virtual ~IInputSource() = default;

    /**
     * Retorna o estado lógico do jogador especificado (`0 <= player_index < kMaxLocalPlayers`).
     */
    virtual LogicalInputState Poll(int player_index) = 0;
};

} // namespace strikers::input

#endif // STRIKERS_INPUT_STATE_HPP_
