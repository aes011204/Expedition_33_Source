#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ShaderCom;
class Camera;

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

    HRESULT Update_StaticShadowFromBounds(
        const ShadowLightDesc& desc,
        const BoundingBox& bounds,
        const Vec3& lightDir);

    HRESULT Bind_ShadowLightDesc(Shared<ShaderCom> shader, const char* lightFarName, const char* biasName, const char* intensityName);
    HRESULT Bind_TransformStateMatrix(Shared<ShaderCom> shader, const char* viewName, const char* projName);

    HRESULT Bind_ShadowCB(Shared<ShaderCom> shader);
    HRESULT Bind_StaticShadowCB(Shared<ShaderCom> shader);
    HRESULT Set_ActiveCascade(uint32 cascadeIndex);
    void    Set_ActiveStaticShadow();

    void    Clear();

public:
    const Matrix*           Get_ViewMatrix() const;
    const Matrix*           Get_ProjMatrix() const;
    const ShadowLightDesc*  Get_ShadowLightDesc() const;
    CascadeLevel            Get_CascadeLevel() const;
    const ShadowCascadePresetSet* Get_CascadePresets() const;
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
    float           _cascadeSplits[CascadeCount]{};
    uint32          _activeCascadeIndex = 0;

    Matrix          _staticViewMatrix = Matrix::Identity;
    Matrix          _staticProjMatrix = Matrix::Identity;
    bool            _staticShadowEnabled = false;
};

NS_END
