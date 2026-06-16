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

void PlayerInventory::Unlock_Weapon(const string& characterId, const string& weaponId)
{
    if (characterId.empty() || weaponId.empty())
        return;

    if (Is_WeaponUnlocked(characterId, weaponId))
        return; // 같은 캐릭터의 같은 무기는 중복 저장하지 않는다.

    WeaponInventoryEntry entry{};
    entry.characterId = characterId;
    entry.weaponId = weaponId;
    entry.enhanceLevel = 1;
    entry.enhanceExp = 0;

    _ownedWeapons.push_back(entry);
}

void PlayerInventory::Ensure_DefaultUnlockedWeapons(const vector<WeaponInventoryEntry>& weapons)
{
    for (const WeaponInventoryEntry& weapon : weapons)
    {
        Unlock_Weapon(weapon.characterId, weapon.weaponId);
    }
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
