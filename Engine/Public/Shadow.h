#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ShaderCom;
class Camera;

class ENGINE_DLL Shadow : public Base
{
public:
    explicit Shadow() = default;
    virtual ~Shadow() = default;

public:
    HRESULT Update_ShadowLight(const ShadowLightDesc& desc);

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

    void    Clear();

public:
    const Matrix*           Get_ViewMatrix() const;
    const Matrix*           Get_ProjMatrix() const;
    const ShadowLightDesc*  Get_ShadowLightDesc() const;

public:
    static Unique<Shadow> Create();
    void Free() override;

private:
    ShadowLightDesc _desc{};
    Matrix          _viewMatrix = Matrix::Identity;
    Matrix          _projMatrix = Matrix::Identity;

    Matrix          _staticViewMatrix = Matrix::Identity;
    Matrix          _staticProjMatrix = Matrix::Identity;
    bool            _staticShadowEnabled = false;
};

NS_END
