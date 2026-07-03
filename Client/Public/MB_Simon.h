#pragma once
#include "Monster_BattleBase.h"

NS_BEGIN(Client)

class Mon_Simon_Body;
class Mon_Simon_Hair;
class Mon_Simon_Weapon;

class MB_Simon : public Monster_BattleBase
{
    GENERATED_GAMEOBJECT(MB_Simon, ObjectType::Monster)

public:
    struct LoadedWeaponPrefab
    {
        json        rootJson  {};
        wstring     modelTag  {};
        string      modelGuid {};
        ModelType   modelType { ModelType::NonAnim };
    };

public:
    MB_Simon(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MB_Simon(const MB_Simon& prototype);
    ~MB_Simon() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;

    void From_Json(const json& data) override;

protected:
    HRESULT Ready_PartObjects();
    bool    Should_ApplySerializedPart(const wstring& partTag, const json& partObjectJson) const override;

    void Apply_WeaponScaleMultiplier();

private:
    Shared<Mon_Simon_Body> _body = {};
    Shared<Mon_Simon_Hair> _hair = {};
    Shared<Mon_Simon_Weapon> _weapon = {};

    float _weaponScaleMultiplier = 1.f;
    Vec3  _weaponBaseScale = Vec3::One;

public:
    static Shared<MB_Simon> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
