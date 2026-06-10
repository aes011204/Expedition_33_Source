#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ShaderCom;
class Camera;

struct ENGINE_DLL ShadowCascadeDebugInfo
{
    float expandedNear = 0.f;
    float expandedFar = 0.f;
    Vec3 centerWS = Vec3::Zero;
    Vec3 eyeWS = Vec3::Zero;
    Vec3 centerLS = Vec3::Zero;
    float stableRadius = 0.f;
    Vec3 minLS = Vec3::Zero;
    Vec3 maxLS = Vec3::Zero;
    Vec3 influenceMinLS = Vec3::Zero;
    Vec3 influenceMaxLS = Vec3::Zero;
};

// 현재 cascade에 그림자를 드리울 수 있는 오브젝트 범위
// sliceCorners + upstreamDir * extrusionDistance 로 긴 프리즘을 형성한다.
struct ENGINE_DLL ShadowCascadeInfluenceVolume 
{
    static constexpr uint32 MaxPlaneCount = 32;

    Vec4 planes[MaxPlaneCount]{};
    uint32 planeCount = 0u;
    Vec3 padding = Vec3::Zero;
    BoundingBox bounds{};
};

class ENGINE_DLL Shadow : public Base
{
public:
    static constexpr uint32 CascadeCount = ShadowLightDesc::SHADOW_CASCADE_COUNT;

public:
    explicit Shadow() = default;
    virtual ~Shadow() = default;

public:
    HRESULT Update_ShadowLight(const ShadowLightDesc& desc);
    HRESULT Set_CascadeLevel(CascadeLevel level);
    void    Set_CascadePresets(const ShadowCascadePresetSet& presets);

    HRESULT Update_FromCameraFrustum(
        const ShadowLightDesc& desc,
        const Shared<Camera>& camera,
        const Vec3& lightDir);

    HRESULT Bind_ShadowLightDesc(Shared<ShaderCom> shader, const char* lightFarName, const char* biasName, const char* intensityName);
    HRESULT Bind_TransformStateMatrix(Shared<ShaderCom> shader, const char* viewName, const char* projName);

    HRESULT Bind_ShadowCB(Shared<ShaderCom> shader);
    HRESULT Set_ActiveCascade(uint32 cascadeIndex);

    void    Clear();

public:
    const Matrix*           Get_ViewMatrix() const;
    const Matrix*           Get_ProjMatrix() const;
    const Matrix*           Get_CascadeViewMatrices() const;
    const Matrix*           Get_CascadeProjMatrices() const;
    const ShadowLightDesc*  Get_ShadowLightDesc() const;
    CascadeLevel            Get_CascadeLevel() const;
    const ShadowCascadePresetSet* Get_CascadePresets() const;
    const ShadowCascadeDebugInfo* Get_CascadeDebugInfos() const;
    const ShadowCascadeInfluenceVolume* Get_CascadeInfluenceVolumes() const;
    uint32                  Get_CascadeCount() const { return CascadeCount; }

public:
    static Unique<Shadow> Create();
    void Free() override;

private:
    void Refresh_DefaultCascadePresets(const ShadowLightDesc& desc);
    void Apply_ActiveCascadePreset(ShadowLightDesc& desc) const;

private:
    ShadowLightDesc _desc{};
    ShadowCascadePresetSet _cascadePresets{};
    CascadeLevel    _activeCascadeLevel = CascadeLevel::Normal;
    bool            _hasCustomCascadePresets = false;
    Matrix          _viewMatrix = Matrix::Identity;
    Matrix          _projMatrix = Matrix::Identity;
    Matrix          _cascadeViewMatrices[CascadeCount]{};
    Matrix          _cascadeProjMatrices[CascadeCount]{};
    ShadowCascadeInfluenceVolume _cascadeInfluenceVolumes[CascadeCount]{};
    float           _cascadeSplits[CascadeCount]{};
    ShadowCascadeDebugInfo _cascadeDebugInfos[CascadeCount]{};
    uint32          _activeCascadeIndex = 0;
};

NS_END
