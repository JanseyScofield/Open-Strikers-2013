#include "gameplay/duel_system.hpp"

#include <cmath>
#include <sstream>

namespace strikers::gameplay {

namespace {

constexpr std::int32_t StepThreshold20(std::int32_t diff) noexcept {
    if (diff >= 21)  return +2;
    if (diff >= 1)   return +1;
    if (diff == 0)   return  0;
    if (diff >= -19) return -1;
    return -2;
}

BattleElement ToBattleElement(core::Element elem) noexcept {
    switch (elem) {
        case core::Element::Wind:     return BattleElement::Wind;
        case core::Element::Wood:     return BattleElement::Wood;
        case core::Element::Fire:     return BattleElement::Fire;
        case core::Element::Mountain: return BattleElement::Mountain;
        case core::Element::Void:     return BattleElement::Neutral;
        case core::Element::Special:  return BattleElement::Inherit;
        case core::Element::Unknown:  return BattleElement::Neutral;
    }
    return BattleElement::Neutral;
}

} // namespace

WazaBattleParam WazaBattleParam::FromHissatsu(const core::HissatsuTechnique& tech) noexcept {
    WazaBattleParam w{};
    w.hissatsu_id = tech.hissatsu_id;
    w.name = tech.name;
    w.tp_cost = static_cast<std::int16_t>(tech.tp_cost);
    w.base_power = static_cast<std::int16_t>(tech.base_power);
    w.max_power = static_cast<std::int16_t>(tech.max_power);
    w.element = ToBattleElement(tech.element);
    w.special_attribute_id = static_cast<std::uint16_t>(tech.special_attribute);
    // Na tabela de combate (specs/BATTLE_FORMULAS.md Seção 5.1), o tier varia de 1..3 (Lv1..Lv3).
    w.level_tier_raw = static_cast<std::uint16_t>(std::clamp<int>(tech.level_tier, 1, 3));
    w.flags_0x86 = 0;
    return w;
}

PlayerBattleView PlayerBattleView::FromCharacter(const core::Character& ch, bool use_max_attributes) noexcept {
    PlayerBattleView p{};
    p.name = ch.names.full_name.empty() ? ch.names.hud_name : ch.names.full_name;
    p.character_id = ch.character_id;

    if (use_max_attributes) {
        p.kick       = ch.attributes.kick_max;
        p.catch_stat = ch.attributes.catch_max;
        p.body       = ch.attributes.body_max;
        p.guard      = ch.attributes.guard_max;
        p.control    = ch.attributes.control_max;
        p.speed      = ch.attributes.speed_max;
    } else {
        p.kick       = ch.attributes.kick_base;
        p.catch_stat = ch.attributes.catch_base;
        p.body       = ch.attributes.body_base;
        p.guard      = ch.attributes.guard_base;
        p.control    = ch.attributes.control_base;
        p.speed      = ch.attributes.speed_base;
    }

    p.aggression_foul_rate = 12;
    p.element = ToBattleElement(ch.element);
    p.waza_evolution_tier = 0;
    p.passive_flags = 0;
    p.team_keyman_buff = -1;
    p.team_burning_active = false;
    p.keshin_active = (ch.attributes.keshin_stat_bonus > 0);
    return p;
}

// ============================================================================
// GFSR(521, 32) PRNG Implementation
// ============================================================================

Gfsr521Rng::Gfsr521Rng() noexcept {
    SetSeed(20130920u);
}

Gfsr521Rng::Gfsr521Rng(std::uint32_t seed) noexcept {
    SetSeed(seed);
}

void Gfsr521Rng::SetSeed(std::uint32_t seed) noexcept {
    // Algoritmo LCG padrão (Knuth / Numerical Recipes) para aquecimento do buffer GFSR
    std::uint32_t s = (seed != 0u) ? seed : 19980718u;
    for (std::size_t i = 0; i < state_.size(); ++i) {
        s = 1812433253u * (s ^ (s >> 30u)) + static_cast<std::uint32_t>(i);
        state_[i] = s;
    }
    idx_ = 0;
}

std::int32_t Gfsr521Rng::NextInt(std::int32_t a, std::int32_t b) noexcept {
    const std::int32_t lo = std::min(a, b);
    const std::int32_t hi = std::max(a, b);
    idx_ = (idx_ + 1) % 521;
    const std::int32_t j = (idx_ - 32 + 521) % 521;
    state_[static_cast<std::size_t>(idx_)] ^= state_[static_cast<std::size_t>(j)];

    const std::uint32_t span = static_cast<std::uint32_t>(hi - lo + 1);
    return span ? (lo + static_cast<std::int32_t>(state_[static_cast<std::size_t>(idx_)] % span)) : lo;
}

// ============================================================================
// DuelSystem Core Implementation
// ============================================================================

DuelSystem::DuelSystem(std::uint32_t rng_seed) noexcept
    : rng_(rng_seed) {}

std::int32_t DuelSystem::RollBattleVariance(const PlayerBattleView& p, bool close_range) noexcept {
    std::int32_t min_roll = 0;
    if ((p.passive_flags & 0x10) != 0) {
        min_roll = rng_.NextInt(0, 10);
    } else if ((p.passive_flags & 0x20) != 0) {
        min_roll = 0;
    } else {
        min_roll = rng_.NextInt(0, 5);
    }
    const std::int32_t max_roll = (!close_range && p.keshin_active) ? 19 : 10;
    return rng_.NextInt(min_roll, max_roll);
}

std::int32_t DuelSystem::ComputeShootPower(
    const PlayerBattleView& shooter,
    const WazaBattleParam* shoot_waza,
    float charge_time_sec,
    float dist_to_goal_m,
    float dist_to_gk_m,
    std::int32_t chain_count,
    std::int32_t prev_shoot_power
) noexcept {
    const std::int32_t waza_power = GetEffectiveWazaPower(shooter, shoot_waza);
    const std::int32_t kick_half  = GetEffectiveStat(shooter, StatIndex::Kick) / 2;

    std::int32_t charge_or_chain_bonus = 0;
    if (chain_count == 0) {
        const bool disable_charge = shoot_waza && ((shoot_waza->flags_0x86 & 0x03) != 0);
        if (!disable_charge && charge_time_sec > 0.0f) {
            charge_or_chain_bonus = (charge_time_sec < 0.9f)
                ? static_cast<std::int32_t>((charge_time_sec / 0.9f) * 20.0f)
                : 20;
        }
    } else {
        const std::int32_t clamped_prev = std::max(0, prev_shoot_power);
        charge_or_chain_bonus = (clamped_prev * 25) / 100 + (10 * chain_count);
    }

    const std::int32_t burning_bonus = shooter.team_burning_active ? 30 : 0;
    const bool close_range = (dist_to_gk_m <= 25.0f);
    const std::int32_t rng_bonus = RollBattleVariance(shooter, close_range);

    // Decaimento por distância ao centro do gol (dat.bin idx=7 sec[0] +0x212C..+0x2138)
    const float clamped_dist = std::min(200.0f, std::max(0.0f, dist_to_goal_m));
    const bool is_long_shoot = shoot_waza && (shoot_waza->special_attribute_id == 1);
    const float min_dist = is_long_shoot ? 50.0f : 25.0f;
    const float max_pen  = is_long_shoot ? 100.0f : 150.0f;

    std::int32_t dist_penalty = 0;
    if (clamped_dist > min_dist) {
        dist_penalty = static_cast<std::int32_t>(((clamped_dist - min_dist) / (200.0f - min_dist)) * max_pen);
    }

    const std::int32_t total = waza_power + kick_half + charge_or_chain_bonus + burning_bonus + rng_bonus - dist_penalty;
    return std::max(1, total);
}

ShotDefenseResult DuelSystem::ResolveShotDefense(
    const PlayerBattleView& shooter,
    const WazaBattleParam* shoot_waza,
    std::int32_t shoot_power,
    const PlayerBattleView& defender,
    const WazaBattleParam* def_waza,
    float def_charge_sec,
    StatIndex def_stat_idx,
    bool close_range_flag
) noexcept {
    std::int32_t base_power = 0;
    if (def_waza != nullptr) {
        base_power = GetEffectiveWazaPower(defender, def_waza) + (defender.team_burning_active ? 30 : 0);
    } else if (def_charge_sec > 0.0f) {
        base_power = (def_charge_sec < 0.9f)
            ? static_cast<std::int32_t>((def_charge_sec / 0.9f) * 50.0f)
            : 50;
    }

    const std::int32_t stat_half = GetEffectiveStat(defender, def_stat_idx) / 2;

    const BattleElement shoot_waza_elem = ResolveWazaElement(shoot_waza, shooter.element);
    const BattleElement def_waza_elem   = ResolveWazaElement(def_waza, defender.element);

    const std::int32_t elem_waza_bonus   = 10 * CompareElements(def_waza_elem, shoot_waza_elem);
    const std::int32_t elem_player_bonus = 10 * CompareElements(defender.element, shooter.element);
    const std::int32_t rng_variance      = RollBattleVariance(defender, close_range_flag);

    const std::int32_t defense_power = base_power + stat_half + elem_waza_bonus + elem_player_bonus + rng_variance;

    ShotDefenseResult res{};
    res.shoot_power = shoot_power;
    res.defense_power = defense_power;
    res.elem_waza_bonus = elem_waza_bonus;
    res.elem_player_bonus = elem_player_bonus;
    res.rng_variance = rng_variance;
    res.is_punching = (def_waza && def_waza->special_attribute_id == 4);

    std::ostringstream ss;
    if (defense_power >= shoot_power) {
        // Defesa bem-sucedida (empate favorece o defensor/goleiro)
        res.is_goal = false;
        res.remaining_shoot_power = 0;
        ss << "[DEFESA!] " << defender.name << " parou o chute! (P_def=" << defense_power
           << " vs P_chute=" << shoot_power << ")";
        if (res.is_punching) {
            ss << " [Espalmada / Punching]";
        }
    } else {
        // O chute venceu a defesa!
        res.is_goal = true;
        res.remaining_shoot_power = std::max(0, shoot_power - (defense_power / 3));
        ss << "[GOL!] Chute de " << shooter.name << " superou " << defender.name
           << "! (P_chute=" << shoot_power << " vs P_def=" << defense_power
           << " | Residual=" << res.remaining_shoot_power << ")";
    }
    res.summary = ss.str();
    return res;
}

ShotDefenseResult DuelSystem::ResolveShotVsGoalkeeper(
    const PlayerBattleView& shooter,
    const WazaBattleParam* shoot_waza,
    std::int32_t shoot_power,
    const PlayerBattleView& goalkeeper,
    const WazaBattleParam* gk_waza,
    float gk_charge_sec,
    bool close_range_flag
) noexcept {
    return ResolveShotDefense(
        shooter,
        shoot_waza,
        shoot_power,
        goalkeeper,
        gk_waza,
        gk_charge_sec,
        StatIndex::Catch,
        close_range_flag
    );
}

GroundDuelResult DuelSystem::ResolveNormalTackle(
    const PlayerBattleView& attacker,
    const PlayerBattleView& defender
) noexcept {
    std::int32_t p_drible   = GetEffectiveStat(attacker, StatIndex::Body)  + (GetEffectiveStat(attacker, StatIndex::Speed) / 2);
    std::int32_t p_bloqueio = GetEffectiveStat(defender, StatIndex::Guard) + (GetEffectiveStat(defender, StatIndex::Speed) / 2);

    const std::int32_t elem_cmp = CompareElements(attacker.element, defender.element);
    if (elem_cmp == +1) {
        p_drible += 20;
    } else if (elem_cmp == -1) {
        p_bloqueio += 20;
    }

    const std::int32_t diff = p_drible - p_bloqueio;
    std::int32_t win_threshold = 50;
    if      (diff >= 56) win_threshold = 80;
    else if (diff >= 46) win_threshold = 75;
    else if (diff >= 36) win_threshold = 70;
    else if (diff >= 26) win_threshold = 65;
    else if (diff >= 16) win_threshold = 60;
    else if (diff >= 6)  win_threshold = 55;
    else if (diff >= -5) win_threshold = 50;
    else                 win_threshold = 45;

    const std::int32_t roll = rng_.NextInt(0, 100);
    const bool attacker_won = (roll < win_threshold);

    GroundDuelResult res{};
    res.attacker_power = p_drible;
    res.defender_power = p_bloqueio;
    res.score_diff = diff;
    res.success_probability = win_threshold;
    res.attacker_won = attacker_won;
    res.is_draw = false;

    std::ostringstream ss;
    if (attacker_won) {
        ss << "[DRIBLE VENCEDOR] " << attacker.name << " manteve a posse contra "
           << defender.name << " (Drible=" << p_drible << " vs Bote=" << p_bloqueio
           << ", Chance=" << win_threshold << "%, RNG=" << roll << ")";
    } else {
        ss << "[DESARME LIMPO] " << defender.name << " desarmou "
           << attacker.name << " (Bote=" << p_bloqueio << " vs Drible=" << p_drible
           << ", Chance=" << win_threshold << "%, RNG=" << roll << ")";
    }
    res.summary = ss.str();
    return res;
}

std::int32_t DuelSystem::ResolveHissatsuCommandBattle(
    const PlayerBattleView& attacker,
    const WazaBattleParam* dribble_waza,
    const PlayerBattleView& defender,
    const WazaBattleParam* block_waza
) noexcept {
    if (!dribble_waza && !block_waza) return 0;
    if (dribble_waza && !block_waza)  return 1;
    if (!dribble_waza && block_waza)  return -1;

    const BattleElement atk_waza_elem = ResolveWazaElement(dribble_waza, attacker.element);
    const BattleElement def_waza_elem = ResolveWazaElement(block_waza, defender.element);

    const std::int32_t score_waza_elem = CompareElements(atk_waza_elem, def_waza_elem);
    const std::int32_t score_char_elem = CompareElements(attacker.element, defender.element);
    const std::int32_t score_level     = static_cast<std::int32_t>(dribble_waza->level_tier_raw) - static_cast<std::int32_t>(block_waza->level_tier_raw);
    const std::int32_t score_power     = StepThreshold20(GetEffectiveWazaPower(attacker, dribble_waza) - GetEffectiveWazaPower(defender, block_waza));
    const std::int32_t score_stat      = StepThreshold20(GetEffectiveStat(attacker, StatIndex::Body) - GetEffectiveStat(defender, StatIndex::Guard));

    const std::int32_t total_score = score_waza_elem + score_char_elem + score_level + score_power + score_stat;
    if (total_score >= 1)  return 1;  // Atacante vence
    if (total_score <= -1) return -1; // Defensor vence
    return 0; // Empate
}

bool DuelSystem::CheckSlidingTackleFoul(
    const PlayerBattleView& tackler,
    float angle_diff_rad,
    bool fouls_enabled
) noexcept {
    if (!fouls_enabled) return false;
    std::int32_t foul_prob = 0;
    if (angle_diff_rad > 2.3561945f) { // > 135 graus (por trás)
        foul_prob = tackler.aggression_foul_rate + 15;
    } else if (angle_diff_rad > 1.5707964f) { // > 90 graus (lateral)
        foul_prob = (tackler.aggression_foul_rate / 2) + 7;
    } else { // <= 90 graus (frontal: desarme 100% limpo)
        foul_prob = 0;
    }
    return rng_.NextInt(0, 99) < foul_prob;
}

} // namespace strikers::gameplay
