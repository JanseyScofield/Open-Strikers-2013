#include "input/config_loader.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace strikers::input {

namespace {

constexpr std::array<std::pair<LogicalAction, const char*>, 13> kActionNames = {{
    {LogicalAction::MoveUp,         "move_up"},
    {LogicalAction::MoveDown,       "move_down"},
    {LogicalAction::MoveLeft,       "move_left"},
    {LogicalAction::MoveRight,      "move_right"},
    {LogicalAction::PassSwitch,     "pass_switch"},
    {LogicalAction::ShootSlide,     "shoot_slide"},
    {LogicalAction::DribbleSpecial, "dribble_special"},
    {LogicalAction::DashSprint,     "dash_sprint"},
    {LogicalAction::TacticSwitch,   "tactic_switch"},
    {LogicalAction::HissatsuLevel,  "hissatsu_level"},
    {LogicalAction::PauseMenu,      "pause_menu"},
    {LogicalAction::Confirm,        "confirm"},
    {LogicalAction::Cancel,         "cancel"},
}};

std::string ToLowerAscii(std::string_view sv) {
    std::string out(sv);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

void PopulateDefaultGamepadBindings(PlayerBindingProfile& profile) {
    profile.pad_button_bindings = {
        {LogicalAction::MoveUp,         {SDL_CONTROLLER_BUTTON_DPAD_UP}},
        {LogicalAction::MoveDown,       {SDL_CONTROLLER_BUTTON_DPAD_DOWN}},
        {LogicalAction::MoveLeft,       {SDL_CONTROLLER_BUTTON_DPAD_LEFT}},
        {LogicalAction::MoveRight,      {SDL_CONTROLLER_BUTTON_DPAD_RIGHT}},
        {LogicalAction::PassSwitch,     {SDL_CONTROLLER_BUTTON_A}},
        {LogicalAction::ShootSlide,     {SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X}},
        {LogicalAction::DribbleSpecial, {SDL_CONTROLLER_BUTTON_Y}},
        {LogicalAction::DashSprint,     {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER}},
        {LogicalAction::TacticSwitch,   {SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_LEFTSTICK}},
        {LogicalAction::HissatsuLevel,  {SDL_CONTROLLER_BUTTON_LEFTSHOULDER}},
        {LogicalAction::PauseMenu,      {SDL_CONTROLLER_BUTTON_START}},
        {LogicalAction::Confirm,        {SDL_CONTROLLER_BUTTON_A}},
        {LogicalAction::Cancel,         {SDL_CONTROLLER_BUTTON_B}},
    };
    profile.move_axis_x = SDL_CONTROLLER_AXIS_LEFTX;
    profile.move_axis_y = SDL_CONTROLLER_AXIS_LEFTY;
}

} // namespace

std::string ConfigLoader::ActionToString(LogicalAction action) {
    for (const auto& [act, name] : kActionNames) {
        if (act == action) {
            return name;
        }
    }
    return "none";
}

LogicalAction ConfigLoader::StringToAction(const std::string& name) {
    const std::string norm = ToLowerAscii(name);
    for (const auto& [act, act_name] : kActionNames) {
        if (norm == act_name) {
            return act;
        }
    }
    return LogicalAction::None;
}

SDL_Scancode ConfigLoader::ParseScancode(const std::string& key_name) {
    if (key_name.empty()) {
        return SDL_SCANCODE_UNKNOWN;
    }
    SDL_Scancode sc = SDL_GetScancodeFromName(key_name.c_str());
    if (sc != SDL_SCANCODE_UNKNOWN) {
        return sc;
    }

    const std::string lower = ToLowerAscii(key_name);
    if (lower == "enter" || lower == "return") return SDL_SCANCODE_RETURN;
    if (lower == "space" || lower == "spacebar") return SDL_SCANCODE_SPACE;
    if (lower == "esc" || lower == "escape") return SDL_SCANCODE_ESCAPE;
    if (lower == "lshift" || lower == "left shift") return SDL_SCANCODE_LSHIFT;
    if (lower == "rshift" || lower == "right shift") return SDL_SCANCODE_RSHIFT;
    if (lower == "lctrl" || lower == "left ctrl") return SDL_SCANCODE_LCTRL;
    if (lower == "rctrl" || lower == "right ctrl") return SDL_SCANCODE_RCTRL;
    if (lower == "up") return SDL_SCANCODE_UP;
    if (lower == "down") return SDL_SCANCODE_DOWN;
    if (lower == "left") return SDL_SCANCODE_LEFT;
    if (lower == "right") return SDL_SCANCODE_RIGHT;
    if (lower == "tab") return SDL_SCANCODE_TAB;
    if (lower == "backspace") return SDL_SCANCODE_BACKSPACE;
    return SDL_SCANCODE_UNKNOWN;
}

std::string ConfigLoader::ScancodeToString(SDL_Scancode scancode) {
    const char* name = SDL_GetScancodeName(scancode);
    if (name && name[0] != '\0') {
        return std::string(name);
    }
    return "Unknown";
}

SDL_GameControllerButton ConfigLoader::ParseControllerButton(const std::string& btn_name) {
    if (btn_name.empty()) {
        return SDL_CONTROLLER_BUTTON_INVALID;
    }
    const std::string lower = ToLowerAscii(btn_name);
    SDL_GameControllerButton btn = SDL_GameControllerGetButtonFromString(lower.c_str());
    if (btn != SDL_CONTROLLER_BUTTON_INVALID) {
        return btn;
    }
    // Aliases DualShock / PlayStation e Xbox
    if (lower == "cross" || lower == "south") return SDL_CONTROLLER_BUTTON_A;
    if (lower == "circle" || lower == "east") return SDL_CONTROLLER_BUTTON_B;
    if (lower == "square" || lower == "west") return SDL_CONTROLLER_BUTTON_X;
    if (lower == "triangle" || lower == "north") return SDL_CONTROLLER_BUTTON_Y;
    if (lower == "l1" || lower == "lb") return SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
    if (lower == "r1" || lower == "rb") return SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    if (lower == "select" || lower == "share") return SDL_CONTROLLER_BUTTON_BACK;
    if (lower == "options") return SDL_CONTROLLER_BUTTON_START;
    if (lower == "l3") return SDL_CONTROLLER_BUTTON_LEFTSTICK;
    if (lower == "r3") return SDL_CONTROLLER_BUTTON_RIGHTSTICK;
    return SDL_CONTROLLER_BUTTON_INVALID;
}

std::string ConfigLoader::ControllerButtonToString(SDL_GameControllerButton button) {
    const char* s = SDL_GameControllerGetStringForButton(button);
    if (s && s[0] != '\0') {
        return std::string(s);
    }
    return "invalid";
}

InputBindingsConfig ConfigLoader::CreateDefaultConfig() {
    InputBindingsConfig cfg;
    cfg.schema_version = "1.0.0";

    for (int i = 0; i < kMaxLocalPlayers; ++i) {
        PlayerBindingProfile& p = cfg.players[static_cast<std::size_t>(i)];
        p.player_index = i;
        p.profile_name = "Player " + std::to_string(i + 1);
        p.enable_keyboard = (i < 2);
        p.enable_gamepad = true;
        p.gamepad_device_index = i;
        p.analog_deadzone = 0.18f;
        p.trigger_threshold = 0.35f;
        PopulateDefaultGamepadBindings(p);
    }

    // Jogador 1: Layout ergonômico WASD + J/K/L/U/I + Espaço/Enter
    cfg.players[0].key_bindings = {
        {LogicalAction::MoveUp,         {SDL_SCANCODE_W}},
        {LogicalAction::MoveDown,       {SDL_SCANCODE_S}},
        {LogicalAction::MoveLeft,       {SDL_SCANCODE_A}},
        {LogicalAction::MoveRight,      {SDL_SCANCODE_D}},
        {LogicalAction::PassSwitch,     {SDL_SCANCODE_J}},
        {LogicalAction::ShootSlide,     {SDL_SCANCODE_K}},
        {LogicalAction::DribbleSpecial, {SDL_SCANCODE_L, SDL_SCANCODE_U}},
        {LogicalAction::DashSprint,     {SDL_SCANCODE_L, SDL_SCANCODE_LSHIFT}},
        {LogicalAction::TacticSwitch,   {SDL_SCANCODE_O, SDL_SCANCODE_TAB}},
        {LogicalAction::HissatsuLevel,  {SDL_SCANCODE_I, SDL_SCANCODE_E}},
        {LogicalAction::PauseMenu,      {SDL_SCANCODE_SPACE, SDL_SCANCODE_RETURN, SDL_SCANCODE_ESCAPE}},
        {LogicalAction::Confirm,        {SDL_SCANCODE_J, SDL_SCANCODE_RETURN}},
        {LogicalAction::Cancel,         {SDL_SCANCODE_ESCAPE}},
    };

    // Jogador 2: Setas Direcionais + Teclado Numérico
    cfg.players[1].key_bindings = {
        {LogicalAction::MoveUp,         {SDL_SCANCODE_UP}},
        {LogicalAction::MoveDown,       {SDL_SCANCODE_DOWN}},
        {LogicalAction::MoveLeft,       {SDL_SCANCODE_LEFT}},
        {LogicalAction::MoveRight,      {SDL_SCANCODE_RIGHT}},
        {LogicalAction::PassSwitch,     {SDL_SCANCODE_KP_1}},
        {LogicalAction::ShootSlide,     {SDL_SCANCODE_KP_2}},
        {LogicalAction::DribbleSpecial, {SDL_SCANCODE_KP_3}},
        {LogicalAction::DashSprint,     {SDL_SCANCODE_KP_0, SDL_SCANCODE_RSHIFT}},
        {LogicalAction::TacticSwitch,   {SDL_SCANCODE_KP_5}},
        {LogicalAction::HissatsuLevel,  {SDL_SCANCODE_KP_4}},
        {LogicalAction::PauseMenu,      {SDL_SCANCODE_KP_ENTER, SDL_SCANCODE_ESCAPE}},
        {LogicalAction::Confirm,        {SDL_SCANCODE_KP_1}},
        {LogicalAction::Cancel,         {SDL_SCANCODE_ESCAPE}},
    };

    return cfg;
}

InputBindingsConfig ConfigLoader::LoadFromFile(const std::filesystem::path& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        throw std::runtime_error("Falha ao abrir arquivo de mapeamento de entrada: " + filepath.string());
    }

    nlohmann::json root = nlohmann::json::parse(ifs);
    if (!root.is_object()) {
        throw std::runtime_error("Formato inválido em bindings.json: raiz não é um objeto JSON.");
    }

    InputBindingsConfig config = CreateDefaultConfig();
    config.schema_version = root.value("schema_version", "1.0.0");

    if (!root.contains("players") || !root.at("players").is_array()) {
        throw std::runtime_error("Formato inválido em bindings.json: array 'players' ausente.");
    }

    const auto& players_json = root.at("players");
    for (const auto& p_node : players_json) {
        if (!p_node.is_object()) {
            continue;
        }
        int p_idx = p_node.value("player_index", -1);
        if (p_idx < 0 || p_idx >= kMaxLocalPlayers) {
            continue;
        }

        PlayerBindingProfile& profile = config.players[static_cast<std::size_t>(p_idx)];
        profile.player_index = p_idx;
        profile.profile_name = p_node.value("profile_name", profile.profile_name);
        profile.enable_keyboard = p_node.value("enable_keyboard", profile.enable_keyboard);
        profile.enable_gamepad = p_node.value("enable_gamepad", profile.enable_gamepad);
        profile.gamepad_device_index = p_node.value("gamepad_device_index", p_idx);
        profile.analog_deadzone = p_node.value("analog_deadzone", 0.18f);
        profile.trigger_threshold = p_node.value("trigger_threshold", 0.35f);

        if (p_node.contains("keyboard") && p_node.at("keyboard").is_object()) {
            profile.key_bindings.clear();
            for (const auto& item : p_node.at("keyboard").items()) {
                LogicalAction act = StringToAction(item.key());
                if (act == LogicalAction::None) {
                    continue;
                }
                std::vector<SDL_Scancode> scancodes;
                if (item.value().is_array()) {
                    for (const auto& k_elem : item.value()) {
                        if (k_elem.is_string()) {
                            SDL_Scancode sc = ParseScancode(k_elem.get<std::string>());
                            if (sc != SDL_SCANCODE_UNKNOWN) {
                                scancodes.push_back(sc);
                            }
                        }
                    }
                } else if (item.value().is_string()) {
                    SDL_Scancode sc = ParseScancode(item.value().get<std::string>());
                    if (sc != SDL_SCANCODE_UNKNOWN) {
                        scancodes.push_back(sc);
                    }
                }
                profile.key_bindings[act] = std::move(scancodes);
            }
        }

        if (p_node.contains("gamepad") && p_node.at("gamepad").is_object()) {
            const auto& gp_node = p_node.at("gamepad");
            if (gp_node.contains("move_axis_x") && gp_node.at("move_axis_x").is_string()) {
                SDL_GameControllerAxis ax = SDL_GameControllerGetAxisFromString(
                    gp_node.at("move_axis_x").get<std::string>().c_str()
                );
                if (ax != SDL_CONTROLLER_AXIS_INVALID) {
                    profile.move_axis_x = ax;
                }
            }
            if (gp_node.contains("move_axis_y") && gp_node.at("move_axis_y").is_string()) {
                SDL_GameControllerAxis ay = SDL_GameControllerGetAxisFromString(
                    gp_node.at("move_axis_y").get<std::string>().c_str()
                );
                if (ay != SDL_CONTROLLER_AXIS_INVALID) {
                    profile.move_axis_y = ay;
                }
            }
            if (gp_node.contains("buttons") && gp_node.at("buttons").is_object()) {
                profile.pad_button_bindings.clear();
                for (const auto& item : gp_node.at("buttons").items()) {
                    LogicalAction act = StringToAction(item.key());
                    if (act == LogicalAction::None) {
                        continue;
                    }
                    std::vector<SDL_GameControllerButton> buttons;
                    if (item.value().is_array()) {
                        for (const auto& b_elem : item.value()) {
                            if (b_elem.is_string()) {
                                SDL_GameControllerButton btn = ParseControllerButton(b_elem.get<std::string>());
                                if (btn != SDL_CONTROLLER_BUTTON_INVALID) {
                                    buttons.push_back(btn);
                                }
                            }
                        }
                    } else if (item.value().is_string()) {
                        SDL_GameControllerButton btn = ParseControllerButton(item.value().get<std::string>());
                        if (btn != SDL_CONTROLLER_BUTTON_INVALID) {
                            buttons.push_back(btn);
                        }
                    }
                    profile.pad_button_bindings[act] = std::move(buttons);
                }
            }
        }
    }

    return config;
}

void ConfigLoader::SaveToFile(const InputBindingsConfig& config, const std::filesystem::path& filepath) {
    if (filepath.has_parent_path()) {
        std::filesystem::create_directories(filepath.parent_path());
    }

    nlohmann::json root = nlohmann::json::object();
    root["schema_version"] = config.schema_version;

    nlohmann::json players_arr = nlohmann::json::array();
    for (const auto& p : config.players) {
        nlohmann::json p_obj = nlohmann::json::object();
        p_obj["player_index"] = p.player_index;
        p_obj["profile_name"] = p.profile_name;
        p_obj["enable_keyboard"] = p.enable_keyboard;
        p_obj["enable_gamepad"] = p.enable_gamepad;
        p_obj["gamepad_device_index"] = p.gamepad_device_index;
        p_obj["analog_deadzone"] = p.analog_deadzone;
        p_obj["trigger_threshold"] = p.trigger_threshold;

        nlohmann::json kb_obj = nlohmann::json::object();
        for (const auto& [act, sc_list] : p.key_bindings) {
            nlohmann::json keys_arr = nlohmann::json::array();
            for (SDL_Scancode sc : sc_list) {
                keys_arr.push_back(ScancodeToString(sc));
            }
            kb_obj[ActionToString(act)] = std::move(keys_arr);
        }
        p_obj["keyboard"] = std::move(kb_obj);

        nlohmann::json gp_obj = nlohmann::json::object();
        const char* ax_x = SDL_GameControllerGetStringForAxis(p.move_axis_x);
        const char* ax_y = SDL_GameControllerGetStringForAxis(p.move_axis_y);
        gp_obj["move_axis_x"] = ax_x ? ax_x : "leftx";
        gp_obj["move_axis_y"] = ax_y ? ax_y : "lefty";

        nlohmann::json btns_obj = nlohmann::json::object();
        for (const auto& [act, btn_list] : p.pad_button_bindings) {
            nlohmann::json btn_arr = nlohmann::json::array();
            for (SDL_GameControllerButton btn : btn_list) {
                btn_arr.push_back(ControllerButtonToString(btn));
            }
            btns_obj[ActionToString(act)] = std::move(btn_arr);
        }
        gp_obj["buttons"] = std::move(btns_obj);
        p_obj["gamepad"] = std::move(gp_obj);

        players_arr.push_back(std::move(p_obj));
    }
    root["players"] = std::move(players_arr);

    std::ofstream ofs(filepath);
    if (!ofs.is_open()) {
        throw std::runtime_error("Falha ao gravar arquivo de mapeamento de entrada: " + filepath.string());
    }
    ofs << root.dump(2) << '\n';
}

} // namespace strikers::input
