#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)
class ModelCom;
class ShaderCom;
NS_END

NS_BEGIN(EffectEditor)

class TrailPreviewModelObject final : public GameObject
{
public:
    TrailPreviewModelObject(const ComPtr<Device>& device, const ComPtr<Context>& context);
    TrailPreviewModelObject(const TrailPreviewModelObject& prototype);
    ~TrailPreviewModelObject() override = default;

public:
    HRESULT Initialize_Prototype() override { return S_OK; }
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;

private:
    static constexpr const wchar_t* kModelRelativePath{
        L"../../../Client/Bin/Resources/Effects/Models/Editor_TrailPreview/Lanceram_TrailPreview.model"
    };

    inline static const Vec3 kRestPosition{ 0.f, 0.f, 0.f };
    inline static const Vec3 kRestRotationDegrees{ -90.f, 0.f, 0.f };
    inline static const Vec3 kRestScale{ 1.f, 1.f, 1.f };

    inline static const Vec3 kTrailBaseLocalOffset{ 0.f, 0.04f, 0.f };
    inline static const Vec3 kTrailTipLocalOffset{ 0.f, 1.35f, 0.f };

    static constexpr float kSwingDurationSeconds{ 1.5f };
    static constexpr float kSwingAnticipationEnd{ 0.12f };
    static constexpr float kSwingSlashEnd{ 0.48f };
    static constexpr float kSwingDampEnd{ 0.64f };
    static constexpr float kSlashAnticipationDegrees{ -8.f };
    static constexpr float kSlashAmplitudeDegrees{ 90.f };
    static constexpr float kDampedHoldDegrees{ 84.f };

private:
    Shared<ModelCom> _model{};
    Shared<ShaderCom> _shader{};
    float _swingTime{ 0.f };

private:
    wstring Resolve_ModelPath();
    HRESULT Ready_Components();
    HRESULT Bind_ShaderResources();
    void Configure_Materials();
    void Reset_Pose();
    void Update_Swing(float timeDelta);

public:
    static Shared<TrailPreviewModelObject> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
