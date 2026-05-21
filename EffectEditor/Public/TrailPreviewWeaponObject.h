#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)
class ModelCom;
class ShaderCom;
NS_END

NS_BEGIN(Client)
class PB_Gustave;
class PlayableParts;
NS_END

NS_BEGIN(EffectEditor)

class TrailPreviewWeaponObject final : public GameObject
{
public:
    struct PreviewDesc : public GAMEOBJECT_DESC
    {
        Shared<PB_Gustave> gustave{};
    };

public:
    TrailPreviewWeaponObject(const ComPtr<Device>& device, const ComPtr<Context>& context);
    TrailPreviewWeaponObject(const TrailPreviewWeaponObject& prototype);
    ~TrailPreviewWeaponObject() override = default;

public:
    HRESULT Initialize_Prototype() override { return S_OK; }
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

private:
    static constexpr const wchar_t* kWeaponModelRelativePath{
        L"../../../Client/Bin/Resources/Effects/Models/Editor_TrailPreview/Lanceram_TrailPreview.model"
    };
    static constexpr const char* kWeaponSocketName{ "Weapon_R" };
    inline static const Vec3 kWeaponAttachRotationDegrees{ -90.f, 0.f, 180.f };
    inline static const Vec3 kFallbackLocalPosition{ 0.65f, 0.9f, 0.15f };
    inline static const Vec3 kTrailBaseLocalOffset{ 0.f, 0.04f, 0.f };
    inline static const Vec3 kTrailTipLocalOffset{ 0.f, 1.35f, 0.f };

private:
    Weak<PB_Gustave> _gustave{};
    Shared<ModelCom> _weaponModel{};
    Shared<ShaderCom> _weaponShader{};

    Matrix _weaponWorldMatrix{ Matrix::Identity };
    Vec3 _trailBaseWorldPosition{};
    Vec3 _trailTipWorldPosition{};
    bool _isSocketAttached{ false };
    bool _socketWarningLogged{ false };
    bool _socketDebugLogged{ false };

private:
    wstring Resolve_ClientResourcePath(const wchar_t* relativePath) const;
    HRESULT Ready_Components();
    HRESULT Ready_WeaponModel();
    void Configure_WeaponMaterials();
    PlayableParts* Get_BodyPart() const;
    Matrix Get_PlayerWorldMatrix() const;
    Matrix Build_WeaponOffsetMatrix() const;
    void Update_WeaponWorldMatrix();
    void Update_FallbackWorldMatrix(const Matrix& playerWorldMatrix);
    void Update_TrailPoints();
    void Log_AttachmentSocket(const PlayableParts& bodyPart, const Matrix& playerWorldMatrix);
    HRESULT Bind_ShaderResources(uint32 meshIndex);

public:
    static Shared<TrailPreviewWeaponObject> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
