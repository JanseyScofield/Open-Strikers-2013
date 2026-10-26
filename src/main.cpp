#include "core/character.hpp"
#include "input/config_loader.hpp"
#include "input/input_manager.hpp"
#include "input/input_state.hpp"
#include "loader/character_loader.hpp"
#include "physics/kinematic_body.hpp"
#include "render/camera.hpp"
#include "render/debug_renderer.hpp"
#include "render/window.hpp"

#include <SDL2/SDL.h>
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
    int max_frames = -1; // -1 = execução contínua interativa até Pause ou fechar a janela
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--max-frames" && (i + 1) < argc) {
            max_frames = std::atoi(argv[++i]);
        } else if (arg == "--smoke-test") {
            max_frames = 12;
        }
    }

    try {
        std::cout << "======================================================================\n";
        std::cout << "  Open Strikers 2013 - Sandbox Grafica & Cinematica (60 FPS)\n";
        std::cout << "======================================================================\n";

        // 1. Carrega o catálogo de domínio e busca Endo Mamoru (cid = 1)
        const auto catalog_path = ResolveWorkspacePath("output/data/characters_parsed.json");
        const strikers::loader::CharacterCatalog catalog =
            strikers::loader::CharacterLoader::LoadFromFile(catalog_path);

        const strikers::core::Character* endo = catalog.FindCharacterById(1);
        if (endo == nullptr) {
            std::cerr << "[ERRO] Personagem Endo Mamoru (cid=1) nao encontrado em "
                      << catalog_path.string() << "\n";
            return EXIT_FAILURE;
        }

        std::cout << "[Dominio] Atleta carregado: #" << endo->character_id << " "
                  << endo->names.full_name << " (" << endo->names.hud_name << ")\n"
                  << "  - Model Code : " << endo->model_code
                  << " | Posicao: " << endo->position_name
                  << " | Elemento: " << endo->element_name << "\n"
                  << "  - Speed Base : " << static_cast<int>(endo->attributes.speed_base)
                  << " (Max: " << static_cast<int>(endo->attributes.speed_max) << ")"
                  << " -> Velocidade Base no Gramado: "
                  << std::fixed << std::setprecision(2)
                  << strikers::physics::KinematicBody::ComputeMaxSpeedFromStat(endo->attributes.speed_base)
                  << " m/s\n";

        // 2. Inicializa a Janela SDL2 + Contexto OpenGL (1280x720, VSync)
        strikers::render::WindowConfig win_cfg{};
        win_cfg.title = "Open Strikers 2013 - [" + endo->names.full_name + " / " + endo->model_code + "]";
        win_cfg.width = 1280;
        win_cfg.height = 720;
        win_cfg.vsync = true;

        strikers::render::Window window(win_cfg);
        if (!window.Initialize()) {
            std::cerr << "[ERRO] Falha ao inicializar janela SDL2/OpenGL.\n";
            return EXIT_FAILURE;
        }

        // 3. Inicializa o Subsistema de Entrada (SDL2 InputManager + config/bindings.json)
        strikers::input::InputManager input_mgr;
        if (!input_mgr.Initialize()) {
            std::cerr << "[ERRO] Falha ao inicializar InputManager: " << SDL_GetError() << "\n";
            return EXIT_FAILURE;
        }
        const auto bindings_path = ResolveWorkspacePath("config/bindings.json");
        input_mgr.LoadBindings(bindings_path);

        std::cout << "[Input] Controles carregados de " << bindings_path.string() << "\n"
                  << "  - Movimento : WASD / Stick Esquerdo / D-Pad\n"
                  << "  - Dash      : L / Left Shift / RB (R1)\n"
                  << "  - Sair      : Space / Enter / Start (Pause) ou fechar a janela\n\n";

        // 4. Instancia a Cinemática do Atleta, Câmera e DebugRenderer
        strikers::physics::KinematicBody player_body(
            strikers::physics::Vec3{0.0f, 0.0f, 0.0f},
            endo->attributes.speed_base
        );
        strikers::render::Camera camera;
        strikers::render::DebugRenderer debug_renderer;
        strikers::input::IInputSource& input_source = input_mgr;

        // 5. Loop Principal Cravado a 60 FPS
        constexpr float kFixedDeltaSec = 1.0f / 60.0f;
        constexpr Uint64 kTargetFrameMs = 16; // ~60 FPS (16.67 ms)

        int frame_counter = 0;
        bool running = true;

        while (running) {
            const Uint64 frame_start_ms = SDL_GetTicks64();

            // a) Coleta eventos de entrada via InputManager
            if (!input_mgr.PumpEvents()) {
                break;
            }

            const strikers::input::LogicalInputState p1 = input_source.Poll(0);

            // d) Permite sair do loop pelo botão Pause (Space/Enter/Start) ou fechando a janela
            if (p1.IsPressed(strikers::input::LogicalAction::PauseMenu)) {
                std::cout << "[Loop] Comando de Pause/Saida recebido no frame " << frame_counter << ".\n";
                break;
            }

            // b) Atualiza a cinemática do jogador com base no direcional (X, Z) e Dash
            const bool is_dashing = p1.IsHeld(strikers::input::LogicalAction::DashSprint);
            player_body.Update(p1.direction.x, p1.direction.y, is_dashing, kFixedDeltaSec);

            // Atualiza a câmera acompanhando o jogador pelo campo (105m x 68m)
            camera.UpdateFollowTarget(player_body.GetPosition(), kFixedDeltaSec);

            // c) Renderiza o campo, traves e o jogador orientado
            window.BeginFrame();
            if (window.HasOpenGLContext()) {
                camera.ApplyToOpenGL(window.GetAspectRatio());
                debug_renderer.RenderPitch();
                debug_renderer.RenderGoals();
                debug_renderer.RenderAthlete(player_body);
            }
            window.EndFrame();

            // Atualiza o título da janela periodicamente com telemetria da posição/velocidade
            if (frame_counter % 15 == 0) {
                const auto& pos = player_body.GetPosition();
                std::ostringstream title_ss;
                title_ss << "Open Strikers 2013 | " << endo->names.full_name
                         << " (SpeedStat=" << static_cast<int>(endo->attributes.speed_base) << ")"
                         << " | Pos=(" << std::fixed << std::setprecision(1) << pos.x << "m, " << pos.z << "m)"
                         << " | Vel=" << std::setprecision(1) << player_body.GetCurrentSpeed() << "m/s"
                         << (player_body.IsDashing() ? " [DASH]" : "");
                window.SetTitle(title_ss.str());
            }

            ++frame_counter;
            if (max_frames > 0 && frame_counter >= max_frames) {
                running = false;
            }

            // Frame pacing para garantir 60 FPS mesmo se VSync não bloquear no driver
            const Uint64 elapsed_ms = SDL_GetTicks64() - frame_start_ms;
            if (elapsed_ms < kTargetFrameMs) {
                SDL_Delay(static_cast<Uint32>(kTargetFrameMs - elapsed_ms));
            }
        }

        input_mgr.Shutdown();
        window.Shutdown();
        SDL_Quit();

        std::cout << "[OK] Aplicacao finalizada limpa apos " << frame_counter << " quadros.\n";
        return EXIT_SUCCESS;

    } catch (const std::exception& ex) {
        std::cerr << "[FALHA CRITICA] " << ex.what() << "\n";
        SDL_Quit();
        return EXIT_FAILURE;
    }
}
