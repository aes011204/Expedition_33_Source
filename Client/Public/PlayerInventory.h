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

    struct MaterialInventoryEntry
    {
        string materialId{};
        uint32 count = 0;
    };

public:
    PlayerInventory() = default;
    ~PlayerInventory() override = default;

public:
    uint32  Get_Chroma() const { return _chroma; }

    void    Set_Chroma(uint32 chroma) { _chroma = chroma; }
    void    Add_Chroma(uint32 amount) { _chroma += amount; }
    bool    Spend_Chroma(uint32 amount);
    void    Clear();

    // 재료
    const vector<MaterialInventoryEntry>& Get_OwnedMaterials() const { return _ownedMaterials; }
    uint32 Get_MaterialCount(const string& materialId) const;
    void Set_MaterialCount(const string& materialId, uint32 count);
    void Add_Material(const string& materialId, uint32 amount);
    bool Spend_Material(const string& materialId, uint32 amount);

    // 픽토스
    const vector<string>& Get_UnlockedPictosIds() const { return _unlockedPictosIds; }
    bool Is_PictosUnlocked(const string& pictosId) const;
    void Unlock_Pictos(const string& pictosId);
    void Ensure_DefaultUnlockedPictos(const vector<string>& pictosIds);

    // 무기
    const vector<WeaponInventoryEntry>& Get_OwnedWeapons() const { return _ownedWeapons; }
    const WeaponInventoryEntry* Get_WeaponEntry(const string& characterId, const string& weaponId) const;
    bool Is_WeaponUnlocked(const string& characterId, const string& weaponId) const;
    uint32 Get_WeaponEnhanceLevel(const string& characterId, const string& weaponId) const;
    bool Set_WeaponEnhanceLevel(const string& characterId, const string& weaponId, uint32 enhanceLevel);
    void Unlock_Weapon(const string& characterId, const string& weaponId, uint32 enhanceLevel = 1, uint32 enhanceExp = 0);
    void Ensure_DefaultUnlockedWeapons(const vector<WeaponInventoryEntry>& weapons);

    json Create_SaveJson() const;
    bool Apply_SaveJson(const json& data);

private:
    uint32 _chroma = 2000000;
    vector<string> _unlockedPictosIds{};
    vector<WeaponInventoryEntry> _ownedWeapons{};
    vector<MaterialInventoryEntry> _ownedMaterials{};

public:
    static Shared<PlayerInventory> Create();
    void Free() override;
};

NS_END
