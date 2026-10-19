#ifndef STRIKERS_LOADER_CHARACTER_LOADER_HPP_
#define STRIKERS_LOADER_CHARACTER_LOADER_HPP_

#include "core/character.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace strikers::loader {

/**
 * Metadados do arquivo `output/data/characters_parsed.json`.
 */
struct DatasetMetadata {
    std::string game_title;
    std::string game_id;
    std::string source_container;
    std::string compression;
    std::string native_endianness;
    std::string string_encoding;
    std::size_t total_characters_parsed{0};
    std::size_t total_hissatsu_parsed{0};
    std::size_t total_master_strings{0};
    std::size_t total_team_roster_tables{0};
};

/**
 * Catálogo imutável em memória contendo os 411 personagens e 476 técnicas Hissatsu
 * de Inazuma Eleven GO Strikers 2013, indexados por ID e código de modelo.
 */
class CharacterCatalog {
public:
    DatasetMetadata metadata{};
    std::vector<core::Character> characters;
    std::vector<core::HissatsuTechnique> hissatsu_techniques;

    void BuildLookupIndices();

    [[nodiscard]] const core::Character* FindCharacterById(std::uint16_t character_id) const noexcept;
    [[nodiscard]] const core::Character* FindCharacterByModelCode(std::string_view model_code) const noexcept;
    [[nodiscard]] const core::HissatsuTechnique* FindHissatsuById(std::uint16_t hissatsu_id) const noexcept;

private:
    std::unordered_map<std::uint16_t, std::size_t> char_id_to_index_;
    std::unordered_map<std::string, std::size_t> model_code_to_index_;
    std::unordered_map<std::uint16_t, std::size_t> hissatsu_id_to_index_;
};

/**
 * Parser de ingestão e validador de integridade para `output/data/characters_parsed.json`.
 */
class CharacterLoader {
public:
    /**
     * Lê o JSON estruturado, instancia as entidades de domínio (`Character` e `HissatsuTechnique`)
     * e executa verificações estritas de integridade referencial e limites numéricos.
     */
    [[nodiscard]] static CharacterCatalog LoadFromFile(const std::filesystem::path& json_path);

    /**
     * Executa a bateria de verificações de integridade sobre um catálogo carregado.
     * Lança `std::runtime_error` caso qualquer invariante de `specs/CHARACTERS_SCHEMA.md` seja violada.
     */
    static void ValidateCatalogIntegrity(const CharacterCatalog& catalog);
};

} // namespace strikers::loader

#endif // STRIKERS_LOADER_CHARACTER_LOADER_HPP_
