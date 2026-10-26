#include "physics/kinematic_body.hpp"

#include <algorithm>
#include <cmath>

namespace strikers::physics {

KinematicBody::KinematicBody(Vec3 initial_pos, std::uint8_t speed_attribute) noexcept
    : position_(initial_pos) {
    ConfigureFromSpeedAttribute(speed_attribute);
    ClampToPitchBounds();
}

void KinematicBody::ConfigureFromSpeedAttribute(std::uint8_t speed_stat) noexcept {
    base_max_speed_ = ComputeMaxSpeedFromStat(speed_stat);
}

void KinematicBody::SetPosition(Vec3 pos) noexcept {
    position_ = pos;
    ClampToPitchBounds();
}

void KinematicBody::SetVelocity(Vec3 vel) noexcept {
    velocity_ = vel;
}

void KinematicBody::SetFacingDirection(float dir_x, float dir_z) noexcept {
    const float len = std::sqrt(dir_x * dir_x + dir_z * dir_z);
    if (len > 1e-5f) {
        facing_dir_ = {dir_x / len, 0.0f, dir_z / len};
        heading_radians_ = std::atan2(facing_dir_.z, facing_dir_.x);
    }
}

void KinematicBody::Update(float move_dir_x, float move_dir_z, bool is_dashing, float dt) noexcept {
    if (dt <= 0.0f) {
        return;
    }

    is_dashing_ = is_dashing;

    // Normaliza o vetor de entrada caso exceda magnitude unitária
    float in_x = move_dir_x;
    float in_z = move_dir_z;
    const float in_mag = std::sqrt(in_x * in_x + in_z * in_z);
    if (in_mag > 1.0f) {
        in_x /= in_mag;
        in_z /= in_mag;
    }

    const float effective_max_speed =
        base_max_speed_ * (is_dashing_ ? dash_speed_multiplier_ : 1.0f);
    const float effective_accel =
        acceleration_rate_ * (is_dashing_ ? 1.30f : 1.0f);

    if (in_mag > 0.01f) {
        // Força motriz do jogador + resistência linear do gramado
        acceleration_.x = in_x * effective_accel - velocity_.x * ground_friction_ * 0.35f;
        acceleration_.y = 0.0f;
        acceleration_.z = in_z * effective_accel - velocity_.z * ground_friction_ * 0.35f;

        // Atualiza a orientação visual (frente/olhar) suavemente na direção do movimento
        SetFacingDirection(in_x, in_z);
    } else {
        // Desaceleração pura por atrito de solo quando não há input direcional
        acceleration_.x = -velocity_.x * ground_friction_;
        acceleration_.y = 0.0f;
        acceleration_.z = -velocity_.z * ground_friction_;
    }

    // Integração semi-implícita de Euler
    velocity_.x += acceleration_.x * dt;
    velocity_.y = 0.0f;
    velocity_.z += acceleration_.z * dt;

    // Clamping de velocidade máxima escalar e parada completa em velocidades residuais
    const float speed_xz = velocity_.LengthXZ();
    if (speed_xz > effective_max_speed) {
        const float scale = effective_max_speed / speed_xz;
        velocity_.x *= scale;
        velocity_.z *= scale;
    } else if (in_mag <= 0.01f && speed_xz < 0.05f) {
        velocity_.x = 0.0f;
        velocity_.z = 0.0f;
    }

    position_.x += velocity_.x * dt;
    position_.y = 0.0f;
    position_.z += velocity_.z * dt;

    ClampToPitchBounds();
}

void KinematicBody::ClampToPitchBounds() noexcept {
    const float max_x = PitchDimensions::kHalfLengthX;
    const float max_z = PitchDimensions::kHalfWidthZ;

    if (position_.x < -max_x) {
        position_.x = -max_x;
        if (velocity_.x < 0.0f) velocity_.x = 0.0f;
    } else if (position_.x > max_x) {
        position_.x = max_x;
        if (velocity_.x > 0.0f) velocity_.x = 0.0f;
    }

    if (position_.z < -max_z) {
        position_.z = -max_z;
        if (velocity_.z < 0.0f) velocity_.z = 0.0f;
    } else if (position_.z > max_z) {
        position_.z = max_z;
        if (velocity_.z > 0.0f) velocity_.z = 0.0f;
    }
}

} // namespace strikers::physics
