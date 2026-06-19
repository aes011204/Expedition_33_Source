#pragma once

#include "Base.h"

NS_BEGIN(Client)

class PlayerInventory final : public Base
{
public:
    // Weapon Com까진 만들기 귀찮아서.. 그냥 인벤토리에서 처리
    struct WeaponInventoryEntry
    {
        string characterId{};
        string weaponId{};
        uint32 enhanceLevel = 1; // 강화 레벨
        uint32 enhanceExp = 0;   // 경험치ㄴ가 필요할지?
    };

public:
    PlayerInventory() = default;
    ~PlayerInventory() override = default;

public:
    uint32  Get_Chroma() const { return _chroma; }

    void    Set_Chroma(uint32 chroma) { _chroma = chroma; }
    void    Add_Chroma(uint32 amount) { _chroma += amount; }
    void    Clear();

    // 픽토스
    const vector<string>& Get_UnlockedPictosIds() const { return _unlockedPictosIds; }
    bool Is_PictosUnlocked(const string& pictosId) const;
    void Unlock_Pictos(const string& pictosId);
    void Ensure_DefaultUnlockedPictos(const vector<string>& pictosIds);

    // 무기
    const vector<WeaponInventoryEntry>& Get_OwnedWeapons() const { return _ownedWeapons; }
    bool Is_WeaponUnlocked(const string& characterId, const string& weaponId) const;
    void Unlock_Weapon(const string& characterId, const string& weaponId);
    void Ensure_DefaultUnlockedWeapons(const vector<WeaponInventoryEntry>& weapons);

    json Create_SaveJson() const;
    bool Apply_SaveJson(const json& data);

private:
    uint32 _chroma = 0;
    vector<string> _unlockedPictosIds{};
    vector<WeaponInventoryEntry> _ownedWeapons{};

public:
    static Shared<PlayerInventory> Create();
    void Free() override;
};

NS_END
