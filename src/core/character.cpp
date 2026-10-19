#include "core/character.hpp"

namespace strikers::core {

Element ElementFromId(int id) noexcept {
    switch (id) {
        case 0: return Element::Wind;
        case 1: return Element::Wood;
        case 2: return Element::Fire;
        case 3: return Element::Mountain;
        case 4: return Element::Void;
        case 5: return Element::Special;
        default: return Element::Unknown;
    }
}

std::string_view ElementToString(Element elem) noexcept {
    switch (elem) {
        case Element::Wind:     return "Wind (風 / Kaze)";
        case Element::Wood:     return "Wood (林 / Hayashi)";
        case Element::Fire:     return "Fire (火 / Hi)";
        case Element::Mountain: return "Mountain (山 / Yama)";
        case Element::Void:     return "Void (無 / Mu)";
        case Element::Special:  return "Special (5)";
        case Element::Unknown:  return "Unknown";
    }
    return "Unknown";
}

Position PositionFromId(int id) noexcept {
    switch (id) {
        case 0:
        case 34: return Position::Goalkeeper;
        case 35: return Position::Defender;
        case 36: return Position::Midfielder;
        case 37: return Position::Forward;
        case 38: return Position::Manager;
        default: return Position::Unknown;
    }
}

std::string_view PositionToString(Position pos) noexcept {
    switch (pos) {
        case Position::Goalkeeper: return "GK (Goalkeeper)";
        case Position::Defender:   return "DF (Defender)";
        case Position::Midfielder: return "MF (Midfielder)";
        case Position::Forward:    return "FW (Forward)";
        case Position::Manager:    return "Manager / Coach";
        case Position::Unknown:    return "Unknown";
    }
    return "Unknown";
}

BodyType BodyTypeFromId(int id) noexcept {
    switch (id) {
        case 0: return BodyType::Standard;
        case 1: return BodyType::Small;
        case 2: return BodyType::Large;
        default: return BodyType::Unknown;
    }
}

std::string_view BodyTypeToString(BodyType bt) noexcept {
    switch (bt) {
        case BodyType::Standard: return "Standard (Normal)";
        case BodyType::Small:    return "Small / Female";
        case BodyType::Large:    return "Large / Heavy";
        case BodyType::Unknown:  return "Unknown";
    }
    return "Unknown";
}

HissatsuCategory HissatsuCategoryFromId(int id) noexcept {
    switch (id) {
        case 0: return HissatsuCategory::Shoot;
        case 1: return HissatsuCategory::Dribble;
        case 2: return HissatsuCategory::Block;
        case 3: return HissatsuCategory::Catch;
        default: return HissatsuCategory::Unknown;
    }
}

std::string_view HissatsuCategoryToString(HissatsuCategory cat) noexcept {
    switch (cat) {
        case HissatsuCategory::Shoot:   return "Shoot (シュート)";
        case HissatsuCategory::Dribble: return "Dribble (ドリブル)";
        case HissatsuCategory::Block:   return "Block / Defense (ブロック)";
        case HissatsuCategory::Catch:   return "Catch / GK (キャッチ)";
        case HissatsuCategory::Unknown: return "Unknown";
    }
    return "Unknown";
}

HissatsuSpecialAttribute HissatsuSpecialAttributeFromId(int id) noexcept {
    switch (id) {
        case 0: return HissatsuSpecialAttribute::None;
        case 1: return HissatsuSpecialAttribute::LongShoot;
        case 2: return HissatsuSpecialAttribute::ShootBlock;
        case 3: return HissatsuSpecialAttribute::ChainShoot;
        case 4: return HissatsuSpecialAttribute::Punching;
        case 5: return HissatsuSpecialAttribute::SpecialVariant;
        default: return HissatsuSpecialAttribute::Unknown;
    }
}

std::string_view HissatsuSpecialAttributeToString(HissatsuSpecialAttribute attr) noexcept {
    switch (attr) {
        case HissatsuSpecialAttribute::None:           return "None";
        case HissatsuSpecialAttribute::LongShoot:      return "Long Shoot (ロング)";
        case HissatsuSpecialAttribute::ShootBlock:     return "Shoot Block (シュートブロック)";
        case HissatsuSpecialAttribute::ChainShoot:     return "Chain Shoot (チェイン)";
        case HissatsuSpecialAttribute::Punching:       return "Catch / Punching (パンチング)";
        case HissatsuSpecialAttribute::SpecialVariant: return "Special Variant (5)";
        case HissatsuSpecialAttribute::Unknown:        return "Unknown";
    }
    return "Unknown";
}

} // namespace strikers::core
