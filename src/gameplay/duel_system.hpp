#ifndef STRIKERS_GAMEPLAY_DUEL_SYSTEM_HPP_
#define STRIKERS_GAMEPLAY_DUEL_SYSTEM_HPP_

#include "core/character.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

namespace strikers::gameplay {

/**
 * Representação de elementos para combate conforme specs/BATTLE_FORMULAS.md.
 */
enum class BattleElement : std::uint8_t {
    Wind     = 0, // 風 (Vento)
    Wood     = 1, // 林 (Floresta)
    Fire     = 2, // 火 (Fogo)
    Mountain = 3, // 山 (Terra/Montanha)
    Neutral  = 4, // 無 (Neutro / Sem Elemento)
    Inherit  = 5  // Herda o elemento nativo do atleta
};

/**
 * Índices dos 6 atributos primários na memória de partida (0xA19C..0xA1B0).
 */
enum class StatIndex : std::int32_t {
    Kick    = 0, // +0x00 (0xA19C) - キック
    Catch   = 1, // +0x04 (0xA1A0) - キャッチ
    Body    = 2, // +0x08 (0xA1A4) - ボディ
    Guard   = 3, // +0x0C (0xA1A8) - ガード
    Control = 4, // +0x10 (0xA1AC) - コントロール
    Speed   = 5  // +0x14 (0xA1B0) - スピード
};

/**
 * Parâmetros de técnica Hissatsu preparados para resolução de combate.
 */
struct WazaBattleParam {
    std::uint16_t hissatsu_id{0};
    std::string   name;
    std::int16_t  tp_cost{0};
    std::int16_t  base_power{0};
    std::int16_t  max_power{0};
    BattleElement element{BattleElement::Neutral};
    std::uint16_t special_attribute_id{0}; // 1=LongShoot, 2=ShootBlock, 3=ChainShoot, 4=Punching
    std::uint16_t level_tier_raw{1};       // 1=Lv1, 2=Lv2, 3=Lv3, 4=Ultimate
    std::uint8_t  flags_0x86{0};

    [[nodiscard]] static WazaBattleParam FromHissatsu(const core::HissatsuTechnique& tech) noexcept;
};

/**
 * Snapshot dos atributos e bônus de um atleta em um duelo.
 */
struct PlayerBattleView {
    std::string   name;
    std::uint16_t character_id{0};
    std::int32_t  kick{50};
    std::int32_t  catch_stat{50};
    std::int32_t  body{50};
    std::int32_t  guard{50};
    std::int32_t  control{50};
    std::int32_t  speed{50};
    std::int32_t  aggression_foul_rate{10};
    BattleElement element{BattleElement::Wind};
    std::uint8_t  waza_evolution_tier{0}; // 0..3 (3 = evolução máxima / max_power)
    std::uint8_t  passive_flags{0};       // roster +0x79
    std::int32_t  team_keyman_buff{-1};   // -1=None, 0=Speed, 1=Kick/Control, 2=Catch/Body/Guard
    bool          team_burning_active{false};
    bool          keshin_active{false};

    [[nodiscard]] static PlayerBattleView FromCharacter(
        const core::Character& ch,
        bool use_max_attributes = false
    ) noexcept;
};

/**
 * Gerador de números pseudo-aleatórios determinístico GFSR(521, 32)
 * extraído diretamente de main.dol (FUN_80330d14 @ 0x80330D14).
 */
class Gfsr521Rng {
public:
    Gfsr521Rng() noexcept;
    explicit Gfsr521Rng(std::uint32_t seed) noexcept;

    void SetSeed(std::uint32_t seed) noexcept;
    std::int32_t NextInt(std::int32_t a, std::int32_t b) noexcept;

private:
    std::array<std::uint32_t, 521> state_{};
    std::int32_t idx_{0};
};

/**
 * Resultado da disputa de finalização (Chute vs. Defesa de Goleiro ou Bloqueio).
 */
struct ShotDefenseResult {
    std::int32_t shoot_power{0};
    std::int32_t defense_power{0};
    std::int32_t remaining_shoot_power{0};
    std::int32_t elem_waza_bonus{0};
    std::int32_t elem_player_bonus{0};
    std::int32_t rng_variance{0};
    bool         is_goal{false};
    bool         is_punching{false}; // Se espalmada em vez de encaixe
    std::string  summary;
};

/**
 * Resultado do duelo terrestre 1v1 (Drible vs. Bote/Bloqueio).
 */
struct GroundDuelResult {
    std::int32_t attacker_power{0};
    std::int32_t defender_power{0};
    std::int32_t score_diff{0};
    std::int32_t success_probability{0};
    bool         attacker_won{false};
    bool         is_draw{false};
    std::string  summary;
};

/**
 * Subsistema matemático de duelos e confrontos de Inazuma Eleven GO Strikers 2013.
 * Implementa exatamente as rotinas descompiladas de main.dol (specs/BATTLE_FORMULAS.md).
 */
class DuelSystem {
public:
    explicit DuelSystem(std::uint32_t rng_seed = 20130920u) noexcept;

    /**
     * Ciclo elemental: Floresta (1) > Vento (0) > Terra (3) > Fogo (2) > Floresta (1).
     * Retorna +1 (vantagem A), -1 (desvantagem A), 0 (neutro/mesmo elemento).
     */
    [[nodiscard]] static constexpr std::int32_t CompareElements(BattleElement a, BattleElement b) noexcept {
        if (a == BattleElement::Wind)     return (b == BattleElement::Mountain) ? +1 : ((b == BattleElement::Wood)     ? -1 : 0);
        if (a == BattleElement::Wood)     return (b == BattleElement::Wind)     ? +1 : ((b == BattleElement::Fire)     ? -1 : 0);
        if (a == BattleElement::Fire)     return (b == BattleElement::Wood)     ? +1 : ((b == BattleElement::Mountain) ? -1 : 0);
        if (a == BattleElement::Mountain) return (b == BattleElement::Fire)     ? +1 : ((b == BattleElement::Wind)     ? -1 : 0);
        return 0;
    }

    [[nodiscard]] static constexpr BattleElement ResolveWazaElement(
        const WazaBattleParam* waza,
        BattleElement user_elem
    ) noexcept {
        if (!waza) return BattleElement::Neutral;
        return (waza->element == BattleElement::Inherit) ? user_elem : waza->element;
    }

    [[nodiscard]] static constexpr std::int32_t GetEffectiveStat(
        const PlayerBattleView& p,
        StatIndex idx
    ) noexcept {
        std::int32_t base = 0;
        switch (idx) {
            case StatIndex::Kick:    base = p.kick; break;
            case StatIndex::Catch:   base = p.catch_stat; break;
            case StatIndex::Body:    base = p.body; break;
            case StatIndex::Guard:   base = p.guard; break;
            case StatIndex::Control: base = p.control; break;
            case StatIndex::Speed:   base = p.speed; break;
        }

        std::int32_t bonus = 0;
        if ((idx == StatIndex::Kick || idx == StatIndex::Control) && p.team_keyman_buff == 1) {
            bonus = 20;
        } else if ((idx == StatIndex::Catch || idx == StatIndex::Body || idx == StatIndex::Guard) && p.team_keyman_buff == 2) {
            bonus = 20;
        } else if (idx == StatIndex::Speed && p.team_keyman_buff == 0) {
            bonus = 20;
        }
        return base + bonus;
    }

    [[nodiscard]] static constexpr std::int32_t GetEffectiveWazaPower(
        const PlayerBattleView& p,
        const WazaBattleParam* waza
    ) noexcept {
        if (!waza) return 0;
        if (p.waza_evolution_tier >= 3 && waza->max_power > 0) {
            return waza->max_power;
        }
        return waza->base_power;
    }

    /**
     * Sorteio de variância aleatória em dois estágios (FUN_8009f3e0 @ 0x8009F3E0).
     */
    std::int32_t RollBattleVariance(const PlayerBattleView& p, bool close_range) noexcept;

    /**
     * Cálculo do Poder de Chute (FUN_8009e36c @ 0x8009E36C).
     */
    std::int32_t ComputeShootPower(
        const PlayerBattleView& shooter,
        const WazaBattleParam* shoot_waza,
        float charge_time_sec,
        float dist_to_goal_m,
        float dist_to_gk_m,
        std::int32_t chain_count = 0,
        std::int32_t prev_shoot_power = 0
    ) noexcept;

    /**
     * Resolução unificada de Defesa de Chute (FUN_8009eac8 / FUN_8009ec70 / FUN_8009ee18).
     * Suporta Shoot Counter (StatIndex::Kick), Shoot Block (StatIndex::Guard) e Goleiro (StatIndex::Catch).
     */
    ShotDefenseResult ResolveShotDefense(
        const PlayerBattleView& shooter,
        const WazaBattleParam* shoot_waza,
        std::int32_t shoot_power,
        const PlayerBattleView& defender,
        const WazaBattleParam* def_waza,
        float def_charge_sec,
        StatIndex def_stat_idx,
        bool close_range_flag
    ) noexcept;

    /**
     * Resolução da Finalização Chute vs. Goleiro (FUN_8009ee18 @ 0x8009EE18).
     */
    ShotDefenseResult ResolveShotVsGoalkeeper(
        const PlayerBattleView& shooter,
        const WazaBattleParam* shoot_waza,
        std::int32_t shoot_power,
        const PlayerBattleView& goalkeeper,
        const WazaBattleParam* gk_waza,
        float gk_charge_sec,
        bool close_range_flag
    ) noexcept;

    /**
     * Duelo terrestre 1v1 normal: Drible vs. Desarme/Bote (FUN_80075754 @ 0x80075754).
     */
    GroundDuelResult ResolveNormalTackle(
        const PlayerBattleView& attacker,
        const PlayerBattleView& defender
    ) noexcept;

    /**
     * Duelo 1v1 de Hissatsu Command Battle (FUN_8009f078 @ 0x8009F078).
     * Retorna: +1 = Atacante vence, -1 = Defensor vence, 0 = Empate.
     */
    [[nodiscard]] static std::int32_t ResolveHissatsuCommandBattle(
        const PlayerBattleView& attacker,
        const WazaBattleParam* dribble_waza,
        const PlayerBattleView& defender,
        const WazaBattleParam* block_waza
    ) noexcept;

    /**
     * Teste de falta no carrinho por setor angular (FUN_8007590C @ 0x8007590C).
     */
    bool CheckSlidingTackleFoul(
        const PlayerBattleView& tackler,
        float angle_diff_rad,
        bool fouls_enabled = true
    ) noexcept;

    Gfsr521Rng& GetRng() noexcept { return rng_; }

private:
    Gfsr521Rng rng_;
};

} // namespace strikers::gameplay

#endif // STRIKERS_GAMEPLAY_DUEL_SYSTEM_HPP_
