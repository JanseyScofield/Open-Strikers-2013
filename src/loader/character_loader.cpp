#include "loader/character_loader.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace strikers::loader {

namespace {

core::LinkedHissatsuSlot ParseLinkedSlot(const nlohmann::json& node) {
    core::LinkedHissatsuSlot slot;
    slot.slot_index = node.value("slot_index", -1);
    slot.slot_type = node.value("slot_type", "");
    slot.hissatsu_id = node.at("hissatsu_id").get<std::uint16_t>();
    slot.name = node.value("name", "");
    slot.category = node.value("category", "");
    slot.element = node.value("element", "");
    slot.special_attribute = node.value("special_attribute", "");
    slot.level_tier = node.value("level_tier", static_cast<std::uint8_t>(1));
    slot.base_power = node.value("base_power", static_cast<std::uint16_t>(0));
    slot.max_power = node.value("max_power", static_cast<std::uint16_t>(0));
    slot.tp_cost = node.value("tp_cost", static_cast<std::uint16_t>(0));
    slot.participant_count = node.value("participant_count", static_cast<std::uint8_t>(1));
    return slot;
}

core::HissatsuTechnique ParseHissatsuTechnique(const nlohmann::json& node) {
    core::HissatsuTechnique tech;
    tech.hissatsu_id = node.at("hissatsu_id").get<std::uint16_t>();
    tech.name_str_id = node.value("name_str_id", 0u);
    tech.name_byte_offset = node.value("name_byte_offset", 0u);
    tech.name = node.at("name").get<std::string>();
    tech.internal_name_offset = node.value("internal_name_offset", 0u);
    tech.evolved_str_id = node.value("evolved_str_id", 0u);
    tech.evolved_name = node.value("evolved_name", "");
    tech.ruby_str_id = node.value("ruby_str_id", 0u);
    tech.ruby = node.value("ruby", "");

    const int cat_id = node.at("category_id").get<int>();
    tech.category = core::HissatsuCategoryFromId(cat_id);
    tech.category_name = node.value("category_name", std::string(core::HissatsuCategoryToString(tech.category)));

    const int elem_id = node.at("element_id").get<int>();
    tech.element = core::ElementFromId(elem_id);
    tech.element_name = node.value("element_name", std::string(core::ElementToString(tech.element)));

    const int attr_id = node.value("special_attribute_id", 0);
    tech.special_attribute = core::HissatsuSpecialAttributeFromId(attr_id);
    tech.special_attribute_name = node.value(
        "special_attribute_name",
        std::string(core::HissatsuSpecialAttributeToString(tech.special_attribute))
    );

    tech.level_tier = node.at("level_tier").get<std::uint8_t>();
    tech.base_power = node.at("base_power").get<std::uint16_t>();
    tech.max_power = node.at("max_power").get<std::uint16_t>();
    tech.tp_cost = node.at("tp_cost").get<std::uint16_t>();
    tech.participant_count = node.at("participant_count").get<std::uint8_t>();
    tech.kizuna_bond_required = node.value("kizuna_bond_required", static_cast<std::uint16_t>(0));

    if (node.contains("primary_user_char_ids")) {
        tech.primary_user_char_ids = node.at("primary_user_char_ids").get<std::vector<std::uint16_t>>();
    }
    if (node.contains("partner_1_char_ids")) {
        tech.partner_1_char_ids = node.at("partner_1_char_ids").get<std::vector<std::uint16_t>>();
    }
    if (node.contains("partner_2_char_ids")) {
        tech.partner_2_char_ids = node.at("partner_2_char_ids").get<std::vector<std::uint16_t>>();
    }

    return tech;
}

core::Character ParseCharacter(const nlohmann::json& node) {
    core::Character ch;
    ch.character_id = node.at("character_id").get<std::uint16_t>();
    ch.record_byte_offset = node.at("record_byte_offset").get<std::uint32_t>();
    ch.base_person_id = node.at("base_person_id").get<std::uint32_t>();
    ch.model_code = node.at("model_code").get<std::string>();
    ch.default_jersey_number = node.at("default_jersey_number").get<std::uint8_t>();

    const int elem_id = node.at("element_id").get<int>();
    ch.element = core::ElementFromId(elem_id);
    ch.element_name = node.value("element_name", std::string(core::ElementToString(ch.element)));

    const int pos_id = node.at("position_id").get<int>();
    ch.position = core::PositionFromId(pos_id);
    ch.position_name = node.value("position_name", std::string(core::PositionToString(ch.position)));

    ch.team_affiliation_flag_1 = node.value("team_affiliation_flag_1", 0u);
    ch.team_affiliation_flag_2 = node.value("team_affiliation_flag_2", 0u);

    // 1. Names
    const auto& names_node = node.at("names");
    ch.names.full_name_str_id = names_node.value("full_name_str_id", 0u);
    ch.names.full_name_byte_offset = names_node.value("full_name_byte_offset", 0u);
    ch.names.full_name_raw = names_node.value("full_name_raw", "");
    ch.names.full_name = names_node.at("full_name").get<std::string>();
    ch.names.furigana = names_node.value("furigana", "");
    ch.names.short_name_str_id = names_node.value("short_name_str_id", 0u);
    ch.names.short_name_byte_offset = names_node.value("short_name_byte_offset", 0u);
    ch.names.short_name = names_node.at("short_name").get<std::string>();
    ch.names.hud_name_str_id = names_node.value("hud_name_str_id", 0u);
    ch.names.hud_name_byte_offset = names_node.value("hud_name_byte_offset", 0u);
    ch.names.hud_name = names_node.at("hud_name").get<std::string>();
    ch.names.bio_str_id = names_node.value("bio_str_id", 0u);
    ch.names.bio_byte_offset = names_node.value("bio_byte_offset", 0u);
    ch.names.bio_raw = names_node.value("bio_raw", "");
    ch.names.bio = names_node.value("bio", "");

    // 2. Physical Params
    const auto& phys_node = node.at("physical_params");
    ch.physical_params.grade_or_category = phys_node.value("grade_or_category", 0u);
    ch.physical_params.model_scale_a = phys_node.value("model_scale_a", 1000u);
    ch.physical_params.model_scale_b = phys_node.value("model_scale_b", 525u);
    ch.physical_params.head_size_param = phys_node.value("head_size_param", 20u);
    const int bt_id = phys_node.value("body_type_id", 0);
    ch.physical_params.body_type = core::BodyTypeFromId(bt_id);
    ch.physical_params.body_type_name = phys_node.value(
        "body_type_name",
        std::string(core::BodyTypeToString(ch.physical_params.body_type))
    );
    ch.physical_params.skin_variant = phys_node.value("skin_variant", 0u);
    ch.physical_params.skin_color_rgba_1 = phys_node.value("skin_color_rgba_1", "");
    ch.physical_params.skin_color_rgba_2 = phys_node.value("skin_color_rgba_2", "");
    ch.physical_params.age_or_grade_group = phys_node.value("age_or_grade_group", 0u);
    ch.physical_params.body_build_group = phys_node.value("body_build_group", 0u);

    // 3. Asset Refs
    const auto& asset_node = node.at("asset_refs");
    ch.asset_refs.anim_skeleton_group = asset_node.value("anim_skeleton_group", 0u);
    ch.asset_refs.model_grp_id_1 = asset_node.value("model_grp_id_1", 0u);
    ch.asset_refs.model_grp_id_2 = asset_node.value("model_grp_id_2", 0u);
    ch.asset_refs.face_icon_grp_id = asset_node.value("face_icon_grp_id", 0u);
    ch.asset_refs.bust_up_grp_id_1 = asset_node.value("bust_up_grp_id_1", 0u);
    ch.asset_refs.bust_up_grp_id_2 = asset_node.value("bust_up_grp_id_2", 0u);
    ch.asset_refs.socks_model_code = asset_node.value("socks_model_code", "");
    ch.asset_refs.foot_model_code = asset_node.value("foot_model_code", "");
    ch.asset_refs.voice_table_index = asset_node.value("voice_table_index", 0u);

    // 4. Attributes (20-byte block from idx=9 sec[0])
    const auto& attr_node = node.at("attributes");
    ch.attributes.tp_base = attr_node.at("tp_base").get<std::uint8_t>();
    ch.attributes.tp_max = attr_node.at("tp_max").get<std::uint8_t>();
    ch.attributes.kick_base = attr_node.at("kick_base").get<std::uint8_t>();
    ch.attributes.kick_max = attr_node.at("kick_max").get<std::uint8_t>();
    ch.attributes.catch_base = attr_node.at("catch_base").get<std::uint8_t>();
    ch.attributes.catch_max = attr_node.at("catch_max").get<std::uint8_t>();
    ch.attributes.body_base = attr_node.at("body_base").get<std::uint8_t>();
    ch.attributes.body_max = attr_node.at("body_max").get<std::uint8_t>();
    ch.attributes.control_base = attr_node.at("control_base").get<std::uint8_t>();
    ch.attributes.control_max = attr_node.at("control_max").get<std::uint8_t>();
    ch.attributes.guard_base = attr_node.at("guard_base").get<std::uint8_t>();
    ch.attributes.guard_max = attr_node.at("guard_max").get<std::uint8_t>();
    ch.attributes.speed_base = attr_node.at("speed_base").get<std::uint8_t>();
    ch.attributes.speed_max = attr_node.at("speed_max").get<std::uint8_t>();
    ch.attributes.keshin_stat_bonus = attr_node.value("keshin_stat_bonus", static_cast<std::uint8_t>(0));
    ch.attributes.reserved_0f = attr_node.value("reserved_0f", static_cast<std::uint8_t>(0));
    ch.attributes.inazuma_gauge_rate = attr_node.value("inazuma_gauge_rate", static_cast<std::uint8_t>(100));
    if (attr_node.contains("padding") && attr_node.at("padding").is_array()) {
        ch.attributes.padding = attr_node.at("padding").get<std::array<std::uint8_t, 3>>();
    }

    // 5. Hissatsu Loadout (48-byte block from idx=9 sec[2] + linked techniques)
    const auto& load_node = node.at("hissatsu_loadout");
    ch.hissatsu_loadout.shoot_slots = load_node.at("shoot_slots").get<std::array<std::uint16_t, 5>>();
    ch.hissatsu_loadout.dribble_slots = load_node.at("dribble_slots").get<std::array<std::uint16_t, 4>>();
    ch.hissatsu_loadout.block_slots = load_node.at("block_slots").get<std::array<std::uint16_t, 4>>();
    ch.hissatsu_loadout.catch_slots = load_node.at("catch_slots").get<std::array<std::uint16_t, 4>>();
    ch.hissatsu_loadout.special_slots = load_node.at("special_slots").get<std::array<std::uint16_t, 7>>();

    if (load_node.contains("intrinsic_techniques")) {
        for (const auto& t_node : load_node.at("intrinsic_techniques")) {
            ch.hissatsu_loadout.intrinsic_techniques.push_back(ParseLinkedSlot(t_node));
        }
    }
    if (load_node.contains("team_assigned_techniques")) {
        for (const auto& t_node : load_node.at("team_assigned_techniques")) {
            ch.hissatsu_loadout.team_assigned_techniques.push_back(ParseLinkedSlot(t_node));
        }
    }
    if (load_node.contains("compatible_techniques_from_waza_table")) {
        for (const auto& t_node : load_node.at("compatible_techniques_from_waza_table")) {
            ch.hissatsu_loadout.compatible_techniques_from_waza_table.push_back(ParseLinkedSlot(t_node));
        }
    }

    // 6. Team Appearances
    if (node.contains("team_appearances") && node.at("team_appearances").is_array()) {
        for (const auto& app_node : node.at("team_appearances")) {
            core::TeamAppearance app;
            app.team_file_idx = app_node.value("team_file_idx", static_cast<std::uint16_t>(0));
            app.team_virtual_id = app_node.value("team_virtual_id", "");
            app.team_name = app_node.value("team_name", "");
            app.preset_index = app_node.value("preset_index", static_cast<std::uint8_t>(0));
            app.jersey_number = app_node.value("jersey_number", static_cast<std::uint8_t>(0));
            const int app_pos_id = app_node.value("position_id", 34);
            app.position = core::PositionFromId(app_pos_id);
            app.position_name = app_node.value("position_name", std::string(core::PositionToString(app.position)));
            app.formation_slot = app_node.value("formation_slot", static_cast<std::uint8_t>(0));
            app.stat_block_index = app_node.value("stat_block_index", static_cast<std::uint16_t>(0));
            if (app_node.contains("assigned_hissatsu_ids")) {
                app.assigned_hissatsu_ids =
                    app_node.at("assigned_hissatsu_ids").get<std::vector<std::uint16_t>>();
            }
            ch.team_appearances.push_back(std::move(app));
        }
    }

    return ch;
}

} // namespace

void CharacterCatalog::BuildLookupIndices() {
    char_id_to_index_.clear();
    model_code_to_index_.clear();
    hissatsu_id_to_index_.clear();

    char_id_to_index_.reserve(characters.size());
    model_code_to_index_.reserve(characters.size());
    for (std::size_t i = 0; i < characters.size(); ++i) {
        char_id_to_index_[characters[i].character_id] = i;
        if (!characters[i].model_code.empty() &&
            model_code_to_index_.find(characters[i].model_code) == model_code_to_index_.end()) {
            model_code_to_index_[characters[i].model_code] = i;
        }
    }

    hissatsu_id_to_index_.reserve(hissatsu_techniques.size());
    for (std::size_t i = 0; i < hissatsu_techniques.size(); ++i) {
        hissatsu_id_to_index_[hissatsu_techniques[i].hissatsu_id] = i;
    }
}

const core::Character* CharacterCatalog::FindCharacterById(std::uint16_t character_id) const noexcept {
    auto it = char_id_to_index_.find(character_id);
    if (it == char_id_to_index_.end()) {
        return nullptr;
    }
    return &characters[it->second];
}

const core::Character* CharacterCatalog::FindCharacterByModelCode(std::string_view model_code) const noexcept {
    auto it = model_code_to_index_.find(std::string(model_code));
    if (it == model_code_to_index_.end()) {
        return nullptr;
    }
    return &characters[it->second];
}

const core::HissatsuTechnique* CharacterCatalog::FindHissatsuById(std::uint16_t hissatsu_id) const noexcept {
    auto it = hissatsu_id_to_index_.find(hissatsu_id);
    if (it == hissatsu_id_to_index_.end()) {
        return nullptr;
    }
    return &hissatsu_techniques[it->second];
}

CharacterCatalog CharacterLoader::LoadFromFile(const std::filesystem::path& json_path) {
    std::ifstream ifs(json_path);
    if (!ifs.is_open()) {
        throw std::runtime_error("Não foi possível abrir o catálogo JSON: " + json_path.string());
    }

    nlohmann::json root = nlohmann::json::parse(ifs);
    if (!root.is_object()) {
        throw std::runtime_error("Estrutura JSON inválida em " + json_path.string());
    }

    CharacterCatalog catalog;

    if (root.contains("metadata") && root.at("metadata").is_object()) {
        const auto& meta = root.at("metadata");
        catalog.metadata.game_title = meta.value("game_title", "");
        catalog.metadata.game_id = meta.value("game_id", "");
        catalog.metadata.source_container = meta.value("source_container", "");
        catalog.metadata.compression = meta.value("compression", "");
        catalog.metadata.native_endianness = meta.value("native_endianness", "");
        catalog.metadata.string_encoding = meta.value("string_encoding", "");
        catalog.metadata.total_characters_parsed = meta.value("total_characters_parsed", std::size_t{0});
        catalog.metadata.total_hissatsu_parsed = meta.value("total_hissatsu_parsed", std::size_t{0});
        catalog.metadata.total_master_strings = meta.value("total_master_strings", std::size_t{0});
        catalog.metadata.total_team_roster_tables = meta.value("total_team_roster_tables", std::size_t{0});
    }

    if (!root.contains("hissatsu_techniques") || !root.at("hissatsu_techniques").is_array()) {
        throw std::runtime_error("Campo obrigatório 'hissatsu_techniques' ausente em " + json_path.string());
    }
    const auto& tech_arr = root.at("hissatsu_techniques");
    catalog.hissatsu_techniques.reserve(tech_arr.size());
    for (const auto& t_node : tech_arr) {
        catalog.hissatsu_techniques.push_back(ParseHissatsuTechnique(t_node));
    }

    if (!root.contains("characters") || !root.at("characters").is_array()) {
        throw std::runtime_error("Campo obrigatório 'characters' ausente em " + json_path.string());
    }
    const auto& char_arr = root.at("characters");
    catalog.characters.reserve(char_arr.size());
    for (const auto& c_node : char_arr) {
        catalog.characters.push_back(ParseCharacter(c_node));
    }

    catalog.BuildLookupIndices();
    ValidateCatalogIntegrity(catalog);

    return catalog;
}

void CharacterLoader::ValidateCatalogIntegrity(const CharacterCatalog& catalog) {
    if (catalog.characters.size() != core::kExpectedCharacterCount) {
        std::ostringstream oss;
        oss << "Falha de integridade: esperado " << core::kExpectedCharacterCount
            << " personagens, mas foram carregados " << catalog.characters.size();
        throw std::runtime_error(oss.str());
    }

    if (catalog.hissatsu_techniques.size() != core::kExpectedHissatsuCount) {
        std::ostringstream oss;
        oss << "Falha de integridade: esperado " << core::kExpectedHissatsuCount
            << " técnicas Hissatsu, mas foram carregadas " << catalog.hissatsu_techniques.size();
        throw std::runtime_error(oss.str());
    }

    // Valida cada técnica Hissatsu (wid = 1..476)
    for (std::size_t i = 0; i < catalog.hissatsu_techniques.size(); ++i) {
        const auto& tech = catalog.hissatsu_techniques[i];
        const std::uint16_t expected_wid = static_cast<std::uint16_t>(i + 1);
        if (tech.hissatsu_id != expected_wid) {
            throw std::runtime_error(
                "Hissatsu ID fora de sequência: esperado " + std::to_string(expected_wid) +
                ", encontrado " + std::to_string(tech.hissatsu_id)
            );
        }
        if (tech.category == core::HissatsuCategory::Unknown) {
            throw std::runtime_error(
                "Categoria inválida na técnica Hissatsu ID " + std::to_string(tech.hissatsu_id)
            );
        }
        if (tech.element == core::Element::Unknown) {
            throw std::runtime_error(
                "Elemento inválido na técnica Hissatsu ID " + std::to_string(tech.hissatsu_id)
            );
        }
        if (tech.max_power < tech.base_power) {
            throw std::runtime_error(
                "Poder máximo menor que poder base na técnica Hissatsu ID " + std::to_string(tech.hissatsu_id)
            );
        }
    }

    // Valida cada personagem (cid = 1..411) e suas referências cruzadas de técnicas
    for (std::size_t i = 0; i < catalog.characters.size(); ++i) {
        const auto& ch = catalog.characters[i];
        const std::uint16_t expected_cid = static_cast<std::uint16_t>(i + 1);
        if (ch.character_id != expected_cid) {
            throw std::runtime_error(
                "Character ID fora de sequência: esperado " + std::to_string(expected_cid) +
                ", encontrado " + std::to_string(ch.character_id)
            );
        }
        if (ch.element == core::Element::Unknown) {
            throw std::runtime_error(
                "Elemento inválido no personagem ID " + std::to_string(ch.character_id)
            );
        }
        if (ch.position == core::Position::Unknown) {
            throw std::runtime_error(
                "Posição inválida no personagem ID " + std::to_string(ch.character_id)
            );
        }
        if (!ch.attributes.IsConsistent()) {
            throw std::runtime_error(
                "Atributos inconsistentes (max < base) no personagem ID " + std::to_string(ch.character_id)
            );
        }

        auto check_slot_id = [&](std::uint16_t wid, const char* group_name) {
            if (wid == 0) {
                return;
            }
            if (catalog.FindHissatsuById(wid) == nullptr) {
                throw std::runtime_error(
                    std::string("Referência órfã de Hissatsu ID ") + std::to_string(wid) +
                    " em " + group_name + " do personagem ID " + std::to_string(ch.character_id)
                );
            }
        };

        for (std::uint16_t wid : ch.hissatsu_loadout.shoot_slots)   check_slot_id(wid, "shoot_slots");
        for (std::uint16_t wid : ch.hissatsu_loadout.dribble_slots) check_slot_id(wid, "dribble_slots");
        for (std::uint16_t wid : ch.hissatsu_loadout.block_slots)   check_slot_id(wid, "block_slots");
        for (std::uint16_t wid : ch.hissatsu_loadout.catch_slots)   check_slot_id(wid, "catch_slots");
        for (std::uint16_t wid : ch.hissatsu_loadout.special_slots) check_slot_id(wid, "special_slots");

        for (const auto& linked : ch.hissatsu_loadout.intrinsic_techniques) {
            check_slot_id(linked.hissatsu_id, "intrinsic_techniques");
        }
    }
}

} // namespace strikers::loader
