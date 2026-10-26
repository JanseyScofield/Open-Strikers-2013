#include "core/character.hpp"
#include "gameplay/duel_system.hpp"
#include "input/config_loader.hpp"
#include "input/input_manager.hpp"
#include "input/input_state.hpp"
#include "loader/character_loader.hpp"
#include "physics/ball.hpp"
#include "physics/kinematic_body.hpp"
#include "render/camera.hpp"
#include "render/debug_renderer.hpp"
#include "render/mesh.hpp"
#include "render/mesh_loader.hpp"
#include "render/shader.hpp"
#include "render/window.hpp"

#include <SDL2/SDL.h>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace {

std::filesystem::path ResolveWorkspacePath(const std::filesystem::path& relative_target) {
    if (std::filesystem::exists(relative_target)) {
        return relative_target;
    }
    const std::filesystem::path p1 = std::filesystem::path("..") / relative_target;
    if (std::filesystem::exists(p1)) {
        return p1;
    }
    const std::filesystem::path p2 = std::filesystem::path("../..") / relative_target;
    if (std::filesystem::exists(p2)) {
        return p2;
    }
    return relative_target;
}

} // namespace

int main(int argc, char* argv[]) {
    int max_frames = -1; // -1 = execução interativa contínua
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--max-frames" && (i + 1) < argc) {
            max_frames = std::atoi(argv[++i]);
        } else if (arg == "--smoke-test") {
            max_frames = 15;
        }
    }

    try {
        std::cout << "======================================================================\n";
        std::cout << "  Open Strikers 2013 - Match Sandbox & Battle System (60 FPS)\n";
        std::cout << "======================================================================\n";

        // 1. Carrega o catálogo de dados decodificados
        const auto catalog_path = ResolveWorkspacePath("output/data/characters_parsed.json");
        const strikers::loader::CharacterCatalog catalog =
            strikers::loader::CharacterLoader::LoadFromFile(catalog_path);

        // Atacante controlado: Gouenji Shuuya (cid=23, Fire, Kick=30/70, Speed=30/50)
        const strikers::core::Character* striker_char = catalog.FindCharacterById(23);
        if (striker_char == nullptr) {
            striker_char = catalog.FindCharacterById(1); // Fallback para Endo se não encontrar Gouenji
        }

        // Goleiro defensor na meta: Endo Mamoru (cid=1, Mountain, Catch=30/90)
        const strikers::core::Character* gk_char = catalog.FindCharacterById(1);

        // Defensor de linha para Duelo Terrestre 1v1: Kabeyama Heigorou (cid=7, Mountain, Guard=30/70)
        const strikers::core::Character* defender_char = catalog.FindCharacterById(7);
        if (defender_char == nullptr) {
            defender_char = gk_char;
        }

        // Goleiro defensor na meta: Endo Mamoru (cid=1, Mountain, Catch=30/90)
        const strikers::core::Character* gk_char_check = gk_char;
        if (!striker_char || !gk_char_check || !defender_char) {
            std::cerr << "[ERRO] Falha ao carregar atletas do catalogo.\n";
            return EXIT_FAILURE;
        }

        std::cout << "[Dominio] Atacante Ativo : #" << striker_char->character_id << " "
                  << striker_char->names.full_name << " (" << striker_char->names.hud_name << ")"
                  << " | " << striker_char->element_name << " | Kick="
                  << static_cast<int>(striker_char->attributes.kick_base)
                  << " (Max=" << static_cast<int>(striker_char->attributes.kick_max) << ")\n";
        std::cout << "[Dominio] Defensor 1v1   : #" << defender_char->character_id << " "
                  << defender_char->names.full_name << " (" << defender_char->names.hud_name << ")"
                  << " | " << defender_char->element_name << " | Guard="
                  << static_cast<int>(defender_char->attributes.guard_base) << "\n";
        std::cout << "[Dominio] Goleiro na Meta: #" << gk_char->character_id << " "
                  << gk_char->names.full_name << " (" << gk_char->names.hud_name << ")"
                  << " | " << gk_char->element_name << " | Catch="
                  << static_cast<int>(gk_char->attributes.catch_base) << "\n\n";

        // Prepara parâmetros de combate e técnicas (Atacante com stats Max evoluídos para equilibrar contra God Hand)
        auto striker_view  = strikers::gameplay::PlayerBattleView::FromCharacter(*striker_char, true);
        striker_view.waza_evolution_tier = 3; // Fire Tornado Kai / Shin
        auto defender_view = strikers::gameplay::PlayerBattleView::FromCharacter(*defender_char, false);
        auto gk_view       = strikers::gameplay::PlayerBattleView::FromCharacter(*gk_char, false);

        // Técnicas Hissatsu: Fire Tornado (#337), God Hand (#95), Heat Tackle (#206) e The Wall (#97)
        const auto* fire_tornado_tech = catalog.FindHissatsuById(337);
        const auto* god_hand_tech     = catalog.FindHissatsuById(95);
        const auto* dribble_tech      = catalog.FindHissatsuById(206);
        const auto* block_tech        = catalog.FindHissatsuById(97);

        strikers::gameplay::WazaBattleParam shoot_waza{};
        if (fire_tornado_tech) {
            shoot_waza = strikers::gameplay::WazaBattleParam::FromHissatsu(*fire_tornado_tech);
        }

        strikers::gameplay::WazaBattleParam gk_waza{};
        if (god_hand_tech) {
            gk_waza = strikers::gameplay::WazaBattleParam::FromHissatsu(*god_hand_tech);
        }

        strikers::gameplay::WazaBattleParam dribble_waza{};
        if (dribble_tech) {
            dribble_waza = strikers::gameplay::WazaBattleParam::FromHissatsu(*dribble_tech);
        }

        strikers::gameplay::WazaBattleParam block_waza{};
        if (block_tech) {
            block_waza = strikers::gameplay::WazaBattleParam::FromHissatsu(*block_tech);
        }

        // 2. Inicializa Janela SDL2 + OpenGL (1280x720, VSync)
        strikers::render::WindowConfig win_cfg{};
        win_cfg.title = "Open Strikers 2013 - Match Sandbox & Duel Engine";
        win_cfg.width = 1280;
        win_cfg.height = 720;
        win_cfg.vsync = true;

        strikers::render::Window window(win_cfg);
        if (!window.Initialize()) {
            std::cerr << "[ERRO] Falha ao inicializar janela SDL2/OpenGL.\n";
            return EXIT_FAILURE;
        }

        // 3. Inicializa Entrada (SDL2 InputManager + config/bindings.json)
        strikers::input::InputManager input_mgr;
        if (!input_mgr.Initialize()) {
            std::cerr << "[ERRO] Falha ao inicializar InputManager: " << SDL_GetError() << "\n";
            return EXIT_FAILURE;
        }
        const auto bindings_path = ResolveWorkspacePath("config/bindings.json");
        input_mgr.LoadBindings(bindings_path);

        std::cout << "[Controles]\n"
                  << "  - Movimento : WASD / Analogico Esquerdo / D-Pad\n"
                  << "  - Dash      : L / Left Shift / RB (R1)\n"
                  << "  - Conduzir  : Aproxime-se da bola no centro do campo\n"
                  << "  - Passe     : J / Botao A (X)\n"
                  << "  - Drible    : U / Botao Y (Triangulo) ou aproximar do defensor -> Duelo Terrestre 1v1!\n"
                  << "  - Burning   : I / LB (L1) -> Ativa/Desativa Burning Phase (+30 Poder)\n"
                  << "  - Chute     : K / Botao B (O) / Botao X ([]) -> Dispara Duelo contra o Goleiro!\n"
                  << "  - Sair      : Space / Enter / Start (Pause) ou fechar a janela\n\n";

        // 4. Instancia Entidades da Simulação (Jogador, Defensor 1v1, Goleiro, Bola, Duelos)
        strikers::physics::KinematicBody player_body(
            strikers::physics::Vec3{-8.0f, 0.0f, 0.0f},
            striker_char->attributes.speed_max
        );
        player_body.SetFacingDirection(1.0f, 0.0f); // Olhar voltado para a meta direita (+X)

        strikers::physics::KinematicBody defender_body(
            strikers::physics::Vec3{20.0f, 0.0f, 0.0f},
            defender_char->attributes.speed_base
        );
        defender_body.SetFacingDirection(-1.0f, 0.0f); // Defensor olha para o atacante (-X)

        strikers::physics::KinematicBody gk_body(
            strikers::physics::Vec3{47.0f, 0.0f, 0.0f},
            gk_char->attributes.speed_base
        );
        gk_body.SetFacingDirection(-1.0f, 0.0f); // Goleiro olha para o centro (-X)

        strikers::physics::Ball ball(strikers::physics::Vec3{0.0f, strikers::physics::Ball::kRadius, 0.0f});

        strikers::gameplay::DuelSystem duel_system(20130920u);
        strikers::render::Camera camera;
        strikers::render::DebugRenderer debug_renderer;
        strikers::input::IInputSource& input_source = input_mgr;

        // 4.1. Carrega Shaders GLSL e Modelos 3D Originais (.obj + .mtl + .png)
        strikers::render::Shader model_shader;
        if (window.HasOpenGLContext()) {
            const auto vert_path = ResolveWorkspacePath("src/render/shaders/model.vert");
            const auto frag_path = ResolveWorkspacePath("src/render/shaders/model.frag");
            if (!model_shader.LoadDefaultModelShader(vert_path, frag_path)) {
                std::cerr << "[ERRO] Falha ao compilar/vincular shaders GLSL.\n";
                return EXIT_FAILURE;
            }
        }

        const auto endo_obj_path = ResolveWorkspacePath("output/models/endo_mamoru.obj");
        const auto ball_obj_path = ResolveWorkspacePath("output/models/ball.obj");

        strikers::render::Mesh endo_mesh = strikers::render::MeshLoader::LoadObj(endo_obj_path);
        strikers::render::Mesh ball_mesh = strikers::render::MeshLoader::LoadObj(ball_obj_path);

        std::cout << "[Assets 3D] Modelo Atleta (Endo Mamoru): " << endo_mesh.GetSubMeshCount()
                  << " sub-malhas, " << endo_mesh.GetTotalVertexCount() << " vertices, "
                  << endo_mesh.GetTotalTriangleCount() << " triangulos.\n";
        std::cout << "[Assets 3D] Modelo Bola (ball0000)     : " << ball_mesh.GetSubMeshCount()
                  << " sub-malha, " << ball_mesh.GetTotalVertexCount() << " vertices, "
                  << ball_mesh.GetTotalTriangleCount() << " triangulos.\n\n";

        strikers::render::Mat4 ball_rotation_mtx = strikers::render::Mat4::Identity();

        // 5. Loop Principal a 60 FPS
        constexpr float kFixedDeltaSec = 1.0f / 60.0f;
        constexpr Uint64 kTargetFrameMs = 16;

        int frame_counter = 0;
        int goal_reset_timer = 0;
        float ground_duel_cooldown_sec = 0.0f;
        bool shot_pending_gk_save = false;
        std::string last_event_msg = "Bola em jogo no circulo central. Aproxime-se para conduzir!";
        bool running = true;

        while (running) {
            const Uint64 frame_start_ms = SDL_GetTicks64();

            // a) Polling de eventos de entrada
            if (!input_mgr.PumpEvents()) {
                break;
            }

            const strikers::input::LogicalInputState p1 = input_source.Poll(0);

            // Sair pelo botão Pause (Space/Enter/Start)
            if (p1.IsPressed(strikers::input::LogicalAction::PauseMenu)) {
                std::cout << "[Loop] Comando de Pause/Saida recebido no frame " << frame_counter << ".\n";
                break;
            }

            // Alternar Burning Phase (Tecla I / TacticSwitch)
            if (p1.IsPressed(strikers::input::LogicalAction::TacticSwitch)) {
                striker_view.team_burning_active = !striker_view.team_burning_active;
                last_event_msg = striker_view.team_burning_active
                    ? "[BURNING PHASE ATIVA] +30 de poder de chute!"
                    : "[BURNING PHASE DESATIVADA]";
                std::cout << last_event_msg << "\n";
            }

            if (ground_duel_cooldown_sec > 0.0f) {
                ground_duel_cooldown_sec = std::max(0.0f, ground_duel_cooldown_sec - kFixedDeltaSec);
            }

            // b) Atualiza cinemática do atacante
            const bool is_dashing = p1.IsHeld(strikers::input::LogicalAction::DashSprint);
            player_body.Update(p1.direction.x, p1.direction.y, is_dashing, kFixedDeltaSec);

            // c) Interação com a Bola (Domínio, Passe, Duelo 1v1 e Chute com Duelo)
            const strikers::physics::Vec3 prev_ball_pos = ball.GetPosition();
            const strikers::physics::Vec3& p_pos = player_body.GetPosition();
            const strikers::physics::Vec3& b_pos = ball.GetPosition();
            const strikers::physics::Vec3 delta_to_ball = b_pos - p_pos;
            const float dist_to_ball = delta_to_ball.LengthXZ();

            if (ball.CanBePickedUp()) {
                // Domínio de bola / Retomada de posse por aproximação (respeitando cooldown pós-passe/chute)
                if (dist_to_ball <= strikers::physics::Ball::kPickupRadius) {
                    ball.AttachToPlayer(1, p_pos, player_body.GetFacingDirection());
                    shot_pending_gk_save = false;
                    last_event_msg = "[" + striker_char->names.short_name + "] dominou a bola!";
                    std::cout << "[Posse] " << last_event_msg << "\n";
                }
            } else if (ball.IsPossessed() && ball.GetPossessorId() == 1) {
                // Acompanha os pés do jogador em condução
                ball.AttachToPlayer(1, p_pos, player_body.GetFacingDirection());

                const float dist_to_defender = (defender_body.GetPosition() - p_pos).LengthXZ();

                // Duelo Terrestre 1v1 ao se aproximar do defensor ou pressionar U (DribbleSpecial)
                if (dist_to_defender <= 2.2f && ground_duel_cooldown_sec <= 0.0f) {
                    ground_duel_cooldown_sec = 1.5f;

                    if (p1.IsHeld(strikers::input::LogicalAction::DribbleSpecial) ||
                        p1.IsPressed(strikers::input::LogicalAction::DribbleSpecial)) {
                        const int cmd_res = strikers::gameplay::DuelSystem::ResolveHissatsuCommandBattle(
                            striker_view, &dribble_waza, defender_view, &block_waza
                        );
                        std::cout << "\n------------------------------------------------------\n";
                        std::cout << "  DUELO 1v1 HISSATSU COMMAND BATTLE (FUN_8009f078)\n";
                        std::cout << "------------------------------------------------------\n";
                        std::cout << "  Atacante : " << striker_char->names.full_name << " [" << dribble_waza.name << "]\n";
                        std::cout << "  Defensor : " << defender_char->names.full_name << " [" << block_waza.name << "]\n";
                        if (cmd_res >= 0) {
                            last_event_msg = "[HISSATSU DRIBLE VENCEU!] " + striker_char->names.short_name + " superou " + defender_char->names.short_name + "!";
                            std::cout << "  Resultado: " << last_event_msg << "\n";
                        } else {
                            ball.ReleasePossession({-8.0f, 2.5f, 3.0f});
                            last_event_msg = "[BLOQUEIO HISSATSU!] " + defender_char->names.short_name + " desarmou com " + block_waza.name + "!";
                            std::cout << "  Resultado: " << last_event_msg << "\n";
                        }
                        std::cout << "------------------------------------------------------\n\n";
                    } else {
                        const auto tackle_res = duel_system.ResolveNormalTackle(striker_view, defender_view);
                        last_event_msg = tackle_res.summary;
                        std::cout << "\n------------------------------------------------------\n";
                        std::cout << "  DUELO TERRESTRE 1v1 NORMAL (FUN_80075754)\n";
                        std::cout << "------------------------------------------------------\n";
                        std::cout << "  " << tackle_res.summary << "\n";
                        std::cout << "------------------------------------------------------\n\n";
                        if (!tackle_res.attacker_won) {
                            // Defensor desarmou a bola!
                            ball.ReleasePossession({-9.0f, 1.8f, 3.5f});
                        }
                    }
                }

                // Se ainda possui a bola após eventual duelo terrestre, processa Passe ou Chute
                if (ball.IsPossessed() && ball.GetPossessorId() == 1) {
                    // Ação: Passe
                    if (p1.IsPressed(strikers::input::LogicalAction::PassSwitch)) {
                        ball.Pass(player_body.GetFacingDirection(), 18.0f);
                        shot_pending_gk_save = false;
                        last_event_msg = "[PASSE] Toque de bola executado por " + striker_char->names.short_name;
                        std::cout << last_event_msg << "\n";
                    }
                    // Ação: Chute -> Dispara resolução de duelo
                    else if (p1.IsPressed(strikers::input::LogicalAction::ShootSlide)) {
                        // Alvo dentro da meta direita (X = 53.5m > linha de gol 52.5m)
                        const strikers::physics::Vec3 goal_target{53.5f, 1.15f, 0.0f};
                        const strikers::physics::Vec3 goal_line_center{52.5f, 0.0f, 0.0f};
                        const float dist_to_goal = (goal_line_center - p_pos).Length();
                        const float dist_to_gk = (gk_body.GetPosition() - p_pos).Length();

                        // Calcula poder de chute com fórmulas Level-5
                        const int shoot_power = duel_system.ComputeShootPower(
                            striker_view,
                            &shoot_waza,
                            0.9f,
                            dist_to_goal,
                            dist_to_gk
                        );

                        // Goleiro tenta defender com Hissatsu (God Hand)
                        const auto defense_res = duel_system.ResolveShotVsGoalkeeper(
                            striker_view,
                            &shoot_waza,
                            shoot_power,
                            gk_view,
                            &gk_waza,
                            0.9f,
                            (dist_to_gk <= 25.0f)
                        );

                        // Dispara a bola no motor físico em voo balístico em direção à meta
                        ball.Shoot(goal_target, 34.0f, 0.90f);
                        shot_pending_gk_save = !defense_res.is_goal;

                        last_event_msg = defense_res.summary;
                        std::cout << "\n======================================================\n";
                        std::cout << "  DUELO DE FINALIZACAO A GOL (specs/BATTLE_FORMULAS.md)\n";
                        std::cout << "======================================================\n";
                        std::cout << "  Chutador : " << striker_char->names.full_name << " (Kick="
                                  << striker_view.kick << ", Tec: "
                                  << shoot_waza.name << " [Pow="
                                  << strikers::gameplay::DuelSystem::GetEffectiveWazaPower(striker_view, &shoot_waza)
                                  << "])\n";
                        std::cout << "  Goleiro  : " << gk_char->names.full_name << " (Catch="
                                  << gk_view.catch_stat << ", Tec: "
                                  << gk_waza.name << " [Pow="
                                  << strikers::gameplay::DuelSystem::GetEffectiveWazaPower(gk_view, &gk_waza)
                                  << "])\n";
                        std::cout << "  Distancia: " << std::fixed << std::setprecision(1) << dist_to_goal << "m\n";
                        std::cout << "  P_chute  : " << defense_res.shoot_power << "\n";
                        std::cout << "  P_defesa : " << defense_res.defense_power
                                  << " (ElemWaza=" << defense_res.elem_waza_bonus
                                  << ", ElemPlayer=" << defense_res.elem_player_bonus
                                  << ", RNG=" << defense_res.rng_variance << ")\n";
                        std::cout << "  Veredito : " << (defense_res.is_goal ? "GOL! DEFESA SUPERADA!" : "DEFENDIDO PELO GOLEIRO!") << "\n";
                        std::cout << "======================================================\n\n";
                    }
                }
            }

            // Atualiza física e quiques da bola
            ball.Update(kFixedDeltaSec);

            // Calcula rotação visual da bola proporcional ao deslocamento/velocidade de rolagem no solo
            {
                const strikers::physics::Vec3 curr_ball_pos = ball.GetPosition();
                const float dx = curr_ball_pos.x - prev_ball_pos.x;
                const float dz = curr_ball_pos.z - prev_ball_pos.z;
                const float dist_xz = std::sqrt(dx * dx + dz * dz);
                if (dist_xz > 1e-5f) {
                    // Eixo de rolagem: Up (0, 1, 0) x Deslocamento (dx, 0, dz) = (dz, 0, -dx)
                    const strikers::physics::Vec3 roll_axis{dz, 0.0f, -dx};
                    const float roll_angle_rad = dist_xz / strikers::physics::Ball::kRadius;
                    ball_rotation_mtx =
                        strikers::render::Mat4::RotateAxisAngle(roll_axis, roll_angle_rad) * ball_rotation_mtx;
                }
            }

            // Se o goleiro venceu o duelo matemático, ele intercepta fisicamente a bola na linha da pequena área
            if (shot_pending_gk_save && ball.GetPosition().x >= (gk_body.GetPosition().x - 0.8f)) {
                shot_pending_gk_save = false;
                ball.SetPosition({gk_body.GetPosition().x - 1.0f, 0.9f, 0.0f});
                ball.ReleasePossession({-16.0f, 3.5f, 3.0f}); // Rebote/espalmada para fora da área
                std::cout << "[Goleiro] " << gk_char->names.short_name
                          << " interceptou fisicamente o chute e rebateu a bola!\n";
            }

            // Verificação de Gol
            const int goal_scored = ball.CheckGoalScored();
            if (goal_scored != 0) {
                if (goal_reset_timer == 0) {
                    last_event_msg = "[GOL!] A bola estufou as redes da meta!";
                    std::cout << "\n>>> " << last_event_msg << " <<<\n\n";
                    goal_reset_timer = 90; // ~1.5 segundo para comemoração e reset
                }
            }

            if (goal_reset_timer > 0) {
                --goal_reset_timer;
                if (goal_reset_timer == 0) {
                    ball.ResetToCenter();
                    player_body.SetPosition({-8.0f, 0.0f, 0.0f});
                    player_body.SetFacingDirection(1.0f, 0.0f);
                    last_event_msg = "Bola reposicionada no centro para o reinicio!";
                    std::cout << "[Match] " << last_event_msg << "\n";
                }
            }

            // d) Atualização da Câmera
            // Câmera acompanha o jogador ou a bola quando em voo
            const strikers::physics::Vec3 camera_focus =
                ball.IsInFlight() ? (ball.GetPosition() * 0.7f + player_body.GetPosition() * 0.3f)
                                  : player_body.GetPosition();
            camera.UpdateFollowTarget(camera_focus, kFixedDeltaSec);

            // e) Renderização OpenGL
            window.BeginFrame();
            if (window.HasOpenGLContext()) {
                const float aspect = window.GetAspectRatio();
                camera.ApplyToOpenGL(aspect);

                // 1. DebugRenderer ativo apenas para traçar o campo (linhas brancas) e as traves
                strikers::render::Shader::Unbind();
                strikers::render::Texture::Unbind();
                debug_renderer.RenderPitch();
                debug_renderer.RenderGoals();

                // 2. Renderização das Malhas 3D com Texturas e Iluminação Direcional Difusa via Shader
                const strikers::render::Mat4 view_proj_mtx =
                    camera.GetProjectionMatrix(aspect) * camera.GetViewMatrix();
                const strikers::physics::Vec3 light_dir{0.35f, 0.85f, 0.40f};
                constexpr float kAthleteModelScale = 1.85f;
                // Raio nativo de ball.obj é ~0.1685m; escalamos para casar com Ball::kRadius (0.32m)
                constexpr float kBallModelScale = strikers::physics::Ball::kRadius / 0.1685f;

                // Jogador Controlado (Modelo 3D Endo Mamoru orientado na direção do movimento)
                const strikers::render::Mat4 player_model_mtx =
                    strikers::render::Mat4::FromPositionDirectionScale(
                        player_body.GetPosition(),
                        player_body.GetFacingDirection(),
                        kAthleteModelScale
                    );
                endo_mesh.Draw(model_shader, player_model_mtx, view_proj_mtx, light_dir, 1.0f, 1.0f, 1.0f, 1.0f);

                // Defensor 1v1 (orientado em direção ao centro)
                const strikers::render::Mat4 defender_model_mtx =
                    strikers::render::Mat4::FromPositionDirectionScale(
                        defender_body.GetPosition(),
                        defender_body.GetFacingDirection(),
                        kAthleteModelScale
                    );
                endo_mesh.Draw(model_shader, defender_model_mtx, view_proj_mtx, light_dir, 0.82f, 0.90f, 1.0f, 1.0f);

                // Goleiro na Meta Direita
                const strikers::render::Mat4 gk_model_mtx =
                    strikers::render::Mat4::FromPositionDirectionScale(
                        gk_body.GetPosition(),
                        gk_body.GetFacingDirection(),
                        kAthleteModelScale
                    );
                endo_mesh.Draw(model_shader, gk_model_mtx, view_proj_mtx, light_dir, 0.92f, 1.0f, 0.92f, 1.0f);

                // Bola de Futebol 3D com Textura e Rotação de Rolagem
                const strikers::render::Mat4 ball_model_mtx =
                    strikers::render::Mat4::Translate(ball.GetPosition()) *
                    ball_rotation_mtx *
                    strikers::render::Mat4::Scale({kBallModelScale, kBallModelScale, kBallModelScale});
                ball_mesh.Draw(model_shader, ball_model_mtx, view_proj_mtx, light_dir, 1.0f, 1.0f, 1.0f, 1.0f);
            }
            window.EndFrame();

            // Atualiza telemetria no título da janela
            if (frame_counter % 12 == 0) {
                std::ostringstream title_ss;
                title_ss << "Open Strikers 2013 | Atacante: " << striker_char->names.short_name
                         << " (" << (ball.IsPossessed() ? "COM POSSE" : "SEM POSSE") << ")"
                         << " | " << last_event_msg;
                window.SetTitle(title_ss.str());
            }

            ++frame_counter;
            if (max_frames > 0 && frame_counter >= max_frames) {
                running = false;
            }

            // Frame pacing para 60 FPS estáveis
            const Uint64 elapsed_ms = SDL_GetTicks64() - frame_start_ms;
            if (elapsed_ms < kTargetFrameMs) {
                SDL_Delay(static_cast<Uint32>(kTargetFrameMs - elapsed_ms));
            }
        }

        input_mgr.Shutdown();
        window.Shutdown();
        SDL_Quit();

        std::cout << "[OK] Sessao finalizada limpa apos " << frame_counter << " quadros.\n";
        return EXIT_SUCCESS;

    } catch (const std::exception& ex) {
        std::cerr << "[FALHA CRITICA] " << ex.what() << "\n";
        SDL_Quit();
        return EXIT_FAILURE;
    }
}
