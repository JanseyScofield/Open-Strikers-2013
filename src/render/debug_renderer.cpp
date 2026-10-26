#include "render/debug_renderer.hpp"

#include <GL/gl.h>
#include <cmath>

namespace strikers::render {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kLineY = 0.02f; ///< Elevação sutil sobre Y=0 para evitar z-fighting com o gramado

} // namespace

void DebugRenderer::DrawGroundRectOutline(float x_min, float z_min, float x_max, float z_max, float y) noexcept {
    glBegin(GL_LINE_LOOP);
    glVertex3f(x_min, y, z_min);
    glVertex3f(x_max, y, z_min);
    glVertex3f(x_max, y, z_max);
    glVertex3f(x_min, y, z_max);
    glEnd();
}

void DebugRenderer::DrawGroundCircle(float cx, float cz, float radius, int segments, float y) noexcept {
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        const float angle = (2.0f * kPi * static_cast<float>(i)) / static_cast<float>(segments);
        glVertex3f(cx + std::cos(angle) * radius, y, cz + std::sin(angle) * radius);
    }
    glEnd();
}

void DebugRenderer::DrawGroundArc(
    float cx,
    float cz,
    float radius,
    float start_angle_rad,
    float end_angle_rad,
    int segments,
    float y
) noexcept {
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const float angle = start_angle_rad + (end_angle_rad - start_angle_rad) * t;
        glVertex3f(cx + std::cos(angle) * radius, y, cz + std::sin(angle) * radius);
    }
    glEnd();
}

void DebugRenderer::RenderPitch() const noexcept {
    using physics::PitchDimensions;
    const float hx = PitchDimensions::kHalfLengthX; // 52.5m
    const float hz = PitchDimensions::kHalfWidthZ;  // 34.0m

    // 1. Área externa ao redor das quatro linhas (Tapete do estádio)
    glColor4f(0.11f, 0.34f, 0.16f, 1.0f);
    glBegin(GL_QUADS);
    glVertex3f(-hx - 8.0f, -0.02f, -hz - 6.0f);
    glVertex3f( hx + 8.0f, -0.02f, -hz - 6.0f);
    glVertex3f( hx + 8.0f, -0.02f,  hz + 6.0f);
    glVertex3f(-hx - 8.0f, -0.02f,  hz + 6.0f);
    glEnd();

    // 2. Gramado principal (105m x 68m) com 14 faixas verticais alternadas
    constexpr int kStripeCount = 14;
    const float stripe_width = PitchDimensions::kLengthX / static_cast<float>(kStripeCount);
    glBegin(GL_QUADS);
    for (int i = 0; i < kStripeCount; ++i) {
        if (i % 2 == 0) {
            glColor4f(0.16f, 0.54f, 0.22f, 1.0f);
        } else {
            glColor4f(0.19f, 0.60f, 0.25f, 1.0f);
        }
        const float x0 = -hx + static_cast<float>(i) * stripe_width;
        const float x1 = x0 + stripe_width;
        glVertex3f(x0, 0.0f, -hz);
        glVertex3f(x1, 0.0f, -hz);
        glVertex3f(x1, 0.0f,  hz);
        glVertex3f(x0, 0.0f,  hz);
    }
    glEnd();

    // 3. Linhas brancas oficiais do campo
    glLineWidth(2.2f);
    glColor4f(0.96f, 0.98f, 0.96f, 0.95f);

    // Retângulo exterior (105m x 68m)
    DrawGroundRectOutline(-hx, -hz, hx, hz, kLineY);

    // Linha de meio-campo (X = 0)
    glBegin(GL_LINES);
    glVertex3f(0.0f, kLineY, -hz);
    glVertex3f(0.0f, kLineY,  hz);
    glEnd();

    // Círculo central (raio 9.15m) e marca central
    DrawGroundCircle(0.0f, 0.0f, PitchDimensions::kCenterCircleRadius, 64, kLineY);
    DrawGroundCircle(0.0f, 0.0f, 0.35f, 16, kLineY);

    // Grandes áreas (16.5m x 40.32m) e Pequenas áreas (5.5m x 18.32m)
    const float pen_half_z = PitchDimensions::kPenaltyBoxWidthZ * 0.5f;
    const float goal_box_half_z = PitchDimensions::kGoalBoxWidthZ * 0.5f;

    // Lado Esquerdo (-X)
    DrawGroundRectOutline(-hx, -pen_half_z, -hx + PitchDimensions::kPenaltyBoxDepthX, pen_half_z, kLineY);
    DrawGroundRectOutline(-hx, -goal_box_half_z, -hx + PitchDimensions::kGoalBoxDepthX, goal_box_half_z, kLineY);
    DrawGroundCircle(-hx + 11.0f, 0.0f, 0.30f, 16, kLineY);
    DrawGroundArc(-hx + 11.0f, 0.0f, PitchDimensions::kCenterCircleRadius, -0.93f, 0.93f, 32, kLineY);

    // Lado Direito (+X)
    DrawGroundRectOutline(hx - PitchDimensions::kPenaltyBoxDepthX, -pen_half_z, hx, pen_half_z, kLineY);
    DrawGroundRectOutline(hx - PitchDimensions::kGoalBoxDepthX, -goal_box_half_z, hx, goal_box_half_z, kLineY);
    DrawGroundCircle(hx - 11.0f, 0.0f, 0.30f, 16, kLineY);
    DrawGroundArc(hx - 11.0f, 0.0f, PitchDimensions::kCenterCircleRadius, kPi - 0.93f, kPi + 0.93f, 32, kLineY);
}

void DebugRenderer::DrawGoalStructure(float goal_line_x, float depth_dir_sign) noexcept {
    using physics::PitchDimensions;
    const float half_w = PitchDimensions::kGoalWidthZ * 0.5f; // 3.66m
    const float h = PitchDimensions::kGoalHeightY;            // 2.44m
    const float back_x = goal_line_x + depth_dir_sign * PitchDimensions::kGoalDepthX;

    // Postes principais e travessão frontal
    glLineWidth(3.5f);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_LINE_STRIP);
    glVertex3f(goal_line_x, 0.0f, -half_w);
    glVertex3f(goal_line_x, h,    -half_w);
    glVertex3f(goal_line_x, h,     half_w);
    glVertex3f(goal_line_x, 0.0f,  half_w);
    glEnd();

    // Armação traseira e rede de suporte
    glLineWidth(1.5f);
    glColor4f(0.82f, 0.86f, 0.92f, 0.75f);
    glBegin(GL_LINES);
    // hastes superiores traseiras
    glVertex3f(goal_line_x, h, -half_w);
    glVertex3f(back_x,      h, -half_w);
    glVertex3f(goal_line_x, h,  half_w);
    glVertex3f(back_x,      h,  half_w);
    // hastes inferiores traseiras
    glVertex3f(goal_line_x, 0.02f, -half_w);
    glVertex3f(back_x,      0.02f, -half_w);
    glVertex3f(goal_line_x, 0.02f,  half_w);
    glVertex3f(back_x,      0.02f,  half_w);
    // quadro de fundo
    glVertex3f(back_x, 0.02f, -half_w);
    glVertex3f(back_x, h,     -half_w);
    glVertex3f(back_x, h,     -half_w);
    glVertex3f(back_x, h,      half_w);
    glVertex3f(back_x, h,      half_w);
    glVertex3f(back_x, 0.02f,  half_w);
    glVertex3f(back_x, 0.02f, -half_w);
    glVertex3f(back_x, 0.02f,  half_w);
    glEnd();
}

void DebugRenderer::RenderGoals() const noexcept {
    using physics::PitchDimensions;
    DrawGoalStructure(-PitchDimensions::kHalfLengthX, -1.0f);
    DrawGoalStructure( PitchDimensions::kHalfLengthX,  1.0f);
}

void DebugRenderer::RenderAthlete(
    const physics::KinematicBody& body,
    const ColorRGBA& primary_color
) const noexcept {
    const physics::Vec3& pos = body.GetPosition();
    const float heading_deg = body.GetHeadingRadians() * (180.0f / kPi);
    const bool dashing = body.IsDashing();

    glPushMatrix();
    glTranslatef(pos.x, pos.y, pos.z);
    // Rotaciona no eixo Y (-heading_deg pois em coordenadas mão-direita +Z aponta para o sul)
    glRotatef(-heading_deg, 0.0f, 1.0f, 0.0f);

    // 1. Anel de seleção sob os pés do atleta (muda para ciano elétrico durante o Dash/Sprint)
    glLineWidth(2.6f);
    if (dashing) {
        glColor4f(0.15f, 0.95f, 1.0f, 0.95f);
    } else {
        glColor4f(1.0f, 0.92f, 0.20f, 0.90f);
    }
    DrawGroundCircle(0.0f, 0.0f, 1.15f, 32, 0.04f);

    // 2. Seta direcional no gramado apontando para a frente (+X local = direção do olhar)
    glBegin(GL_TRIANGLES);
    if (dashing) {
        glColor4f(0.20f, 0.95f, 1.0f, 0.90f);
    } else {
        glColor4f(1.0f, 0.35f, 0.10f, 0.90f);
    }
    glVertex3f(1.95f, 0.05f,  0.0f);
    glVertex3f(1.10f, 0.05f, -0.55f);
    glVertex3f(1.10f, 0.05f,  0.55f);
    glEnd();

    // 3. Corpo 3D do Atleta (Prisma/Pirâmide truncada com altura 1.80m)
    const float r = 0.48f;
    const float h_waist = 0.75f;
    const float h_shoulder = 1.45f;
    const float h_head = 1.85f;

    // Calção / Pernas (Azul Raimon / Verde escuro GK)
    glColor4f(0.12f, 0.25f, 0.58f, 1.0f);
    glBegin(GL_QUADS);
    // Frente
    glVertex3f( r, 0.0f,    -r);
    glVertex3f( r, 0.0f,     r);
    glVertex3f( r, h_waist,  r);
    glVertex3f( r, h_waist, -r);
    // Costas
    glVertex3f(-r, 0.0f,     r);
    glVertex3f(-r, 0.0f,    -r);
    glVertex3f(-r, h_waist, -r);
    glVertex3f(-r, h_waist,  r);
    // Lateral Esquerda
    glVertex3f(-r, 0.0f,    -r);
    glVertex3f( r, 0.0f,    -r);
    glVertex3f( r, h_waist, -r);
    glVertex3f(-r, h_waist, -r);
    // Lateral Direita
    glVertex3f( r, 0.0f,     r);
    glVertex3f(-r, 0.0f,     r);
    glVertex3f(-r, h_waist,  r);
    glVertex3f( r, h_waist,  r);
    glEnd();

    // Tronco / Camisa (Cor primária do uniforme)
    glColor4f(primary_color.r, primary_color.g, primary_color.b, primary_color.a);
    glBegin(GL_QUADS);
    // Frente (Peito)
    glVertex3f( r, h_waist,    -r);
    glVertex3f( r, h_waist,     r);
    glVertex3f( r, h_shoulder,  r);
    glVertex3f( r, h_shoulder, -r);
    // Costas
    glVertex3f(-r, h_waist,     r);
    glVertex3f(-r, h_waist,    -r);
    glVertex3f(-r, h_shoulder, -r);
    glVertex3f(-r, h_shoulder,  r);
    // Lateral Esquerda
    glVertex3f(-r, h_waist,    -r);
    glVertex3f( r, h_waist,    -r);
    glVertex3f( r, h_shoulder, -r);
    glVertex3f(-r, h_shoulder, -r);
    // Lateral Direita
    glVertex3f( r, h_waist,     r);
    glVertex3f(-r, h_waist,     r);
    glVertex3f(-r, h_shoulder,  r);
    glVertex3f( r, h_shoulder,  r);
    // Topo dos ombros
    glVertex3f(-r, h_shoulder, -r);
    glVertex3f( r, h_shoulder, -r);
    glVertex3f( r, h_shoulder,  r);
    glVertex3f(-r, h_shoulder,  r);
    glEnd();

    // 4. Cabeça + Bandana Laranja característica do Endo Mamoru + Visor de Olhar (+X)
    const float hr = 0.30f;
    glColor4f(0.92f, 0.74f, 0.56f, 1.0f);
    glBegin(GL_QUADS);
    glVertex3f( hr, h_shoulder, -hr);
    glVertex3f( hr, h_shoulder,  hr);
    glVertex3f( hr, h_head,      hr);
    glVertex3f( hr, h_head,     -hr);

    glVertex3f(-hr, h_shoulder,  hr);
    glVertex3f(-hr, h_shoulder, -hr);
    glVertex3f(-hr, h_head,     -hr);
    glVertex3f(-hr, h_head,      hr);

    glVertex3f(-hr, h_shoulder, -hr);
    glVertex3f( hr, h_shoulder, -hr);
    glVertex3f( hr, h_head,     -hr);
    glVertex3f(-hr, h_head,     -hr);

    glVertex3f( hr, h_shoulder,  hr);
    glVertex3f(-hr, h_shoulder,  hr);
    glVertex3f(-hr, h_head,      hr);
    glVertex3f( hr, h_head,      hr);

    glVertex3f(-hr, h_head, -hr);
    glVertex3f( hr, h_head, -hr);
    glVertex3f( hr, h_head,  hr);
    glVertex3f(-hr, h_head,  hr);
    glEnd();

    // Indicador frontal 3D (cone apontando para onde o atleta está olhando)
    glColor4f(1.0f, 1.0f, 1.0f, 0.95f);
    glBegin(GL_LINES);
    glVertex3f(0.0f, 1.15f, 0.0f);
    glVertex3f(1.65f, 1.15f, 0.0f);
    glEnd();

    glPopMatrix();
}

} // namespace strikers::render
