#pragma once

#include "Client_Defines.h"
#include "Monster_Part.h"

NS_BEGIN(Client)

class Mon_Dualliste_Sword_R final : public Monster_Part
{
    GENERATED_GAMEOBJECT(Mon_Dualliste_Sword_R, ObjectType::Weapon)

public:
    struct SWORD_DESC : Monster_Part::MONSTER_PART_DESC
    {
        const Matrix* socketMatrix = nullptr;
    };

public:
    Mon_Dualliste_Sword_R(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Mon_Dualliste_Sword_R(const Mon_Dualliste_Sword_R& prototype);
    virtual ~Mon_Dualliste_Sword_R() override = default;

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

private:
    virtual uint32 Decide_PassIndex(Shared<Material> mat) override;
    HRESULT Ready_DefaultComponents();
    HRESULT Ensure_RuntimeComponentsFromJson(const json& data);

private:
    const Matrix* _socketMatrix = nullptr;
    Matrix _offsetMatrix = {};
    wstring _trailEffectName = L"";

public:
    static Shared<Mon_Dualliste_Sword_R> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
