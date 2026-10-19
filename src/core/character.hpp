#ifndef STRIKERS_CORE_CHARACTER_HPP_
#define STRIKERS_CORE_CHARACTER_HPP_

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace strikers::core {

/// Total oficial de variantes de personagens em `idx = 15` (`0x9C4F`) `sec[0]` (`cid = 1..411`).
inline constexpr std::size_t kExpectedCharacterCount = 411;

/// Total oficial de técnicas Hissatsu em `idx = 79` (`0x9C8F`) / `idx = 81` (`0x9C91`) (`wid = 1..476`).
inline constexpr std::size_t kExpectedHissatsuCount = 476;

/**
 * Afinidade elemental do personagem ou técnica (`+0x0F4` em `idx=15`, `+0x08` em `idx=81`).
 */
enum class Element : std::uint8_t {
    Wind     = 0,  ///< 風 (Kaze)
    Wood     = 1,  ///< 林 (Hayashi)
    Fire     = 2,  ///< 火 (Hi)
    Mountain = 3,  ///< 山 (Yama)
    Void     = 4,  ///< 無 (Mu / Neutro em técnicas de Protocol Omega / El Dorado)
    Special  = 5,  ///< Variante Especial / Keshin
    Unknown  = 255
};

/**
 * Posição tática do personagem (`+0x05C` em `idx=15`, normalizado em `main.dol` @ `0x80009EF4`).
 */
enum class Position : std::uint8_t {
    Goalkeeper = 34,  ///< GK (`0` em disco é normalizado para `34` / `0x22`)
    Defender   = 35,  ///< DF (`0x23`)
    Midfielder = 36,  ///< MF (`0x24`)
    Forward    = 37,  ///< FW (`0x25`)
    Manager    = 38,  ///< Manager / Coach (`0x26`)
    Unknown    = 255
};

/**
 * Tipo físico do esqueleto 3D (`+0x04C` em `idx=15`).
 */
enum class BodyType : std::uint8_t {
    Standard = 0,  ///< Normal
    Small    = 1,  ///< Pequeno / Feminino
    Large    = 2,  ///< Grande / Pesado
    Unknown  = 255
};

/**
 * Categoria de técnica Hissatsu (`+0x00` em `idx=79` `sec[5]`).
 */
enum class HissatsuCategory : std::uint8_t {
    Shoot   = 0,  ///< シュート (Chute)
    Dribble = 1,  ///< ドリブル (Drible)
    Block   = 2,  ///< ブロック (Bloqueio / Defesa)
    Catch   = 3,  ///< キャッチ (Goleiro / Catch)
    Unknown = 255
};

/**
 * Atributo secundário especial de uma técnica Hissatsu (`+0x0A` em `idx=81` `sec[0]`).
 */
enum class HissatsuSpecialAttribute : std::uint8_t {
    None            = 0,
    LongShoot       = 1,  ///< ロングシュート
    ShootBlock      = 2,  ///< シュートブロック
    ChainShoot      = 3,  ///< チェインシュート
    Punching        = 4,  ///< パンチング
    SpecialVariant  = 5,  ///< Variante Especial / Keshin (`+0x0A == 5`)
    Unknown         = 255
};

/**
 * Entidade de domínio para Técnica Especial (Hissatsu), unificando os registros
 * de `idx = 79` (`0x9C8F`, stride 100 bytes) e `idx = 81` (`0x9C91`, stride 140 bytes).
 */
struct HissatsuTechnique {
    std::uint16_t hissatsu_id{0};
    std::uint32_t name_str_id{0};
    std::uint32_t name_byte_offset{0};
    std::string name;
    std::uint32_t internal_name_offset{0};
    std::uint32_t evolved_str_id{0};
    std::string evolved_name;
    std::uint32_t ruby_str_id{0};
    std::string ruby;

    HissatsuCategory category{HissatsuCategory::Unknown};
    std::string category_name;

    Element element{Element::Unknown};
    std::string element_name;

    HissatsuSpecialAttribute special_attribute{HissatsuSpecialAttribute::None};
    std::string special_attribute_name;

    std::uint8_t level_tier{1};          ///< Nível 1..4 (onde 4 = Co-op / Ultimate)
    std::uint16_t base_power{0};         ///< Poder base (`+0x02` em `idx=81`)
    std::uint16_t max_power{0};          ///< Poder evoluído máximo (`+0x04` em `idx=81`)
    std::uint16_t tp_cost{0};            ///< Custo de TP (`+0x06` em `idx=81`)
    std::uint8_t participant_count{1};   ///< Jogadores necessários (`1`, `2` ou `3`)
    std::uint16_t kizuna_bond_required{0}; ///< Amizade mínima para Co-op (`0`, `500`, `700`, `900`)

    std::vector<std::uint16_t> primary_user_char_ids;
    std::vector<std::uint16_t> partner_1_char_ids;
    std::vector<std::uint16_t> partner_2_char_ids;

    [[nodiscard]] constexpr bool IsCoop() const noexcept {
        return participant_count > 1;
    }
};

/**
 * Bloco de atributos Base e Máximos de 20 bytes (`0x14`) em `idx = 9` (`0x9C49`) `sec[0]`,
 * conforme decodificado em `main.dol` @ `0x80026C68`.
 */
struct Attributes {
    std::uint8_t tp_base{0};
    std::uint8_t tp_max{0};
    std::uint8_t kick_base{0};
    std::uint8_t kick_max{0};
    std::uint8_t catch_base{0};
    std::uint8_t catch_max{0};
    std::uint8_t body_base{0};
    std::uint8_t body_max{0};
    std::uint8_t control_base{0};
    std::uint8_t control_max{0};
    std::uint8_t guard_base{0};
    std::uint8_t guard_max{0};
    std::uint8_t speed_base{0};
    std::uint8_t speed_max{0};
    std::uint8_t keshin_stat_bonus{0};
    std::uint8_t reserved_0f{0};
    std::uint8_t inazuma_gauge_rate{100};
    std::array<std::uint8_t, 3> padding{0, 0, 0};

    /**
     * Verifica se todos os atributos máximos são maiores ou iguais aos atributos base.
     */
    [[nodiscard]] constexpr bool IsConsistent() const noexcept {
        return tp_max >= tp_base &&
               kick_max >= kick_base &&
               catch_max >= catch_base &&
               body_max >= body_base &&
               control_max >= control_base &&
               guard_max >= guard_base &&
               speed_max >= speed_base;
    }
};

/**
 * Strings e metadados de localização resolvidos a partir da Master String Table (`idx = 1` / `0x9C41`).
 */
struct CharacterNames {
    std::uint32_t full_name_str_id{0};
    std::uint32_t full_name_byte_offset{0};
    std::string full_name_raw;
    std::string full_name;
    std::string furigana;

    std::uint32_t short_name_str_id{0};
    std::uint32_t short_name_byte_offset{0};
    std::string short_name;

    std::uint32_t hud_name_str_id{0};
    std::uint32_t hud_name_byte_offset{0};
    std::string hud_name;

    std::uint32_t bio_str_id{0};
    std::uint32_t bio_byte_offset{0};
    std::string bio_raw;
    std::string bio;
};

/**
 * Parâmetros físicos e de proporção de modelo 3D em `idx = 15` (`0x9C4F`).
 */
struct CharacterPhysicalParams {
    std::uint32_t grade_or_category{0};
    std::uint32_t model_scale_a{1000};
    std::uint32_t model_scale_b{525};
    std::uint32_t head_size_param{20};
    BodyType body_type{BodyType::Standard};
    std::string body_type_name;
    std::uint32_t skin_variant{0};
    std::string skin_color_rgba_1;
    std::string skin_color_rgba_2;
    std::uint32_t age_or_grade_group{0};
    std::uint32_t body_build_group{0};
};

/**
 * Referências de assets gráficos (`grp.bin`) e de áudio (`idx = 103`) do personagem.
 */
struct CharacterAssetRefs {
    std::uint32_t anim_skeleton_group{0};
    std::uint32_t model_grp_id_1{0};
    std::uint32_t model_grp_id_2{0};
    std::uint32_t face_icon_grp_id{0};
    std::uint32_t bust_up_grp_id_1{0};
    std::uint32_t bust_up_grp_id_2{0};
    std::string socks_model_code;
    std::string foot_model_code;
    std::uint32_t voice_table_index{0};
};

/**
 * Slot resolvido de técnica Hissatsu associada a um personagem.
 */
struct LinkedHissatsuSlot {
    int slot_index{-1};
    std::string slot_type;
    std::uint16_t hissatsu_id{0};
    std::string name;
    std::string category;
    std::string element;
    std::string special_attribute;
    std::uint8_t level_tier{1};
    std::uint16_t base_power{0};
    std::uint16_t max_power{0};
    std::uint16_t tp_cost{0};
    std::uint8_t participant_count{1};
};

/**
 * Loadout de técnicas Hissatsu de 48 bytes (`24x uint16` em `idx=9` `sec[2]`)
 * combinado com as técnicas atribuídas em time (`idx=19..73`) e compatíveis (`idx=81`).
 */
struct CharacterHissatsuLoadout {
    std::array<std::uint16_t, 5> shoot_slots{};
    std::array<std::uint16_t, 4> dribble_slots{};
    std::array<std::uint16_t, 4> block_slots{};
    std::array<std::uint16_t, 4> catch_slots{};
    std::array<std::uint16_t, 7> special_slots{};

    std::vector<LinkedHissatsuSlot> intrinsic_techniques;
    std::vector<LinkedHissatsuSlot> team_assigned_techniques;
    std::vector<LinkedHissatsuSlot> compatible_techniques_from_waza_table;
};

/**
 * Participação do personagem em um elenco de time (`idx = 19..73`, stride `0x44` = 68 bytes).
 */
struct TeamAppearance {
    std::uint16_t team_file_idx{0};
    std::string team_virtual_id;
    std::string team_name;
    std::uint8_t preset_index{0};
    std::uint8_t jersey_number{0};
    Position position{Position::Unknown};
    std::string position_name;
    std::uint8_t formation_slot{0};
    std::uint16_t stat_block_index{0};
    std::vector<std::uint16_t> assigned_hissatsu_ids;
};

/**
 * Entidade principal de Personagem (`idx = 15`, stride `0x148` = 328 bytes),
 * agregando seus atributos (`idx = 9` `sec[0]`), loadout (`idx = 9` `sec[2]`) e elencos.
 */
struct Character {
    std::uint16_t character_id{0};
    std::uint32_t record_byte_offset{0};
    std::uint32_t base_person_id{0};
    std::string model_code;
    std::uint8_t default_jersey_number{0};

    Element element{Element::Unknown};
    std::string element_name;

    Position position{Position::Unknown};
    std::string position_name;

    std::uint32_t team_affiliation_flag_1{0};
    std::uint32_t team_affiliation_flag_2{0};

    CharacterNames names;
    CharacterPhysicalParams physical_params;
    CharacterAssetRefs asset_refs;
    Attributes attributes;
    CharacterHissatsuLoadout hissatsu_loadout;
    std::vector<TeamAppearance> team_appearances;
};

// Funções utilitárias de conversão e formatação de enums do domínio
[[nodiscard]] Element ElementFromId(int id) noexcept;
[[nodiscard]] std::string_view ElementToString(Element elem) noexcept;

[[nodiscard]] Position PositionFromId(int id) noexcept;
[[nodiscard]] std::string_view PositionToString(Position pos) noexcept;

[[nodiscard]] BodyType BodyTypeFromId(int id) noexcept;
[[nodiscard]] std::string_view BodyTypeToString(BodyType bt) noexcept;

[[nodiscard]] HissatsuCategory HissatsuCategoryFromId(int id) noexcept;
[[nodiscard]] std::string_view HissatsuCategoryToString(HissatsuCategory cat) noexcept;

[[nodiscard]] HissatsuSpecialAttribute HissatsuSpecialAttributeFromId(int id) noexcept;
[[nodiscard]] std::string_view HissatsuSpecialAttributeToString(HissatsuSpecialAttribute attr) noexcept;

} // namespace strikers::core

#endif // STRIKERS_CORE_CHARACTER_HPP_
