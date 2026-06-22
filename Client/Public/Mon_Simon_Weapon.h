#pragma once

#include "Client_Defines.h"
#include "Monster_Part.h"

NS_BEGIN(Client)

class Mon_Simon_Weapon final : public Monster_Part
{
    GENERATED_GAMEOBJECT(Mon_Simon_Weapon, ObjectType::Weapon)

public:
    struct  WEAPON_DESC : Monster_Part::MONSTER_PART_DESC
    {
        const  Matrix* socketMatrix = nullptr;
    };

public:
    Mon_Simon_Weapon(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Mon_Simon_Weapon(const Mon_Simon_Weapon& prototype);
    virtual ~Mon_Simon_Weapon() override = default;

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
    void Set_UseSocketBinding(bool useSocketBinding) { _useSocketBinding = useSocketBinding; }
    void Detach_PreserveWorld();
    void Attach_ToBindings();

    void Begin_WeaponEchoPlayback(float delaySec, float startAlpha, float endAlpha, const Vec4& tintColor);
    void Record_WeaponEchoSample(float elapsedSec);
    void End_WeaponEchoPlayback();
    bool Has_WeaponEchoPlayback() const;
    void Queue_WeaponEchoRender();
    const wstring& Get_TrailEffectName() const { return _trailEffectName; }

private:
    virtual uint32 Decide_PassIndex(Shared<Material> mat) override;
    HRESULT Ready_DefaultComponents();
    HRESULT Ensure_RuntimeComponentsFromJson(const json& data);

    void Update_WeaponEchoPlayback(float timeDelta);
    bool Try_SampleWeaponEchoMatrix(float elapsedSec, Matrix& outWorldMatrix) const;
    HRESULT Render_WeaponEchoPlayback();

private:
    const Matrix* _socketMatrix = nullptr;
    Matrix _offsetMatrix = {};
    Matrix _detachedWorldMatrix = Matrix::Identity;
    bool _useSocketBinding = true;
    bool _isDetached = false;

    struct WeaponEchoSample
    {
        float elapsedSec = 0.f;
        Matrix worldMatrix = Matrix::Identity;
    };

    vector<WeaponEchoSample> _weaponEchoSamples{};
    bool _weaponEchoActive = false;
    bool _weaponEchoRecordingEnded = false;
    float _weaponEchoDelaySec = 0.2f;
    float _weaponEchoPlaybackSec = 0.f;
    float _weaponEchoEndElapsedSec = 0.f;
    float _weaponEchoStartAlpha = 0.42f;
    float _weaponEchoEndAlpha = 0.f;
    Vec4 _weaponEchoTintColor = Vec4(0.70f, 1.0f, 0.92f, 1.f);

    wstring _trailEffectName = L"";

public:
    static Shared<Mon_Simon_Weapon> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
