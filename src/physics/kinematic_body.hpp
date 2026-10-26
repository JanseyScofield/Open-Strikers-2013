#ifndef STRIKERS_PHYSICS_KINEMATIC_BODY_HPP_
#define STRIKERS_PHYSICS_KINEMATIC_BODY_HPP_

#include <cmath>
#include <cstdint>

namespace strikers::physics {

/**
 * Vetor 3D de ponto flutuante (Sistema de Coordenadas: X = Comprimento, Y = Altura, Z = Largura).
 */
struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    [[nodiscard]] constexpr Vec3 operator+(const Vec3& rhs) const noexcept {
        return {x + rhs.x, y + rhs.y, z + rhs.z};
    }

    [[nodiscard]] constexpr Vec3 operator-(const Vec3& rhs) const noexcept {
        return {x - rhs.x, y - rhs.y, z - rhs.z};
    }

    [[nodiscard]] constexpr Vec3 operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar, z * scalar};
    }

    constexpr Vec3& operator+=(const Vec3& rhs) noexcept {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    [[nodiscard]] float LengthXZ() const noexcept {
        return std::sqrt(x * x + z * z);
    }

    [[nodiscard]] float Length() const noexcept {
        return std::sqrt(x * x + y * y + z * z);
    }

    [[nodiscard]] Vec3 NormalizedXZ() const noexcept {
        const float len = LengthXZ();
        if (len <= 1e-5f) {
            return {0.0f, 0.0f, 0.0f};
        }
        return {x / len, 0.0f, z / len};
    }
};

/**
 * Constantes geométricas oficiais do campo conforme `specs/RENDER_AND_PHYSICS.md`.
 * Centro do campo na origem (0.0, 0.0, 0.0).
 */
struct PitchDimensions {
    static constexpr float kLengthX            = 105.0f; ///< Comprimento oficial (Eixo X: -52.5m .. +52.5m)
    static constexpr float kWidthZ             = 68.0f;  ///< Largura oficial (Eixo Z: -34.0m .. +34.0m)
    static constexpr float kHalfLengthX        = kLengthX * 0.5f; ///< 52.5m
    static constexpr float kHalfWidthZ         = kWidthZ * 0.5f;  ///< 34.0m
    static constexpr float kCenterCircleRadius = 9.15f;  ///< Raio do círculo central (9.15m)
    static constexpr float kPenaltyBoxDepthX   = 16.5f;  ///< Profundidade da grande área
    static constexpr float kPenaltyBoxWidthZ   = 40.32f; ///< Largura da grande área
    static constexpr float kGoalBoxDepthX      = 5.5f;   ///< Profundidade da pequena área
    static constexpr float kGoalBoxWidthZ      = 18.32f; ///< Largura da pequena área
    static constexpr float kGoalWidthZ         = 7.32f;  ///< Largura da meta (trave)
    static constexpr float kGoalHeightY        = 2.44f;  ///< Altura do travessão
    static constexpr float kGoalDepthX         = 2.40f;  ///< Profundidade da rede
};

/**
 * Corpo cinemático de um atleta sobre o gramado (plano XZ, altura Y).
 * Desacoplado de E/S e renderização conforme `specs/AGENTS.md`.
 */
class KinematicBody {
public:
    KinematicBody() noexcept = default;
    explicit KinematicBody(Vec3 initial_pos, std::uint8_t speed_attribute = 20) noexcept;

    /**
     * Configura a velocidade máxima base a partir do atributo `Speed` do personagem
     * extraído de `output/data/characters_parsed.json`.
     */
    void ConfigureFromSpeedAttribute(std::uint8_t speed_stat) noexcept;

    /**
     * Atualiza aceleração, atrito no gramado, velocidade, orientação e posição com clamping
     * estrito dentro das quatro linhas do campo (`[-52.5, +52.5]` x `[-34.0, +34.0]`).
     *
     * @param move_dir_x Componente X do direcional normalizado `[-1.0, +1.0]`
     * @param move_dir_z Componente Z do direcional normalizado `[-1.0, +1.0]`
     * @param is_dashing Flag de acionamento de corrida/dash
     * @param dt         Passo de integração em segundos (ex.: `1.0f / 60.0f`)
     */
    void Update(float move_dir_x, float move_dir_z, bool is_dashing, float dt) noexcept;

    void SetPosition(Vec3 pos) noexcept;
    void SetVelocity(Vec3 vel) noexcept;
    void SetFacingDirection(float dir_x, float dir_z) noexcept;

    [[nodiscard]] const Vec3& GetPosition() const noexcept { return position_; }
    [[nodiscard]] const Vec3& GetVelocity() const noexcept { return velocity_; }
    [[nodiscard]] const Vec3& GetAcceleration() const noexcept { return acceleration_; }
    [[nodiscard]] const Vec3& GetFacingDirection() const noexcept { return facing_dir_; }
    [[nodiscard]] float GetHeadingRadians() const noexcept { return heading_radians_; }
    [[nodiscard]] float GetBaseMaxSpeed() const noexcept { return base_max_speed_; }
    [[nodiscard]] float GetCurrentSpeed() const noexcept { return velocity_.LengthXZ(); }
    [[nodiscard]] float GetCollisionRadius() const noexcept { return collision_radius_; }
    [[nodiscard]] bool IsDashing() const noexcept { return is_dashing_; }

    /**
     * Converte o atributo `Speed` da Level-5 (`1..100`) em velocidade escalar no gramado (m/s).
     */
    [[nodiscard]] static constexpr float ComputeMaxSpeedFromStat(std::uint8_t speed_stat) noexcept {
        // Escala base: 9.0 m/s (Speed=0) a 15.0 m/s (Speed=100); Endo (Speed=20) = 10.2 m/s base
        return 9.0f + (static_cast<float>(speed_stat) * 0.06f);
    }

private:
    void ClampToPitchBounds() noexcept;

    Vec3 position_{0.0f, 0.0f, 0.0f};
    Vec3 velocity_{0.0f, 0.0f, 0.0f};
    Vec3 acceleration_{0.0f, 0.0f, 0.0f};
    Vec3 facing_dir_{1.0f, 0.0f, 0.0f}; ///< Olhar inicial voltado para o gol adversário (+X)
    float heading_radians_{0.0f};

    float base_max_speed_{10.2f};       ///< Velocidade máxima normal (m/s)
    float dash_speed_multiplier_{1.45f}; ///< Multiplicador de velocidade em Dash/Sprint
    float acceleration_rate_{48.0f};    ///< Taxa de aceleração ativa (m/s^2)
    float ground_friction_{10.5f};      ///< Coeficiente de amortecimento/atrito no gramado (1/s)
    float collision_radius_{0.55f};     ///< Raio físico do jogador para manter o corpo dentro da linha
    bool is_dashing_{false};
};

} // namespace strikers::physics

#endif // STRIKERS_PHYSICS_KINEMATIC_BODY_HPP_
