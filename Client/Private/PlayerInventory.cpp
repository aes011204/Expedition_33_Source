#include "PlayerInventory.h"

void PlayerInventory::Clear()
{
    _chroma = 0;
    _unlockedPictosIds.clear();
    _ownedWeapons.clear();
}

bool PlayerInventory::Is_PictosUnlocked(const string& pictosId) const
{
    if (pictosId.empty())
        return false;

    return find(_unlockedPictosIds.begin(), _unlockedPictosIds.end(), pictosId) != _unlockedPictosIds.end();
}

void PlayerInventory::Unlock_Pictos(const string& pictosId)
{
    if (pictosId.empty())
        return;

    if (Is_PictosUnlocked(pictosId))
        return; // 이미 해금된 id는 중복 저장하지 않는다.

    _unlockedPictosIds.push_back(pictosId);
}

void PlayerInventory::Ensure_DefaultUnlockedPictos(const vector<string>& pictosIds)
{
    // 나중에는, 몬스터 잡거나 그러면서 픽토스 해금되도록 하기
    for (const string& pictosId : pictosIds)
    {
        Unlock_Pictos(pictosId);
    }
}

bool PlayerInventory::Is_WeaponUnlocked(const string& characterId, const string& weaponId) const
{
    if (characterId.empty() || weaponId.empty())
        return false;

    return find_if(_ownedWeapons.begin(), _ownedWeapons.end(),
        [&characterId, &weaponId](const WeaponInventoryEntry& entry)
        {
            return entry.characterId == characterId && entry.weaponId == weaponId;
        }) != _ownedWeapons.end();
}

void PlayerInventory::Unlock_Weapon(const string& characterId, const string& weaponId, uint32 enhanceLevel, uint32 enhanceExp)
{
    if (characterId.empty() || weaponId.empty())
        return;

    if (Is_WeaponUnlocked(characterId, weaponId))
        return; // 같은 캐릭터의 같은 무기는 중복 저장하지 않는다.

    WeaponInventoryEntry entry{};
    entry.characterId = characterId;
    entry.weaponId = weaponId;
    entry.enhanceLevel = enhanceLevel == 0 ? 1 : enhanceLevel;
    entry.enhanceExp = enhanceExp;

    _ownedWeapons.push_back(entry);
}

void PlayerInventory::Ensure_DefaultUnlockedWeapons(const vector<WeaponInventoryEntry>& weapons)
{
    for (const WeaponInventoryEntry& weapon : weapons)
    {
        if (weapon.characterId.empty() || weapon.weaponId.empty())
            continue;

        if (Is_WeaponUnlocked(weapon.characterId, weapon.weaponId))
            continue;

        WeaponInventoryEntry entry = weapon;
        if (entry.enhanceLevel == 0)
            entry.enhanceLevel = 1;

        _ownedWeapons.emplace_back(std::move(entry));
    }
}

json PlayerInventory::Create_SaveJson() const
{
    json root = json::object();
    root["chroma"] = _chroma;
    root["unlocked_pictos"] = _unlockedPictosIds;

    root["owned_weapons"] = json::array();
    for (const WeaponInventoryEntry& weapon : _ownedWeapons)
    {
        json entry = json::object();
        entry["character_id"] = weapon.characterId;
        entry["weapon_id"] = weapon.weaponId;
        entry["enhance_level"] = weapon.enhanceLevel;
        entry["enhance_exp"] = weapon.enhanceExp;
        root["owned_weapons"].emplace_back(entry);
    }

    return root;
}

bool PlayerInventory::Apply_SaveJson(const json& data)
{
    if (!data.is_object())
        return false;

    Clear();
    _chroma = data.value("chroma", 0u);

    if (auto pictosIds = data.find("unlocked_pictos"); pictosIds != data.end() && pictosIds->is_array())
    {
        for (const json& pictosId : *pictosIds)
        {
            if (pictosId.is_string())
                Unlock_Pictos(pictosId.get<string>());
        }
    }

    if (auto weapons = data.find("owned_weapons"); weapons != data.end() && weapons->is_array())
    {
        for (const json& weapon : *weapons)
        {
            if (!weapon.is_object())
                continue;

            WeaponInventoryEntry entry{};
            entry.characterId = weapon.value("character_id", string{});
            entry.weaponId = weapon.value("weapon_id", string{});
            entry.enhanceLevel = weapon.value("enhance_level", 1u);
            entry.enhanceExp = weapon.value("enhance_exp", 0u);

            if (entry.characterId.empty() || entry.weaponId.empty() || Is_WeaponUnlocked(entry.characterId, entry.weaponId))
                continue;

            _ownedWeapons.emplace_back(std::move(entry));
        }
    }

    return true;
}

Shared<PlayerInventory> PlayerInventory::Create()
{
    return make_shared<PlayerInventory>();
}

void PlayerInventory::Free()
{
    Clear();

    Base::Free();
}
