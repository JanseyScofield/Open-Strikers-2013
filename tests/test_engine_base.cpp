#include "core/character.hpp"
#include "gameplay/duel_system.hpp"
#include "input/config_loader.hpp"
#include "input/input_manager.hpp"
#include "input/input_state.hpp"
#include "loader/character_loader.hpp"
#include "physics/ball.hpp"
#include "physics/kinematic_body.hpp"
#include "render/camera.hpp"

#include <SDL2/SDL.h>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {

std::filesystem::path ResolveRepoPath(const std::filesystem::path& relative_target) {
    if (std::filesystem::exists(relative_target)) {
        return relative_target;
    }
    const std::filesystem::path parent_one = std::filesystem::path("..") / relative_target;
    if (std::filesystem::exists(parent_one)) {
        return parent_one;
    }
    const std::filesystem::path parent_two = std::filesystem::path("../..") / relative_target;
    if (std::filesystem::exists(parent_two)) {
        return parent_two;
    }
    return relative_target;
}

void PrintCharacterDetails(const strikers::core::Character& ch) {
    std::cout << "  ----------------------------------------------------------------------\n";
    std::cout << "  [Character ID #" << ch.character_id << "] "
              << ch.names.full_name << " (" << ch.names.furigana << ")\n";
    std::cout << "    HUD / Curto   : " << ch.names.hud_name << " / " << ch.names.short_name << "\n";
    std::cout << "    Model Code    : " << ch.model_code
              << " | Base Person ID: " << ch.base_person_id
              << " | Camisa #" << static_cast<int>(ch.default_jersey_number) << "\n";
    std::cout << "    Posicao / Elem: " << ch.position_name << " | " << ch.element_name << "\n";
    std::cout << "    Atributos (Base -> Max):\n";
    std::cout << "      TP     : " << std::setw(3) << static_cast<int>(ch.attributes.tp_base)
              << " -> " << std::setw(3) << static_cast<int>(ch.attributes.tp_max)
              << " | Kick    : " << std::setw(2) << static_cast<int>(ch.attributes.kick_base)
              << " -> " << std::setw(2) << static_cast<int>(ch.attributes.kick_max) << "\n";
    std::cout << "      Catch  : " << std::setw(3) << static_cast<int>(ch.attributes.catch_base)
              << " -> " << std::setw(3) << static_cast<int>(ch.attributes.catch_max)
              << " | Body    : " << std::setw(2) << static_cast<int>(ch.attributes.body_base)
              << " -> " << std::setw(2) << static_cast<int>(ch.attributes.body_max) << "\n";
    std::cout << "      Control: " << std::setw(3) << static_cast<int>(ch.attributes.control_base)
              << " -> " << std::setw(3) << static_cast<int>(ch.attributes.control_max)
              << " | Guard   : " << std::setw(2) << static_cast<int>(ch.attributes.guard_base)
              << " -> " << std::setw(2) << static_cast<int>(ch.attributes.guard_max) << "\n";
    std::cout << "      Speed  : " << std::setw(3) << static_cast<int>(ch.attributes.speed_base)
              << " -> " << std::setw(3) << static_cast<int>(ch.attributes.speed_max)
              << " | Keshin+ : +" << static_cast<int>(ch.attributes.keshin_stat_bonus)
              << " | Gauge Rate: " << static_cast<int>(ch.attributes.inazuma_gauge_rate) << "\n";

    std::cout << "    Tecnicas Intrinsecas (" << ch.hissatsu_loadout.intrinsic_techniques.size() << "):\n";
    for (const auto& slot : ch.hissatsu_loadout.intrinsic_techniques) {
        std::cout << "      - [" << slot.slot_type << "] ID=" << std::setw(3) << slot.hissatsu_id
                  << " \"" << slot.name << "\" (" << slot.category << ", " << slot.element
                  << ", Lv" << static_cast<int>(slot.level_tier)
                  << ", Power=" << slot.base_power << "->" << slot.max_power
                  << ", TP=" << slot.tp_cost
                  << ", " << static_cast<int>(slot.participant_count) << "P)\n";
    }
}

void PushSyntheticKeyEvent(Uint32 type, SDL_Scancode scancode) {
    SDL_Event ev{};
    ev.type = type;
    ev.key.type = type;
    ev.key.state = (type == SDL_KEYDOWN) ? SDL_PRESSED : SDL_RELEASED;
    ev.key.repeat = 0;
    ev.key.keysym.scancode = scancode;
    ev.key.keysym.sym = static_cast<SDL_Keycode>(scancode);
    SDL_PushEvent(&ev);
}

void PushSyntheticPadAxisEvent(SDL_JoystickID which, SDL_GameControllerAxis axis, Sint16 value) {
    SDL_Event ev{};
    ev.type = SDL_CONTROLLERAXISMOTION;
    ev.caxis.type = SDL_CONTROLLERAXISMOTION;
    ev.caxis.which = which;
    ev.caxis.axis = static_cast<Uint8>(axis);
    ev.caxis.value = value;
    SDL_PushEvent(&ev);
}

void PushSyntheticPadButtonEvent(Uint32 type, SDL_JoystickID which, SDL_GameControllerButton button) {
    SDL_Event ev{};
    ev.type = type;
    ev.cbutton.type = type;
    ev.cbutton.which = which;
    ev.cbutton.button = static_cast<Uint8>(button);
    ev.cbutton.state = (type == SDL_CONTROLLERBUTTONDOWN) ? SDL_PRESSED : SDL_RELEASED;
    SDL_PushEvent(&ev);
}

} // namespace

int main() {
    try {
        std::cout << "======================================================================\n";
        std::cout << "  Inazuma Eleven GO Strikers 2013 - Teste de Base do Motor (C++20)\n";
        std::cout << "======================================================================\n\n";

        // ------------------------------------------------------------------
        // Etapa A: Inicialização da SDL2 e do Subsistema de Entrada (PC HAL)
        // ------------------------------------------------------------------
        SDL_version sdl_ver{};
        SDL_GetVersion(&sdl_ver);
        std::cout << "[1/3] Inicializando SDL2 (v"
                  << static_cast<int>(sdl_ver.major) << "."
                  << static_cast<int>(sdl_ver.minor) << "."
                  << static_cast<int>(sdl_ver.patch) << ") e InputManager...\n";

        const auto bindings_path = ResolveRepoPath("config/bindings.json");
        strikers::input::InputManager input_mgr;
        if (!input_mgr.Initialize()) {
            std::cerr << "[ERRO] Falha ao inicializar SDL2 InputManager: " << SDL_GetError() << "\n";
            return EXIT_FAILURE;
        }
        input_mgr.LoadBindings(bindings_path);
        std::cout << "  - Mapeamentos carregados de : " << bindings_path.string() << "\n";
        std::cout << "  - Perfil P1                 : "
                  << input_mgr.GetBindingsConfig().players[0].profile_name << "\n\n";

        // ------------------------------------------------------------------
        // Etapa B & C: Ingestão do Catálogo de Personagens e Técnicas
        // ------------------------------------------------------------------
        const auto catalog_path = ResolveRepoPath("output/data/characters_parsed.json");
        std::cout << "[2/3] Carregando e validando catalogo de dominio: " << catalog_path.string() << "...\n";

        const strikers::loader::CharacterCatalog catalog =
            strikers::loader::CharacterLoader::LoadFromFile(catalog_path);

        std::map<strikers::core::Position, int> pos_counts;
        std::map<strikers::core::Element, int> elem_counts;
        for (const auto& ch : catalog.characters) {
            pos_counts[ch.position]++;
            elem_counts[ch.element]++;
        }

        std::cout << "  - Jogo                      : " << catalog.metadata.game_title
                  << " (" << catalog.metadata.game_id << ")\n";
        std::cout << "  - Personagens instanciados  : " << catalog.characters.size()
                  << " (esperado: " << strikers::core::kExpectedCharacterCount << ")\n";
        std::cout << "  - Tecnicas Hissatsu         : " << catalog.hissatsu_techniques.size()
                  << " (esperado: " << strikers::core::kExpectedHissatsuCount << ")\n";
        std::cout << "  - Distribuicao por Posicao  : "
                  << "GK=" << pos_counts[strikers::core::Position::Goalkeeper] << ", "
                  << "DF=" << pos_counts[strikers::core::Position::Defender] << ", "
                  << "MF=" << pos_counts[strikers::core::Position::Midfielder] << ", "
                  << "FW=" << pos_counts[strikers::core::Position::Forward] << ", "
                  << "Manager=" << pos_counts[strikers::core::Position::Manager] << "\n";
        std::cout << "  - Distribuicao por Elemento : "
                  << "Wind=" << elem_counts[strikers::core::Element::Wind] << ", "
                  << "Wood=" << elem_counts[strikers::core::Element::Wood] << ", "
                  << "Fire=" << elem_counts[strikers::core::Element::Fire] << ", "
                  << "Mountain=" << elem_counts[strikers::core::Element::Mountain] << "\n";

        // Exibe exemplos representativos (Endo Mamoru #1 e Gouenji Shuuya #23)
        if (const auto* endo = catalog.FindCharacterById(1)) {
            PrintCharacterDetails(*endo);
        } else {
            throw std::runtime_error("Personagem cid=1 (Endo) nao encontrado no catalogo.");
        }

        if (const auto* gouenji = catalog.FindCharacterById(23)) {
            PrintCharacterDetails(*gouenji);
        }

        std::cout << "  ----------------------------------------------------------------------\n\n";

        // ------------------------------------------------------------------
        // Etapa D: Mini Loop de Polling da SDL2 (Verificação de Teclado e Gamepad)
        // ------------------------------------------------------------------
        std::cout << "[3/3] Executando mini loop de polling da SDL2 (8 quadros)...\n";
        strikers::input::IInputSource& abstract_input = input_mgr;

        constexpr int kTotalFrames = 8;
        for (int frame = 0; frame < kTotalFrames; ++frame) {
            // Injeta estímulos determinísticos na fila da SDL2 para comprovar o pipeline completo
            if (frame == 1) {
                // Frame 1: P1 pressiona W + D (diagonal superior-direita) e J (Passe/Troca)
                PushSyntheticKeyEvent(SDL_KEYDOWN, SDL_SCANCODE_W);
                PushSyntheticKeyEvent(SDL_KEYDOWN, SDL_SCANCODE_D);
                PushSyntheticKeyEvent(SDL_KEYDOWN, SDL_SCANCODE_J);
            } else if (frame == 2) {
                // Frame 2: P1 mantém W+D, solta J e pressiona K (Chute) + L (Drible/Dash)
                PushSyntheticKeyEvent(SDL_KEYUP, SDL_SCANCODE_J);
                PushSyntheticKeyEvent(SDL_KEYDOWN, SDL_SCANCODE_K);
                PushSyntheticKeyEvent(SDL_KEYDOWN, SDL_SCANCODE_L);
            } else if (frame == 3) {
                // Frame 3: Libera teclas de P1 e simula movimento analógico + botão de GameController
                PushSyntheticKeyEvent(SDL_KEYUP, SDL_SCANCODE_W);
                PushSyntheticKeyEvent(SDL_KEYUP, SDL_SCANCODE_D);
                PushSyntheticKeyEvent(SDL_KEYUP, SDL_SCANCODE_K);
                PushSyntheticKeyEvent(SDL_KEYUP, SDL_SCANCODE_L);

                PushSyntheticPadAxisEvent(0, SDL_CONTROLLER_AXIS_LEFTX, -24000);
                PushSyntheticPadAxisEvent(0, SDL_CONTROLLER_AXIS_LEFTY, 16000);
                PushSyntheticPadButtonEvent(SDL_CONTROLLERBUTTONDOWN, 0, SDL_CONTROLLER_BUTTON_Y);
                PushSyntheticPadButtonEvent(SDL_CONTROLLERBUTTONDOWN, 0, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
            } else if (frame == 4) {
                // Frame 4: Pressiona Space (Pause) e centraliza o eixo analógico
                PushSyntheticPadAxisEvent(0, SDL_CONTROLLER_AXIS_LEFTX, 0);
                PushSyntheticPadAxisEvent(0, SDL_CONTROLLER_AXIS_LEFTY, 0);
                PushSyntheticPadButtonEvent(SDL_CONTROLLERBUTTONUP, 0, SDL_CONTROLLER_BUTTON_Y);
                PushSyntheticPadButtonEvent(SDL_CONTROLLERBUTTONUP, 0, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
                PushSyntheticKeyEvent(SDL_KEYDOWN, SDL_SCANCODE_SPACE);
            } else if (frame == 5) {
                PushSyntheticKeyEvent(SDL_KEYUP, SDL_SCANCODE_SPACE);
            }

            if (!input_mgr.PumpEvents()) {
                std::cout << "  [Frame " << frame << "] Evento SDL_QUIT recebido.\n";
                break;
            }

            const strikers::input::LogicalInputState p1 = abstract_input.Poll(0);

            std::cout << "  [Frame " << frame << "] P1 Axis=("
                      << std::fixed << std::setprecision(2) << std::setw(5) << p1.direction.x << ", "
                      << std::setw(5) << p1.direction.y << ")"
                      << " | Held=0x" << std::hex << std::setw(4) << std::setfill('0') << p1.buttons_held
                      << " | Pressed=0x" << std::setw(4) << p1.buttons_pressed
                      << " | Released=0x" << std::setw(4) << p1.buttons_released
                      << std::dec << std::setfill(' ');

            if (p1.IsPressed(strikers::input::LogicalAction::PassSwitch))     std::cout << " [+PASS]";
            if (p1.IsPressed(strikers::input::LogicalAction::ShootSlide))     std::cout << " [+SHOOT]";
            if (p1.IsPressed(strikers::input::LogicalAction::DribbleSpecial)) std::cout << " [+DRIBBLE]";
            if (p1.IsPressed(strikers::input::LogicalAction::DashSprint))     std::cout << " [+DASH]";
            if (p1.IsPressed(strikers::input::LogicalAction::HissatsuLevel))  std::cout << " [+HISSATSU_LV]";
            if (p1.IsPressed(strikers::input::LogicalAction::PauseMenu))      std::cout << " [+PAUSE]";
            std::cout << "\n";

            // Asserções determinísticas sobre os estados capturados
            if (frame == 1) {
                if (!p1.IsPressed(strikers::input::LogicalAction::PassSwitch) ||
                    std::fabs(p1.direction.Magnitude() - 1.0f) > 0.01f) {
                    throw std::runtime_error("Falha de assercao no Frame 1 (diagonal normalizada + PassSwitch).");
                }
            } else if (frame == 2) {
                if (!p1.IsReleased(strikers::input::LogicalAction::PassSwitch) ||
                    !p1.IsPressed(strikers::input::LogicalAction::ShootSlide) ||
                    !p1.IsHeld(strikers::input::LogicalAction::DashSprint)) {
                    throw std::runtime_error("Falha de assercao no Frame 2 (ShootSlide + DashSprint).");
                }
            } else if (frame == 3) {
                if (!p1.IsHeld(strikers::input::LogicalAction::DribbleSpecial) ||
                    !p1.IsPressed(strikers::input::LogicalAction::HissatsuLevel) ||
                    p1.direction.x >= -0.5f || p1.direction.y <= 0.2f) {
                    throw std::runtime_error("Falha de assercao no Frame 3 (Gamepad Axis + DribbleSpecial + HissatsuLevel).");
                }
            } else if (frame == 4) {
                if (!p1.IsPressed(strikers::input::LogicalAction::PauseMenu)) {
                    throw std::runtime_error("Falha de assercao no Frame 4 (PauseMenu via Space).");
                }
            }

            SDL_Delay(2);
        }

        input_mgr.Shutdown();
        SDL_Quit();

        // ------------------------------------------------------------------
        // Etapa E: Verificação Determinística de Cinemática e Limites de Campo
        // ------------------------------------------------------------------
        std::cout << "\n[4/6] Verificando cinematica (KinematicBody) e limites do gramado (105m x 68m)...\n";
        strikers::physics::KinematicBody endo_body({0.0f, 0.0f, 0.0f}, 20);
        constexpr float kDt = 1.0f / 60.0f;
        for (int i = 0; i < 600; ++i) {
            endo_body.Update(1.0f, -1.0f, true, kDt);
        }
        const auto& clamped_pos = endo_body.GetPosition();
        std::cout << "  - Posicao apos corrida diagonal longa : ("
                  << clamped_pos.x << "m, " << clamped_pos.z << "m)\n";
        if (std::fabs(clamped_pos.x - strikers::physics::PitchDimensions::kHalfLengthX) > 1e-3f ||
            std::fabs(clamped_pos.z + strikers::physics::PitchDimensions::kHalfWidthZ) > 1e-3f) {
            throw std::runtime_error("Falha no clamping de limites do gramado (105m x 68m).");
        }

        // ------------------------------------------------------------------
        // Etapa F: Verificação da Física da Bola (Ball)
        // ------------------------------------------------------------------
        std::cout << "\n[5/6] Verificando fisica, estados e quique da bola (Ball)...\n";
        strikers::physics::Ball ball({0.0f, 2.5f, 0.0f});
        if (!ball.IsFree()) {
            throw std::runtime_error("Bola inicial deve comecar no estado Free.");
        }
        // Simula queda livre com quique
        for (int i = 0; i < 60; ++i) {
            ball.Update(kDt);
        }
        if (ball.GetPosition().y < strikers::physics::Ball::kRadius) {
            throw std::runtime_error("Bola afundou abaixo do raio minimo de contato com o solo.");
        }

        // Simula posse, passe (com cooldown de re-captura) e desprendimento em chute
        ball.AttachToPlayer(1, {10.0f, 0.0f, 5.0f}, {1.0f, 0.0f, 0.0f});
        if (!ball.IsPossessed() || ball.GetPossessorId() != 1) {
            throw std::runtime_error("Falha ao associar posse da bola ao jogador.");
        }
        ball.Pass({1.0f, 0.0f, 0.0f}, 18.0f);
        if (!ball.IsFree() || ball.CanBePickedUp()) {
            throw std::runtime_error("Bola apos passe deve estar Free mas com cooldown de re-captura ativo.");
        }
        for (int i = 0; i < 30; ++i) {
            ball.Update(kDt);
        }
        if (!ball.CanBePickedUp()) {
            throw std::runtime_error("Cooldown de re-captura da bola deve expirar apos 0.35s.");
        }
        ball.AttachToPlayer(1, {15.0f, 0.0f, 5.0f}, {1.0f, 0.0f, 0.0f});
        ball.Shoot({52.5f, 1.2f, 0.0f}, 25.0f, 0.5f);
        if (!ball.IsInFlight()) {
            throw std::runtime_error("Bola deve estar no estado InFlight apos chute.");
        }

        // ------------------------------------------------------------------
        // Etapa G: Verificação das Fórmulas de Duelo (DuelSystem)
        // ------------------------------------------------------------------
        std::cout << "\n[6/6] Verificando formulas de confronto e resolucao (specs/BATTLE_FORMULAS.md)...\n";
        strikers::gameplay::DuelSystem duel_sys(20130920u);

        // 1. Ciclo Elemental: Floresta(1) > Vento(0) > Terra(3) > Fogo(2) > Floresta(1)
        using strikers::gameplay::BattleElement;
        if (strikers::gameplay::DuelSystem::CompareElements(BattleElement::Wood, BattleElement::Wind) != +1 ||
            strikers::gameplay::DuelSystem::CompareElements(BattleElement::Wind, BattleElement::Mountain) != +1 ||
            strikers::gameplay::DuelSystem::CompareElements(BattleElement::Mountain, BattleElement::Fire) != +1 ||
            strikers::gameplay::DuelSystem::CompareElements(BattleElement::Fire, BattleElement::Wood) != +1 ||
            strikers::gameplay::DuelSystem::CompareElements(BattleElement::Wind, BattleElement::Wood) != -1 ||
            strikers::gameplay::DuelSystem::CompareElements(BattleElement::Neutral, BattleElement::Fire) != 0) {
            throw std::runtime_error("Falha na matriz de comparacao elemental.");
        }

        // 2. Chute vs Goleiro com dados reais de catálogo
        const auto* gouenji_char = catalog.FindCharacterById(23); // Gouenji (Fire, FW, Kick 30)
        const auto* endo_char    = catalog.FindCharacterById(1);  // Endo (Mountain, GK, Catch 30)
        if (!gouenji_char || !endo_char) {
            throw std::runtime_error("Falha ao carregar Gouenji ou Endo para teste de duelo.");
        }

        auto gouenji_view = strikers::gameplay::PlayerBattleView::FromCharacter(*gouenji_char);
        auto endo_view    = strikers::gameplay::PlayerBattleView::FromCharacter(*endo_char);

        const auto* fire_tornado_tech = catalog.FindHissatsuById(337); // Fire Tornado (Lv1, Fire, Power 40)
        const auto* god_hand_tech     = catalog.FindHissatsuById(95);  // God Hand (Lv1, Mountain, Power 30)

        auto fire_tornado_param = strikers::gameplay::WazaBattleParam::FromHissatsu(*fire_tornado_tech);
        auto god_hand_param     = strikers::gameplay::WazaBattleParam::FromHissatsu(*god_hand_tech);

        const int shoot_power = duel_sys.ComputeShootPower(
            gouenji_view,
            &fire_tornado_param,
            0.9f,  // Carga máxima (+20)
            18.0f, // Distância ao gol (18m <= 25m, sem penalidade)
            18.0f  // Distância ao GK (à queima roupa)
        );

        auto defense_res = duel_sys.ResolveShotVsGoalkeeper(
            gouenji_view,
            &fire_tornado_param,
            shoot_power,
            endo_view,
            &god_hand_param,
            0.9f,
            true
        );

        std::cout << "  - Chute de Gouenji (Fire Tornado) : P_chute=" << shoot_power << "\n"
                  << "  - Defesa de Endo (God Hand)       : P_def=" << defense_res.defense_power << "\n"
                  << "  - Resultado da Finalizacao        : " << defense_res.summary << "\n";

        if (shoot_power <= 0 || defense_res.defense_power <= 0) {
            throw std::runtime_error("Poderes calculados de chute/defesa devem ser estritamente positivos.");
        }

        // 3. Duelo terrestre 1v1 e Hissatsu Command Battle
        auto tackle_res = duel_sys.ResolveNormalTackle(gouenji_view, endo_view);
        std::cout << "  - Duelo Terrestre 1v1 (Gouenji vs Endo) : " << tackle_res.summary << "\n";

        if (strikers::gameplay::DuelSystem::GetEffectiveStat(gouenji_view, strikers::gameplay::StatIndex::Kick) != gouenji_view.kick) {
            throw std::runtime_error("Falha em DuelSystem::GetEffectiveStat.");
        }
        const int cmd_battle = strikers::gameplay::DuelSystem::ResolveHissatsuCommandBattle(
            gouenji_view, &fire_tornado_param, endo_view, &god_hand_param
        );
        std::cout << "  - Hissatsu Command Battle               : resultado=" << cmd_battle << "\n";

        std::cout << "\n[SUCESSO] Todos os testes de dominio, cinematica, bola e formulas de combate passaram!\n";
        return EXIT_SUCCESS;

    } catch (const std::exception& ex) {
        std::cerr << "\n[FALHA CRITICA] Excecao capturada: " << ex.what() << "\n";
        SDL_Quit();
        return EXIT_FAILURE;
    }
}
