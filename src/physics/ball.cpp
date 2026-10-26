#include "physics/ball.hpp"

#include <algorithm>
#include <cmath>

namespace strikers::physics {

Ball::Ball() noexcept
    : position_{0.0f, kRadius, 0.0f}, velocity_{0.0f, 0.0f, 0.0f} {}

Ball::Ball(Vec3 initial_pos) noexcept
    : position_{initial_pos.x, std::max(initial_pos.y, kRadius), initial_pos.z},
      velocity_{0.0f, 0.0f, 0.0f} {}

void Ball::Update(float dt) noexcept {
    if (dt <= 0.0f) {
        return;
    }

    if (pickup_cooldown_sec_ > 0.0f) {
        pickup_cooldown_sec_ = std::max(0.0f, pickup_cooldown_sec_ - dt);
    }

    if (state_ == BallState::Possessed) {
        // Quando em posse, a posição é governada pelo jogador condutor (via AttachToPlayer)
        velocity_ = {0.0f, 0.0f, 0.0f};
        return;
    }

    // 1. Aplica gravidade e arrasto aerodinâmico
    velocity_.y += kGravity * dt;
    velocity_.x -= velocity_.x * kAirDrag * dt;
    velocity_.z -= velocity_.z * kAirDrag * dt;

    // 2. Integração de posição
    position_.x += velocity_.x * dt;
    position_.y += velocity_.y * dt;
    position_.z += velocity_.z * dt;

    // 3. Colisão com o solo (gramado em Y = 0, centro da bola em Y = kRadius)
    if (position_.y <= kRadius) {
        position_.y = kRadius;

        // Quique vertical com restituição elástica
        if (std::fabs(velocity_.y) > 0.85f) {
            velocity_.y = -velocity_.y * kRestitution;
        } else {
            velocity_.y = 0.0f;
            // Se quase não há quique, a bola rola no gramado e transiciona para Free
            if (state_ == BallState::InFlight) {
                state_ = BallState::Free;
            }
        }

        // Atrito com o solo no plano horizontal XZ
        const float speed_xz = velocity_.LengthXZ();
        if (speed_xz > 1e-4f) {
            const float friction_loss = kGroundFriction * dt;
            const float new_speed = std::max(0.0f, speed_xz - friction_loss * speed_xz);
            const float factor = new_speed / speed_xz;
            velocity_.x *= factor;
            velocity_.z *= factor;

            if (new_speed < 0.08f && std::fabs(velocity_.y) < 0.05f) {
                velocity_.x = 0.0f;
                velocity_.z = 0.0f;
                velocity_.y = 0.0f;
                state_ = BallState::Free;
            }
        }
    }

    ClampToPitchBounds();
}

void Ball::AttachToPlayer(std::uint32_t player_id, const Vec3& player_pos, const Vec3& facing_dir) noexcept {
    state_ = BallState::Possessed;
    possessor_id_ = player_id;
    pickup_cooldown_sec_ = 0.0f;

    Vec3 norm_facing = facing_dir.NormalizedXZ();
    if (norm_facing.LengthXZ() < 0.1f) {
        norm_facing = {1.0f, 0.0f, 0.0f};
    }

    position_.x = player_pos.x + norm_facing.x * kPossessionOffset;
    position_.y = kRadius;
    position_.z = player_pos.z + norm_facing.z * kPossessionOffset;
    velocity_ = {0.0f, 0.0f, 0.0f};

    ClampToPitchBounds();
}

void Ball::Pass(const Vec3& direction, float speed) noexcept {
    state_ = BallState::Free;
    possessor_id_ = std::nullopt;
    pickup_cooldown_sec_ = kPickupCooldownSec;

    Vec3 norm_dir = direction.NormalizedXZ();
    if (norm_dir.LengthXZ() < 0.1f) {
        norm_dir = {1.0f, 0.0f, 0.0f};
    }

    velocity_.x = norm_dir.x * speed;
    velocity_.y = 0.35f; // Leve impulso para decolagem suave sobre o gramado
    velocity_.z = norm_dir.z * speed;
}

void Ball::Shoot(const Vec3& target_pos, float speed, float arc_height) noexcept {
    state_ = BallState::InFlight;
    possessor_id_ = std::nullopt;
    pickup_cooldown_sec_ = kPickupCooldownSec;

    Vec3 delta = target_pos - position_;
    const float dist_xz = delta.LengthXZ();
    if (dist_xz <= 1e-4f) {
        velocity_ = {speed, arc_height, 0.0f};
        return;
    }

    const float time_to_target = std::max(0.15f, dist_xz / speed);
    velocity_.x = (delta.x / dist_xz) * speed;
    velocity_.z = (delta.z / dist_xz) * speed;

    // Trajetória balística para atingir a altura desejada ao chegar no alvo
    // y(t) = y0 + vy * t + 0.5 * g * t^2 => vy = (y_target - y0 - 0.5 * g * t^2) / t
    const float y_target = std::max(kRadius, target_pos.y);
    velocity_.y = (y_target - position_.y - 0.5f * kGravity * time_to_target * time_to_target) / time_to_target;
    velocity_.y = std::clamp(velocity_.y, 0.0f, 18.0f) + arc_height;
}

void Ball::ReleasePossession(const Vec3& initial_velocity) noexcept {
    state_ = BallState::Free;
    possessor_id_ = std::nullopt;
    pickup_cooldown_sec_ = kPickupCooldownSec;
    velocity_ = initial_velocity;
}

void Ball::ResetToCenter() noexcept {
    state_ = BallState::Free;
    possessor_id_ = std::nullopt;
    pickup_cooldown_sec_ = 0.0f;
    position_ = {0.0f, kRadius, 0.0f};
    velocity_ = {0.0f, 0.0f, 0.0f};
}

int Ball::CheckGoalScored() const noexcept {
    const float hx = PitchDimensions::kHalfLengthX; // 52.5m
    const float half_goal_w = PitchDimensions::kGoalWidthZ * 0.5f; // 3.66m
    const float goal_h = PitchDimensions::kGoalHeightY; // 2.44m

    if (std::fabs(position_.z) <= half_goal_w && position_.y <= goal_h) {
        if (position_.x <= -hx) {
            return -1; // Gol na meta esquerda (lado -X)
        }
        if (position_.x >= hx) {
            return 1;  // Gol na meta direita (lado +X)
        }
    }
    return 0;
}

void Ball::ClampToPitchBounds() noexcept {
    const float hx = PitchDimensions::kHalfLengthX;
    const float hz = PitchDimensions::kHalfWidthZ;
    const float half_goal_w = PitchDimensions::kGoalWidthZ * 0.5f;
    const float goal_h = PitchDimensions::kGoalHeightY;
    const float goal_depth = PitchDimensions::kGoalDepthX;

    // Se a bola estiver dentro do vão do gol (em Z e Y), permite penetração na rede até goal_depth
    const bool in_goal_mouth = (std::fabs(position_.z) <= half_goal_w && position_.y <= goal_h);

    if (in_goal_mouth) {
        const float max_back_x = hx + goal_depth;
        if (position_.x > max_back_x) {
            position_.x = max_back_x;
            velocity_.x = -velocity_.x * 0.25f;
        } else if (position_.x < -max_back_x) {
            position_.x = -max_back_x;
            velocity_.x = -velocity_.x * 0.25f;
        }
    } else {
        // Quique nas linhas laterais e de fundo fora do vão da trave
        if (position_.x > hx) {
            position_.x = hx;
            velocity_.x = -velocity_.x * kRestitution;
        } else if (position_.x < -hx) {
            position_.x = -hx;
            velocity_.x = -velocity_.x * kRestitution;
        }
    }

    if (position_.z > hz) {
        position_.z = hz;
        velocity_.z = -velocity_.z * kRestitution;
    } else if (position_.z < -hz) {
        position_.z = -hz;
        velocity_.z = -velocity_.z * kRestitution;
    }
}

} // namespace strikers::physics
