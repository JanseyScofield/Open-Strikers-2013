#ifndef STRIKERS_RENDER_DEBUG_RENDERER_HPP_
#define STRIKERS_RENDER_DEBUG_RENDERER_HPP_

#include "physics/ball.hpp"
#include "physics/kinematic_body.hpp"

namespace strikers::render {

/**
 * Cor RGBA em ponto flutuante `[0.0f, 1.0f]`.
 */
struct ColorRGBA {
    float r{1.0f};
    float g{1.0f};
    float b{1.0f};
    float a{1.0f};
};

/**
 * Renderizador de primitivas OpenGL para o gramado, marcações oficiais (105m x 68m),
 * traves 3D e marcador 3D orientado do atleta.
 */
class DebugRenderer {
public:
    DebugRenderer() noexcept = default;

    /**
     * Renderiza o gramado com faixas alternadas e todas as linhas brancas oficiais:
     * - Limites externos (105.0m x 68.0m)
     * - Linha de meio-campo e círculo central (raio 9.15m)
     * - Grandes áreas (16.5m x 40.32m), pequenas áreas (5.5m x 18.32m) e meias-luas
     */
    void RenderPitch() const noexcept;

    /**
     * Renderiza as traves 3D (7.32m largura x 2.44m altura x 2.40m profundidade)
     * em ambas as extremidades do campo (`X = -52.5m` e `X = +52.5m`).
     */
    void RenderGoals() const noexcept;

    /**
     * Renderiza o atleta como um marcador 3D orientado com anel de seleção no solo,
     * prisma de corpo e indicador frontal de direção (`facing_dir`).
     */
    void RenderAthlete(
        const physics::KinematicBody& body,
        const ColorRGBA& primary_color = {1.0f, 0.72f, 0.10f, 1.0f}
    ) const noexcept;

    /**
     * Renderiza a bola no espaço 3D com padrão visual esférico e sombra projetada no gramado.
     */
    void RenderBall(const physics::Ball& ball) const noexcept;

private:
    static void DrawGroundRectOutline(float x_min, float z_min, float x_max, float z_max, float y) noexcept;
    static void DrawGroundCircle(float cx, float cz, float radius, int segments, float y) noexcept;
    static void DrawGroundArc(
        float cx,
        float cz,
        float radius,
        float start_angle_rad,
        float end_angle_rad,
        int segments,
        float y
    ) noexcept;
    static void DrawGoalStructure(float goal_line_x, float depth_dir_sign) noexcept;
};

} // namespace strikers::render

#endif // STRIKERS_RENDER_DEBUG_RENDERER_HPP_
