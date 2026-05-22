#pragma once

#include "GameObject.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Engine)
class ModelCom;
class ShaderCom;
NS_END

NS_BEGIN(Client)
class PB_Gustave;
class PlayableParts;
NS_END

NS_BEGIN(EffectEditor)

class TrailPreviewWeaponObject final : public GameObject, public IEffectTrailSampleProvider
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
    // Weapon-local trail anchor가 현재 preview frame에서 world sample로 유효한지 반환한다.
    bool Has_TrailSample() const;
    // 다음 trail/ribbon consumer가 읽을 weapon-local base world sample이다.
    Vec3 Get_TrailBaseWorldPosition() const { return _trailBaseWorldPosition; }
    // 다음 trail/ribbon consumer가 읽을 weapon-local tip world sample이다.
    Vec3 Get_TrailTipWorldPosition() const { return _trailTipWorldPosition; }

    // Trail runtime이 attachment 계산을 몰라도 되도록 base/tip sample만 제공한다.
    bool Try_GetTrailSample(EffectTrailSample& outSample) const override;

private:
    static constexpr auto kWeaponModelRelativePath{
        L"../../../Client/Bin/Resources/Effects/Models/Editor_TrailPreview/Lanceram_TrailPreview.model"
    };
    static constexpr auto kWeaponSocketName{ "Weapon_R" };

    inline static const Vec3 kWeaponAttachRotationDegrees{ -90.f, 0.f, 180.f }; // 이 각도로 돌려야 무기가 제대로 장착된다.
    inline static const Vec3 kFallbackLocalPosition{ 0.65f, 0.9f, 0.15f };
    inline static const float kTrailAnchorMarkerRadius{ 0.03f };

private:
    Weak<PB_Gustave> _gustave{};
    Shared<ModelCom> _weaponModel{};
    Shared<ShaderCom> _weaponShader{};

    Matrix _weaponWorldMatrix{ Matrix::Identity };
    Vec3 _trailBaseWorldPosition{};
    Vec3 _trailTipWorldPosition{};
    bool _hasTrailSample{ false };
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
    void Draw_TrailAnchorMarkers() const;
    void Draw_TrailAnchorMarker(const Vec3& worldPosition, const Color& color) const;
    void Log_AttachmentSocket(const PlayableParts& bodyPart, const Matrix& playerWorldMatrix);
    HRESULT Bind_ShaderResources(uint32 meshIndex);

public:
    static Shared<TrailPreviewWeaponObject> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
