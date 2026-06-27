#include "PlayerInventory.h"

void PlayerInventory::Clear()
{
    _chroma = 0;
    _unlockedPictosIds.clear();
    _ownedWeapons.clear();
    _ownedMaterials.clear();
}

bool PlayerInventory::Spend_Chroma(uint32 amount)
{
    if (_chroma < amount)
        return false;

    _chroma -= amount;
    return true;
}

uint32 PlayerInventory::Get_MaterialCount(const string& materialId) const
{
    if (materialId.empty())
        return 0;

    const auto iter = find_if(_ownedMaterials.begin(), _ownedMaterials.end(),
        [&materialId](const MaterialInventoryEntry& entry)
        {
            return entry.materialId == materialId;
        });

    if (iter == _ownedMaterials.end())
        return 0;

    return iter->count;
}

void PlayerInventory::Set_MaterialCount(const string& materialId, uint32 count)
{
    if (materialId.empty())
        return;

    auto iter = find_if(_ownedMaterials.begin(), _ownedMaterials.end(),
        [&materialId](const MaterialInventoryEntry& entry)
        {
            return entry.materialId == materialId;
        });

    if (iter == _ownedMaterials.end())
    {
        if (count == 0)
            return;

        _ownedMaterials.push_back({ materialId, count });
        return;
    }

    if (count == 0)
    {
        _ownedMaterials.erase(iter);
        return;
    }

    iter->count = count;
}

void PlayerInventory::Add_Material(const string& materialId, uint32 amount)
{
    if (materialId.empty() || amount == 0)
        return;

    Set_MaterialCount(materialId, Get_MaterialCount(materialId) + amount);
}

bool PlayerInventory::Spend_Material(const string& materialId, uint32 amount)
{
    if (amount == 0)
        return true;

    const uint32 currentCount = Get_MaterialCount(materialId);
    if (currentCount < amount)
        return false;

    Set_MaterialCount(materialId, currentCount - amount);
    return true;
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

    return Get_WeaponEntry(characterId, weaponId) != nullptr;
}

const PlayerInventory::WeaponInventoryEntry* PlayerInventory::Get_WeaponEntry(
    const string& characterId,
    const string& weaponId) const
{
    if (characterId.empty() || weaponId.empty())
        return nullptr;

    const auto iter = find_if(_ownedWeapons.begin(), _ownedWeapons.end(),
        [&characterId, &weaponId](const WeaponInventoryEntry& entry)
        {
            return entry.characterId == characterId && entry.weaponId == weaponId;
        });

    if (iter == _ownedWeapons.end())
        return nullptr;

    return &(*iter);
}

uint32 PlayerInventory::Get_WeaponEnhanceLevel(const string& characterId, const string& weaponId) const
{
    const WeaponInventoryEntry* entry = Get_WeaponEntry(characterId, weaponId);
    if (entry == nullptr)
        return 1;

    return max(1u, entry->enhanceLevel);
}

bool PlayerInventory::Set_WeaponEnhanceLevel(
    const string& characterId,
    const string& weaponId,
    uint32 enhanceLevel)
{
    if (characterId.empty() || weaponId.empty())
        return false;

    auto iter = find_if(_ownedWeapons.begin(), _ownedWeapons.end(),
        [&characterId, &weaponId](const WeaponInventoryEntry& entry)
        {
            return entry.characterId == characterId && entry.weaponId == weaponId;
        });

    if (iter == _ownedWeapons.end())
        return false;

    iter->enhanceLevel = max(1u, enhanceLevel);
    return true;
}

void PlayerInventory::Unlock_Weapon(const string& characterId, const string& weaponId, uint32 enhanceLevel)
{
    if (characterId.empty() || weaponId.empty())
        return;

    if (Is_WeaponUnlocked(characterId, weaponId))
        return; // 같은 캐릭터의 같은 무기는 중복 저장하지 않는다.

    WeaponInventoryEntry entry{};
    entry.characterId = characterId;
    entry.weaponId = weaponId;
    entry.enhanceLevel = max(1u, enhanceLevel);
    entry.enhanceExp = 0;

    _ownedWeapons.push_back(entry);
}

void PlayerInventory::Ensure_DefaultUnlockedWeapons(const vector<WeaponInventoryEntry>& weapons)
{
    for (const WeaponInventoryEntry& weapon : weapons)
    {
        Unlock_Weapon(weapon.characterId, weapon.weaponId, weapon.enhanceLevel);
    }
}

json PlayerInventory::Create_SaveJson() const
{
    json root = json::object();
    root["chroma"] = _chroma;
    root["unlocked_pictos"] = _unlockedPictosIds;

    root["owned_materials"] = json::array();
    for (const MaterialInventoryEntry& material : _ownedMaterials)
    {
        if (material.materialId.empty() || material.count == 0)
            continue;

        json entry = json::object();
        entry["material_id"] = material.materialId;
        entry["count"] = material.count;
        root["owned_materials"].emplace_back(entry);
    }

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

    if (auto materials = data.find("owned_materials"); materials != data.end() && materials->is_array())
    {
        for (const json& material : *materials)
        {
            if (!material.is_object())
                continue;

            const string materialId = material.value("material_id", string{});
            const uint32 count = material.value("count", 0u);

            if (materialId.empty() || count == 0)
                continue;

            Set_MaterialCount(materialId, count);
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
