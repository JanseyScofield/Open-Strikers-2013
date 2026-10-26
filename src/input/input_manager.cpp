#include "input/input_manager.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace strikers::input {

InputManager::InputManager()
    : config_(ConfigLoader::CreateDefaultConfig()) {
    for (int i = 0; i < kMaxLocalPlayers; ++i) {
        current_states_[static_cast<std::size_t>(i)].player_index = i;
    }
}

InputManager::InputManager(InputBindingsConfig config)
    : config_(std::move(config)) {
    for (int i = 0; i < kMaxLocalPlayers; ++i) {
        current_states_[static_cast<std::size_t>(i)].player_index = i;
    }
}

InputManager::~InputManager() {
    Shutdown();
}

bool InputManager::Initialize() {
    if (initialized_) {
        return true;
    }

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    constexpr Uint32 kRequiredSubsystems =
        SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER;

    Uint32 already_init = SDL_WasInit(kRequiredSubsystems);
    if ((already_init & kRequiredSubsystems) != kRequiredSubsystems) {
        if (SDL_InitSubSystem(kRequiredSubsystems) != 0) {
            return false;
        }
        owned_sdl_init_ = true;
    }

    SDL_ClearError();
    event_key_state_.fill(false);
    previous_held_masks_.fill(0u);
    quit_requested_ = false;
    initialized_ = true;

    OpenAvailableControllers();
    return true;
}

void InputManager::Shutdown() noexcept {
    for (auto& slot : controllers_) {
        if (slot.handle != nullptr) {
            SDL_GameControllerClose(slot.handle);
            slot.handle = nullptr;
            slot.instance_id = -1;
        }
        slot.event_buttons.fill(false);
        slot.event_axes.fill(0);
    }

    if (owned_sdl_init_) {
        SDL_QuitSubSystem(SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
        owned_sdl_init_ = false;
    }
    initialized_ = false;
}

void InputManager::LoadBindings(const std::filesystem::path& filepath) {
    config_ = ConfigLoader::LoadFromFile(filepath);
}

void InputManager::SaveBindings(const std::filesystem::path& filepath) const {
    ConfigLoader::SaveToFile(config_, filepath);
}

void InputManager::OpenAvailableControllers() {
    const int num_joysticks = SDL_NumJoysticks();
    for (int joy_idx = 0; joy_idx < num_joysticks; ++joy_idx) {
        if (SDL_IsGameController(joy_idx) == SDL_TRUE) {
            OnControllerAdded(joy_idx);
        }
    }
}

void InputManager::OnControllerAdded(int device_index) {
    if (SDL_IsGameController(device_index) != SDL_TRUE) {
        return;
    }

    SDL_GameController* gc = SDL_GameControllerOpen(device_index);
    if (gc == nullptr) {
        return;
    }

    SDL_Joystick* joy = SDL_GameControllerGetJoystick(gc);
    SDL_JoystickID inst_id = (joy != nullptr) ? SDL_JoystickInstanceID(joy) : -1;

    // Verifica se já está registrado
    for (const auto& slot : controllers_) {
        if (slot.handle == gc || (inst_id != -1 && slot.instance_id == inst_id)) {
            SDL_GameControllerClose(gc);
            return;
        }
    }

    // Aloca no primeiro slot livre
    for (auto& slot : controllers_) {
        if (slot.handle == nullptr) {
            slot.handle = gc;
            slot.instance_id = inst_id;
            slot.event_buttons.fill(false);
            slot.event_axes.fill(0);
            return;
        }
    }

    SDL_GameControllerClose(gc);
}

void InputManager::OnControllerRemoved(SDL_JoystickID instance_id) {
    for (auto& slot : controllers_) {
        if (slot.handle != nullptr && slot.instance_id == instance_id) {
            SDL_GameControllerClose(slot.handle);
            slot.handle = nullptr;
            slot.instance_id = -1;
            slot.event_buttons.fill(false);
            slot.event_axes.fill(0);
        }
    }
}

bool InputManager::PumpEvents() {
    if (!initialized_) {
        return false;
    }

    SDL_Event ev{};
    while (SDL_PollEvent(&ev) != 0) {
        switch (ev.type) {
            case SDL_QUIT:
                quit_requested_ = true;
                break;

            case SDL_WINDOWEVENT:
                if (ev.window.event == SDL_WINDOWEVENT_CLOSE) {
                    quit_requested_ = true;
                }
                break;

            case SDL_KEYDOWN:
                if (ev.key.keysym.scancode >= 0 && ev.key.keysym.scancode < SDL_NUM_SCANCODES) {
                    event_key_state_[static_cast<std::size_t>(ev.key.keysym.scancode)] = true;
                }
                break;

            case SDL_KEYUP:
                if (ev.key.keysym.scancode >= 0 && ev.key.keysym.scancode < SDL_NUM_SCANCODES) {
                    event_key_state_[static_cast<std::size_t>(ev.key.keysym.scancode)] = false;
                }
                break;

            case SDL_CONTROLLERDEVICEADDED:
                OnControllerAdded(ev.cdevice.which);
                break;

            case SDL_CONTROLLERDEVICEREMOVED:
                OnControllerRemoved(ev.cdevice.which);
                break;

            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP: {
                const bool pressed = (ev.type == SDL_CONTROLLERBUTTONDOWN) || (ev.cbutton.state == SDL_PRESSED);
                for (std::size_t i = 0; i < controllers_.size(); ++i) {
                    auto& slot = controllers_[i];
                    if (slot.instance_id == ev.cbutton.which ||
                        (slot.handle == nullptr && static_cast<int>(i) == ev.cbutton.which)) {
                        if (ev.cbutton.button < SDL_CONTROLLER_BUTTON_MAX) {
                            slot.event_buttons[ev.cbutton.button] = pressed;
                        }
                    }
                }
                break;
            }

            case SDL_CONTROLLERAXISMOTION: {
                for (std::size_t i = 0; i < controllers_.size(); ++i) {
                    auto& slot = controllers_[i];
                    if (slot.instance_id == ev.caxis.which ||
                        (slot.handle == nullptr && static_cast<int>(i) == ev.caxis.which)) {
                        if (ev.caxis.axis < SDL_CONTROLLER_AXIS_MAX) {
                            slot.event_axes[ev.caxis.axis] = ev.caxis.value;
                        }
                    }
                }
                break;
            }

            default:
                break;
        }
    }

    for (int p = 0; p < kMaxLocalPlayers; ++p) {
        RebuildPlayerState(p);
    }

    return !quit_requested_;
}

float InputManager::NormalizeAxisValue(Sint16 raw_value, float deadzone) noexcept {
    const float norm = (raw_value < 0)
        ? (static_cast<float>(raw_value) / 32768.0f)
        : (static_cast<float>(raw_value) / 32767.0f);

    const float abs_val = std::fabs(norm);
    if (abs_val <= deadzone) {
        return 0.0f;
    }

    const float sign = (norm < 0.0f) ? -1.0f : 1.0f;
    const float rescaled = (abs_val - deadzone) / (1.0f - deadzone);
    return sign * std::clamp(rescaled, 0.0f, 1.0f);
}

void InputManager::RebuildPlayerState(int player_index) {
    if (player_index < 0 || player_index >= kMaxLocalPlayers) {
        return;
    }

    const auto idx = static_cast<std::size_t>(player_index);
    const PlayerBindingProfile& profile = config_.players[idx];

    int num_keys = 0;
    const Uint8* sdl_kb_state = SDL_GetKeyboardState(&num_keys);

    std::uint32_t held_mask = 0u;

    // 1. Leitura de Teclado (combina snapshot de hardware da SDL2 e fila de eventos)
    if (profile.enable_keyboard) {
        for (const auto& [action, scancodes] : profile.key_bindings) {
            for (SDL_Scancode sc : scancodes) {
                const int sc_idx = static_cast<int>(sc);
                if (sc_idx <= 0 || sc_idx >= SDL_NUM_SCANCODES) {
                    continue;
                }
                const bool hw_down = (sdl_kb_state != nullptr && sc_idx < num_keys && sdl_kb_state[sc_idx] != 0);
                const bool ev_down = event_key_state_[static_cast<std::size_t>(sc_idx)];
                if (hw_down || ev_down) {
                    held_mask |= ToMask(action);
                    break;
                }
            }
        }
    }

    // 2. Leitura de Gamepad (SDL_GameController + eventos)
    float analog_x = 0.0f;
    float analog_y = 0.0f;
    bool pad_connected = false;

    const int pad_idx = std::clamp(profile.gamepad_device_index, 0, kMaxLocalPlayers - 1);
    const ControllerSlot& pad_slot = controllers_[static_cast<std::size_t>(pad_idx)];

    if (profile.enable_gamepad) {
        SDL_GameController* gc = pad_slot.handle;
        pad_connected = (gc != nullptr && SDL_GameControllerGetAttached(gc) == SDL_TRUE);

        for (const auto& [action, buttons] : profile.pad_button_bindings) {
            for (SDL_GameControllerButton btn : buttons) {
                if (btn <= SDL_CONTROLLER_BUTTON_INVALID || btn >= SDL_CONTROLLER_BUTTON_MAX) {
                    continue;
                }
                const bool hw_btn = (pad_connected && SDL_GameControllerGetButton(gc, btn) != 0);
                const bool ev_btn = pad_slot.event_buttons[static_cast<std::size_t>(btn)];
                if (hw_btn || ev_btn) {
                    held_mask |= ToMask(action);
                    break;
                }
            }
        }

        // Eixos Analógicos (Stick Esquerdo / Configurado)
        Sint16 raw_x = pad_slot.event_axes[static_cast<std::size_t>(profile.move_axis_x)];
        Sint16 raw_y = pad_slot.event_axes[static_cast<std::size_t>(profile.move_axis_y)];
        if (pad_connected) {
            const Sint16 hw_x = SDL_GameControllerGetAxis(gc, profile.move_axis_x);
            const Sint16 hw_y = SDL_GameControllerGetAxis(gc, profile.move_axis_y);
            if (hw_x != 0) raw_x = hw_x;
            if (hw_y != 0) raw_y = hw_y;

            // Gatilhos analógicos (L2/LT = Nível de Hissatsu, R2/RT = Dash/Corrida)
            const Sint16 trig_l = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
            const Sint16 trig_r = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
            if (NormalizeAxisValue(trig_l, profile.trigger_threshold) > 0.0f) {
                held_mask |= ToMask(LogicalAction::HissatsuLevel);
            }
            if (NormalizeAxisValue(trig_r, profile.trigger_threshold) > 0.0f) {
                held_mask |= ToMask(LogicalAction::DashSprint);
            }
        }

        analog_x = NormalizeAxisValue(raw_x, profile.analog_deadzone);
        analog_y = NormalizeAxisValue(raw_y, profile.analog_deadzone);
    }

    // 3. Síntese do Vetor Direcional (Combina D-Pad/WASD Digital + Analógico com normalização unitária)
    float digital_x = 0.0f;
    float digital_y = 0.0f;
    if ((held_mask & ToMask(LogicalAction::MoveLeft)) != 0u)  digital_x -= 1.0f;
    if ((held_mask & ToMask(LogicalAction::MoveRight)) != 0u) digital_x += 1.0f;
    if ((held_mask & ToMask(LogicalAction::MoveUp)) != 0u)    digital_y -= 1.0f;
    if ((held_mask & ToMask(LogicalAction::MoveDown)) != 0u)  digital_y += 1.0f;

    float final_x = (std::fabs(analog_x) > std::fabs(digital_x)) ? analog_x : digital_x;
    float final_y = (std::fabs(analog_y) > std::fabs(digital_y)) ? analog_y : digital_y;

    const float mag = std::sqrt(final_x * final_x + final_y * final_y);
    if (mag > 1.0f) {
        final_x /= mag;
        final_y /= mag;
    }

    // Atualiza flags digitais caso o stick analógico esteja defletido além de 0.5
    if (final_x <= -0.5f) held_mask |= ToMask(LogicalAction::MoveLeft);
    if (final_x >=  0.5f) held_mask |= ToMask(LogicalAction::MoveRight);
    if (final_y <= -0.5f) held_mask |= ToMask(LogicalAction::MoveUp);
    if (final_y >=  0.5f) held_mask |= ToMask(LogicalAction::MoveDown);

    const std::uint32_t prev_mask = previous_held_masks_[idx];
    LogicalInputState& state = current_states_[idx];
    state.player_index = player_index;
    state.direction = DirectionalAxis{final_x, final_y};
    state.buttons_held = held_mask;
    state.buttons_pressed = held_mask & (~prev_mask);
    state.buttons_released = (~held_mask) & prev_mask;
    state.controller_connected = pad_connected;

    previous_held_masks_[idx] = held_mask;
}

LogicalInputState InputManager::Poll(int player_index) {
    if (player_index < 0 || player_index >= kMaxLocalPlayers) {
        return LogicalInputState{player_index, {0.0f, 0.0f}, 0u, 0u, 0u, false};
    }
    return current_states_[static_cast<std::size_t>(player_index)];
}

} // namespace strikers::input
