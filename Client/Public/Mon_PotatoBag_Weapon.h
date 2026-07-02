#pragma once

#include "Client_Defines.h"
#include "Monster_Part.h"
#include "Weapon.h"

NS_BEGIN(Client)

class Mon_PotatoBag_Weapon final : public Monster_Part
{
    GENERATED_GAMEOBJECT(Mon_PotatoBag_Weapon, ObjectType::Weapon)

public:
    struct WEAPON_DESC : Monster_Part::MONSTER_PART_DESC
    {
        const Matrix* socketMatrix = nullptr;
    };

public:
    Mon_PotatoBag_Weapon(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Mon_PotatoBag_Weapon(const Mon_PotatoBag_Weapon& prototype);
    virtual ~Mon_PotatoBag_Weapon() override = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    virtual void Priority_Update(float timeDelta) override;
    virtual void Update(float timeDelta) override;
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;
    virtual HRESULT Render_Shadow() override;
    void From_Json(const json& data) override;

public:
    const wstring& Get_TrailEffectName() const { return _trailEffectName; }
    vector<WeaponSourceHistoryAnchorDesc>& Get_SourceHistoryAnchors() { return _sourceHistoryAnchors; }
    const vector<WeaponSourceHistoryAnchorDesc>& Get_SourceHistoryAnchors() const { return _sourceHistoryAnchors; }
    const WeaponSourceHistoryAnchorDesc* Find_SourceHistoryAnchor(const string& anchorName) const;

private:
    virtual uint32 Decide_PassIndex(Shared<Material> mat) override;
    json To_Json() const override;
    HRESULT Ready_DefaultComponents();
    HRESULT Ensure_RuntimeComponentsFromJson(const json& data);

private:
    const Matrix* _socketMatrix = nullptr;
    Matrix _offsetMatrix = {};
    wstring _trailEffectName = L"";
    vector<WeaponSourceHistoryAnchorDesc> _sourceHistoryAnchors{};

public:
    static Shared<Mon_PotatoBag_Weapon> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
