#ifndef STRIKERS_RENDER_CAMERA_HPP_
#define STRIKERS_RENDER_CAMERA_HPP_

#include "physics/kinematic_body.hpp"

#include <array>

namespace strikers::render {

/**
 * Matriz 4x4 Column-Major compatível diretamente com `glLoadMatrixf`.
 */
struct Mat4 {
    std::array<float, 16> m{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    [[nodiscard]] static Mat4 Identity() noexcept;
    [[nodiscard]] static Mat4 Perspective(float fov_y_degrees, float aspect, float near_plane, float far_plane) noexcept;
    [[nodiscard]] static Mat4 LookAt(
        const physics::Vec3& eye,
        const physics::Vec3& target,
        const physics::Vec3& up
    ) noexcept;
};

/**
 * Câmera de transmissão lateral/isométrica de futebol (estilo Strikers 2013).
 * Posicionada em altura e ângulo elevados na lateral sul (+Z) olhando para o campo (Y=0),
 * acompanhando suavemente o atleta ativo sem perder o enquadramento tático do gramado.
 */
class Camera {
public:
    Camera() noexcept;

    /**
     * Atualiza o alvo da câmera acompanhando suavemente a posição do jogador no gramado.
     */
    void UpdateFollowTarget(const physics::Vec3& target_pos, float dt) noexcept;

    /**
     * Carrega as matrizes de projeção (`GL_PROJECTION`) e visualização (`GL_MODELVIEW`)
     * no pipeline OpenGL para o aspect ratio informado.
     */
    void ApplyToOpenGL(float aspect_ratio) const noexcept;

    void SetEyeOffset(const physics::Vec3& offset) noexcept { eye_offset_ = offset; }
    void SetFieldOfView(float fov_degrees) noexcept { fov_degrees_ = fov_degrees; }

    [[nodiscard]] const physics::Vec3& GetEyePosition() const noexcept { return eye_pos_; }
    [[nodiscard]] const physics::Vec3& GetLookAtTarget() const noexcept { return look_target_; }
    [[nodiscard]] Mat4 GetProjectionMatrix(float aspect_ratio) const noexcept;
    [[nodiscard]] Mat4 GetViewMatrix() const noexcept;

private:
    physics::Vec3 look_target_{0.0f, 0.0f, 0.0f};
    physics::Vec3 eye_offset_{0.0f, 52.0f, 64.0f}; ///< Altura Y=52m e recuo lateral Z=+64m
    physics::Vec3 eye_pos_{0.0f, 52.0f, 64.0f};
    float fov_degrees_{42.0f};
    float near_plane_{1.0f};
    float far_plane_{300.0f};
    float follow_smoothing_{6.0f};
};

} // namespace strikers::render

#endif // STRIKERS_RENDER_CAMERA_HPP_
