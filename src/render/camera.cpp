#include "render/camera.hpp"

#include <GL/gl.h>
#include <algorithm>
#include <cmath>

namespace strikers::render {

namespace {

constexpr float kPi = 3.14159265358979323846f;

physics::Vec3 Cross(const physics::Vec3& a, const physics::Vec3& b) noexcept {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float Dot(const physics::Vec3& a, const physics::Vec3& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

physics::Vec3 Normalize(const physics::Vec3& v) noexcept {
    const float len = v.Length();
    if (len <= 1e-6f) {
        return {0.0f, 1.0f, 0.0f};
    }
    return {v.x / len, v.y / len, v.z / len};
}

} // namespace

Mat4 Mat4::Identity() noexcept {
    return Mat4{};
}

Mat4 Mat4::Perspective(float fov_y_degrees, float aspect, float near_plane, float far_plane) noexcept {
    Mat4 out{};
    out.m.fill(0.0f);

    const float safe_aspect = (aspect > 0.01f) ? aspect : (16.0f / 9.0f);
    const float fov_rad = fov_y_degrees * (kPi / 180.0f);
    const float f = 1.0f / std::tan(fov_rad * 0.5f);
    const float nf = 1.0f / (near_plane - far_plane);

    out.m[0]  = f / safe_aspect;
    out.m[5]  = f;
    out.m[10] = (far_plane + near_plane) * nf;
    out.m[11] = -1.0f;
    out.m[14] = (2.0f * far_plane * near_plane) * nf;
    return out;
}

Mat4 Mat4::LookAt(
    const physics::Vec3& eye,
    const physics::Vec3& target,
    const physics::Vec3& up
) noexcept {
    const physics::Vec3 f = Normalize(target - eye);
    const physics::Vec3 s = Normalize(Cross(f, up));
    const physics::Vec3 u = Cross(s, f);

    Mat4 out = Identity();
    out.m[0] =  s.x;
    out.m[4] =  s.y;
    out.m[8] =  s.z;

    out.m[1] =  u.x;
    out.m[5] =  u.y;
    out.m[9] =  u.z;

    out.m[2]  = -f.x;
    out.m[6]  = -f.y;
    out.m[10] = -f.z;

    out.m[12] = -Dot(s, eye);
    out.m[13] = -Dot(u, eye);
    out.m[14] =  Dot(f, eye);
    return out;
}

Camera::Camera() noexcept {
    eye_pos_ = look_target_ + eye_offset_;
}

void Camera::UpdateFollowTarget(const physics::Vec3& target_pos, float dt) noexcept {
    // Acompanha 55% do deslocamento X e 35% do deslocamento Z para manter o campo sempre enquadrado
    const float desired_x = std::clamp(target_pos.x * 0.55f, -26.0f, 26.0f);
    const float desired_z = std::clamp(target_pos.z * 0.35f, -12.0f, 12.0f);

    const float alpha = (dt > 0.0f) ? std::clamp(follow_smoothing_ * dt, 0.0f, 1.0f) : 1.0f;
    look_target_.x += (desired_x - look_target_.x) * alpha;
    look_target_.y = 0.0f;
    look_target_.z += (desired_z - look_target_.z) * alpha;

    eye_pos_ = look_target_ + eye_offset_;
}

Mat4 Camera::GetProjectionMatrix(float aspect_ratio) const noexcept {
    return Mat4::Perspective(fov_degrees_, aspect_ratio, near_plane_, far_plane_);
}

Mat4 Camera::GetViewMatrix() const noexcept {
    return Mat4::LookAt(eye_pos_, look_target_, physics::Vec3{0.0f, 1.0f, 0.0f});
}

void Camera::ApplyToOpenGL(float aspect_ratio) const noexcept {
    const Mat4 proj = GetProjectionMatrix(aspect_ratio);
    const Mat4 view = GetViewMatrix();

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(proj.m.data());

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(view.m.data());
}

} // namespace strikers::render
