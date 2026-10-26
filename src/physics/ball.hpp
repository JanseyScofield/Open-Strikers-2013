#ifndef STRIKERS_PHYSICS_BALL_HPP_
#define STRIKERS_PHYSICS_BALL_HPP_

#include "physics/kinematic_body.hpp"

#include <cstdint>
#include <optional>

namespace strikers::physics {

/**
 * Estados da bola no motor de simulação.
 */
enum class BallState : std::uint8_t {
    Free,       ///< Livre sobre o gramado (rolando/quicando sob gravidade e atrito)
    Possessed,  ///< Conduzida junto aos pés de um jogador ativo
    InFlight    ///< Em voo balístico/rasante (trajetória de passe ou finalização)
};

/**
 * Entidade cinemática da bola de futebol.
 * Gerencia posição 3D, velocidade vetorial, atrito de solo, gravidade, quique e posse.
 */
class Ball {
public:
    static constexpr float kRadius             = 0.22f;  ///< Raio físico da bola (~22cm)
    static constexpr float kGravity            = -9.81f; ///< Aceleração da gravidade (m/s^2)
    static constexpr float kRestitution        = 0.62f;  ///< Coeficiente de restituição elástica no quique
    static constexpr float kGroundFriction     = 2.80f;  ///< Atrito de rolamento no gramado (1/s)
    static constexpr float kAirDrag            = 0.12f;  ///< Arrasto aerodinâmico em voo (1/s)
    static constexpr float kPickupRadius       = 1.45f;  ///< Raio de atração para assumir posse (metros)
    static constexpr float kPossessionOffset   = 0.85f;  ///< Distância à frente dos pés do condutor
    static constexpr float kPickupCooldownSec  = 0.35f;  ///< Cooldown após passe/chute para evitar re-captura imediata

    Ball() noexcept;
    explicit Ball(Vec3 initial_pos) noexcept;

    /**
     * Atualização temporal da bola.
     * @param dt Passo de tempo em segundos (ex.: 1/60 s)
     */
    void Update(float dt) noexcept;

    /**
     * Vincula a bola aos pés de um jogador em condução.
     */
    void AttachToPlayer(std::uint32_t player_id, const Vec3& player_pos, const Vec3& facing_dir) noexcept;

    /**
     * Desprende a bola em um passe na direção informada.
     */
    void Pass(const Vec3& direction, float speed) noexcept;

    /**
     * Dispara a bola em direção a um alvo tridimensional (meta ou ponto do campo).
     * @param target_pos Ponto de destino
     * @param speed      Velocidade escalar da bola
     * @param arc_height Elevação vertical inicial (arco balístico)
     */
    void Shoot(const Vec3& target_pos, float speed, float arc_height = 0.5f) noexcept;

    /**
     * Libera a posse da bola imediatamente com velocidade residual.
     */
    void ReleasePossession(const Vec3& initial_velocity = {0.0f, 0.0f, 0.0f}) noexcept;

    /**
     * Reposiciona a bola no centro do campo com velocidade zero.
     */
    void ResetToCenter() noexcept;

    void SetPosition(const Vec3& pos) noexcept { position_ = pos; }
    void SetVelocity(const Vec3& vel) noexcept { velocity_ = vel; }

    [[nodiscard]] const Vec3& GetPosition() const noexcept { return position_; }
    [[nodiscard]] const Vec3& GetVelocity() const noexcept { return velocity_; }
    [[nodiscard]] BallState GetState() const noexcept { return state_; }
    [[nodiscard]] bool IsFree() const noexcept { return state_ == BallState::Free; }
    [[nodiscard]] bool CanBePickedUp() const noexcept {
        return state_ == BallState::Free && pickup_cooldown_sec_ <= 0.0f;
    }
    [[nodiscard]] bool IsPossessed() const noexcept { return state_ == BallState::Possessed; }
    [[nodiscard]] bool IsInFlight() const noexcept { return state_ == BallState::InFlight; }
    [[nodiscard]] std::optional<std::uint32_t> GetPossessorId() const noexcept { return possessor_id_; }
    [[nodiscard]] float GetSpeed() const noexcept { return velocity_.Length(); }

    /**
     * Verifica se a bola cruzou completamente uma das metas (X <= -52.5m ou X >= +52.5m).
     * @return -1 se gol na meta esquerda (-X), +1 se gol na meta direita (+X), 0 se em jogo.
     */
    [[nodiscard]] int CheckGoalScored() const noexcept;

private:
    void ClampToPitchBounds() noexcept;

    Vec3 position_{0.0f, kRadius, 0.0f};
    Vec3 velocity_{0.0f, 0.0f, 0.0f};
    BallState state_{BallState::Free};
    std::optional<std::uint32_t> possessor_id_{std::nullopt};
    float pickup_cooldown_sec_{0.0f};
};

} // namespace strikers::physics

#endif // STRIKERS_PHYSICS_BALL_HPP_
