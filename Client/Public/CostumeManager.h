#pragma once

#include "Client_Defines.h"
#include "Base.h"

NS_BEGIN(Client)

struct CostumePartLocalTransform
{
    bool enabled = false;
    Vec3 position{ Vec3::Zero };
    Vec3 rotation{ Vec3::Zero };
    Vec3 scale{ Vec3::One };
};

struct CostumePartset
{
    wstring bodyModelTag{};         // 몸통
    wstring headModelTag{};         // 머리통
    wstring hairModelTag{};         // 머리카락
    wstring beardModelTag{};        // 턱수염
    wstring mustacheModelTag{};     // 콧수염
    wstring glassesModelTag{};      // 선글라스

    CostumePartLocalTransform bodyTransform{};
    CostumePartLocalTransform headTransform{};
    CostumePartLocalTransform hairTransform{};
    CostumePartLocalTransform beardTransform{};
    CostumePartLocalTransform mustacheTransform{};
    CostumePartLocalTransform glassesTransform{};
};

struct CostumePreset
{
    HeroId          heroId = HeroId::Gustave;
    string          presetId{};
    wstring         displayName{};
    CostumePartset  parts{};
};

struct CostumePresetDesc
{
    HeroId heroId = HeroId::Gustave;
    const char* presetId = "";
    const wchar_t* displayName = L"";

    const wchar_t* bodyModelTag = nullptr;
    const wchar_t* headModelTag = nullptr;
    const wchar_t* hairModelTag = nullptr;
    const wchar_t* beardModelTag = nullptr;
    const wchar_t* mustacheModelTag = nullptr;
    const wchar_t* glassesModelTag = nullptr;
};

struct DefaultCostumeDesc
{
    HeroId heroId = HeroId::Gustave;
    const char* presetId = "";
};

class CostumeManager final : public Base
{
public:
    CostumeManager() = default;
    ~CostumeManager() override = default;

public:
    HRESULT Initialize();

    const CostumePreset* Get_CurrentCostume(HeroId heroId) const;
    const CostumePreset* Get_Costume(HeroId heroId, const string& presetId) const;
    const vector<CostumePreset>* Get_Costumes(HeroId heroId) const;
    const CostumePreset* Set_CurrentCostume(HeroId heroId, const string& presetId);
    json Create_CurrentCostumeSaveJson() const;
    bool Apply_CurrentCostumeSaveJson(const json& data);

    // TEMP. 일단 임시로 Hero Manager에서 변경 예정
    const CostumePreset* Toggle_NextCostume(HeroId heroId);

private:
    void Register_DefaultCostumes();
    void Register_Costume(const CostumePreset& preset);

    static CostumePreset Make_Preset(const CostumePresetDesc& desc);
    static void Apply_DefaultPartPlacement(CostumePreset& preset);
    static string  ToStringOrEmpty(const char* value);
    static wstring ToWStringOrEmpty(const wchar_t* value);

private:
    umap<HeroId, vector<CostumePreset>> _costumesByHero{};
    umap<HeroId, string>                _currentCostumeByHero{};


public:
    static Unique<CostumeManager> Create();
    void Free() override;
};

NS_END
