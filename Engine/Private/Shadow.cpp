#include "pch.h"
#include "Shadow.h"
#include "ShaderCom.h"
#include "Shader_CBuffer_Define.h"
#include "Camera.h"

namespace
{
    ShadowCascadePreset& Get_CascadePreset(ShadowCascadePresetSet& presets, CascadeLevel level)
    {
        switch (level)
        {
        case CascadeLevel::Close:
            return presets.close;
        case CascadeLevel::Far:
            return presets.farRange;
        case CascadeLevel::Normal:
        default:
            return presets.normal;
        }
    }

    const ShadowCascadePreset& Get_CascadePreset(const ShadowCascadePresetSet& presets, CascadeLevel level)
    {
        switch (level)
        {
        case CascadeLevel::Close:
            return presets.close;
        case CascadeLevel::Far:
            return presets.farRange;
        case CascadeLevel::Normal:
        default:
            return presets.normal;
        }
    }

    ShadowCascadePreset Sanitize_CascadePreset(const ShadowCascadePreset& preset)
    {
        ShadowCascadePreset sanitized = preset;
        sanitized.focusNear = max(0.001f, sanitized.focusNear);
        sanitized.focusFar = max(sanitized.focusNear + 1.f, sanitized.focusFar);
        sanitized.cascadeSplitLambda = min(1.f, max(0.f, sanitized.cascadeSplitLambda));
        sanitized.cascadeBlendDistance = max(0.f, sanitized.cascadeBlendDistance);
        return sanitized;
    }

    struct CascadeBounds
    {
        Vec3 minLS = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
        Vec3 maxLS = Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        Vec3 centerWS = Vec3::Zero;
        Vec3 eyeWS = Vec3::Zero;
    };

    float Compute_CascadeSplit(
        uint32 cascadeIndex,
        uint32 cascadeCount,
        float nearZ,
        float farZ,
        float lambda)
    {
        const float ratio = farZ / max(nearZ, 0.0001f);
        const float t = static_cast<float>(cascadeIndex + 1) / static_cast<float>(max(1u, cascadeCount));
        const float logSplit = nearZ * powf(ratio, t);
        const float uniformSplit = nearZ + (farZ - nearZ) * t;
        return uniformSplit + (logSplit - uniformSplit) * lambda;
    }

    void Build_CascadeBounds(
        const Matrix& cameraWorld,
        float nearZ,
        float farZ,
        float tanHalfFovY,
        float aspect,
        const Vec3& lightDir,
        const ShadowLightDesc& desc,
        Matrix& outView,
        Matrix& outProj,
        CascadeBounds& outBounds)
    {
        Vec3 corners[8]{};
        uint32 index = 0;
        const float depthList[2] = { nearZ, farZ };

        for (uint32 depthIndex = 0; depthIndex < 2; ++depthIndex)
        {
            const float z = depthList[depthIndex];
            const float halfY = z * tanHalfFovY;
            const float halfX = halfY * aspect;

            const Vec3 viewCorners[4] =
            {
                Vec3(-halfX,  halfY, z),
                Vec3(halfX,   halfY, z),
                Vec3(halfX,  -halfY, z),
                Vec3(-halfX, -halfY, z),
            };

            for (uint32 cornerIndex = 0; cornerIndex < 4; ++cornerIndex)
                corners[index++] = Vec3::Transform(viewCorners[cornerIndex], cameraWorld);
        }

        Vec3 center = Vec3::Zero;
        for (const Vec3& corner : corners)
            center += corner;
        center /= 8.f;

        float radius = 0.f;
        for (const Vec3& corner : corners)
            radius = max(radius, (corner - center).Length());

        const Vec3 eye = center - lightDir * (radius + desc.lightDistancePadding);

        Vec3 up = Vec3::Up;
        if (fabsf(lightDir.Dot(up)) > 0.95f)
            up = Vec3::Forward;

        outView = XMMatrixLookAtLH(eye, center, up);

        Vec3 minLS(FLT_MAX, FLT_MAX, FLT_MAX);
        Vec3 maxLS(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (const Vec3& corner : corners)
        {
            const Vec3 lightSpace = Vec3::Transform(corner, outView);
            minLS = Vec3::Min(minLS, lightSpace);
            maxLS = Vec3::Max(maxLS, lightSpace);
        }

        minLS.x -= desc.xyPadding;
        maxLS.x += desc.xyPadding;
        minLS.y -= desc.xyPadding;
        maxLS.y += desc.xyPadding;
        minLS.z -= desc.zPadding;
        maxLS.z += desc.zPadding;

        const float extentX = max(1.f, maxLS.x - minLS.x);
        const float extentY = max(1.f, maxLS.y - minLS.y);
        const float texelX = extentX / static_cast<float>(max(1u, desc.shadowMapWidth));
        const float texelY = extentY / static_cast<float>(max(1u, desc.shadowMapHeight));

        minLS.x = floorf(minLS.x / texelX) * texelX;
        maxLS.x = ceilf(maxLS.x / texelX) * texelX;
        minLS.y = floorf(minLS.y / texelY) * texelY;
        maxLS.y = ceilf(maxLS.y / texelY) * texelY;

        if (maxLS.z <= minLS.z)
            maxLS.z = minLS.z + 1.f;

        outProj = XMMatrixOrthographicOffCenterLH(
            minLS.x, maxLS.x,
            minLS.y, maxLS.y,
            minLS.z, maxLS.z);

        outBounds.minLS = minLS;
        outBounds.maxLS = maxLS;
        outBounds.centerWS = center;
        outBounds.eyeWS = eye;
    }
}

HRESULT Shadow::Update_ShadowLight(const ShadowLightDesc& desc)
{
    _desc = desc;

    if (!_hasCustomCascadePresets)
        Refresh_DefaultCascadePresets(desc);

    if (_desc.followCameraFrustum)
    {
        Apply_ActiveCascadePreset(_desc);
        return S_OK;
    }

    // Camera frustum 기반 shadow는 Render_Shadow 직전에 Update_FromCameraFrustum()에서
    // orthographic view/proj를 다시 계산한다. 여기서 perspective matrix를 만들면
    // light-space zn/zf가 음수일 때 XMMatrixPerspectiveFovLH assert가 발생할 수 있다.
    _viewMatrix = XMMatrixLookAtLH(desc.eye, desc.at, Vec3::Up);
    _projMatrix = XMMatrixPerspectiveFovLH(desc.fovy, desc.aspect, desc.zn, desc.zf);

    for (uint32 cascadeIndex = 0; cascadeIndex < CascadeCount; ++cascadeIndex)
    {
        _cascadeViewMatrices[cascadeIndex] = _viewMatrix;
        _cascadeProjMatrices[cascadeIndex] = _projMatrix;
        _cascadeSplits[cascadeIndex] = desc.zf;
    }

    return S_OK;
}

HRESULT Shadow::Set_CascadeLevel(CascadeLevel level)
{
    if (level >= CascadeLevel::Count)
        return E_FAIL;

    _activeCascadeLevel = level;
    if (_desc.followCameraFrustum)
        Apply_ActiveCascadePreset(_desc);

    return S_OK;
}

void Shadow::Set_CascadePresets(const ShadowCascadePresetSet& presets)
{
    _cascadePresets.close = Sanitize_CascadePreset(presets.close);
    _cascadePresets.normal = Sanitize_CascadePreset(presets.normal);
    _cascadePresets.farRange = Sanitize_CascadePreset(presets.farRange);
    _hasCustomCascadePresets = true;

    if (_desc.followCameraFrustum)
        Apply_ActiveCascadePreset(_desc);
}

HRESULT Shadow::Update_FromCameraFrustum(const ShadowLightDesc& desc, const Shared<Camera>& camera, const Vec3& lightDir)
{
    if (camera == nullptr || camera->Get_Transform() == nullptr)
        return E_FAIL;

    _desc = desc;
    if (!_hasCustomCascadePresets)
        Refresh_DefaultCascadePresets(desc);
    Apply_ActiveCascadePreset(_desc);

    Vec3 dir = lightDir;
    if (dir.LengthSquared() < 0.0001f)
        dir = Vec3(1.f, -1.f, -1.f);

    dir.Normalize();

    const float aspect = /* x / y */
        static_cast<float>(GAME->Get_ViewportWidth()) /
        static_cast<float>(max(1u, GAME->Get_ViewportHeight()));

    const float fovY = camera->Get_FovY();
    const float tanHalfFovY = tanf(fovY * 0.5f);

    // 카메라 전체 far까지 찍으면 그림자의 품질이 나빠지기 때문에 가까운 구간만 cascade로 나눠 커버한다.
    const float nearZ = max(_desc.focusNear, camera->Get_Near());
    const float farZ = max(nearZ + 1.f, min(_desc.focusFar, camera->Get_Far()));

    const Matrix cameraWorld = camera->Get_Transform()->Get_WorldMatrix();

    float previousSplit = nearZ;
    const float lambda = min(1.f, max(0.f, _desc.cascadeSplitLambda));

    for (uint32 cascadeIndex = 0; cascadeIndex < CascadeCount; ++cascadeIndex)
    {
        const float cascadeFar = (cascadeIndex + 1u == CascadeCount)
            ? farZ
            : Compute_CascadeSplit(cascadeIndex, CascadeCount, nearZ, farZ, lambda);

        CascadeBounds cascadeBounds{};
        Build_CascadeBounds(
            cameraWorld,
            previousSplit,
            cascadeFar,
            tanHalfFovY,
            aspect,
            dir,
            _desc,
            _cascadeViewMatrices[cascadeIndex],
            _cascadeProjMatrices[cascadeIndex],
            cascadeBounds);

        _cascadeSplits[cascadeIndex] = cascadeFar;
        previousSplit = cascadeFar;

        if (cascadeIndex == 0)
        {
            _viewMatrix = _cascadeViewMatrices[cascadeIndex];
            _projMatrix = _cascadeProjMatrices[cascadeIndex];
            _desc.eye = cascadeBounds.eyeWS;
            _desc.at = cascadeBounds.centerWS;
            _desc.zn = cascadeBounds.minLS.z;
        }
    }

    _desc.zf = farZ;

    return S_OK;
}

HRESULT Shadow::Update_StaticShadowFromBounds(const ShadowLightDesc& desc, const BoundingBox& bounds,
    const Vec3& lightDir)
{
    Vec3 dir = lightDir;
    if (dir.LengthSquared() < 0.0001f)
        dir = Vec3(1.f, -1.f, -1.f);

    dir.Normalize();

    Vec3 corners[8]{};
    bounds.GetCorners(corners);

    Vec3 center = Vec3::Zero;
    for (const Vec3& corner : corners)
        center += corner;
    center /= 8.f;

    float radius = 0.f;
    for (const Vec3& corner : corners)
        radius = max(radius, (corner - center).Length());

    const Vec3 eye = center - dir * (radius + desc.lightDistancePadding);

    Vec3 up = Vec3::Up;
    if (fabsf(dir.Dot(up)) > 0.95f)
        up = Vec3::Forward;

    _staticViewMatrix = XMMatrixLookAtLH(eye, center, up);

    Vec3 minLS(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3 maxLS(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (const Vec3& corner : corners)
    {
        const Vec3 lightSpace = Vec3::Transform(corner, _staticViewMatrix);
        minLS = Vec3::Min(minLS, lightSpace);
        maxLS = Vec3::Max(maxLS, lightSpace);
    }

    minLS.x -= desc.xyPadding;
    maxLS.x += desc.xyPadding;
    minLS.y -= desc.xyPadding;
    maxLS.y += desc.xyPadding;
    minLS.z -= desc.zPadding;
    maxLS.z += desc.zPadding;

    if (maxLS.z <= minLS.z)
        maxLS.z = minLS.z + 1.f;

    _staticProjMatrix = XMMatrixOrthographicOffCenterLH(
        minLS.x,
        maxLS.x,
        minLS.y,
        maxLS.y,
        minLS.z,
        maxLS.z);

    _staticShadowEnabled = true;

    return S_OK;
}

HRESULT Shadow::Bind_TransformStateMatrix(Shared<ShaderCom> shader, const char* viewName, const char* projName)
{
    CHECK_NULL(shader, E_FAIL);
    CHECK_FAILED(shader->Bind_Matrix(viewName, &_viewMatrix), E_FAIL);
    CHECK_FAILED(shader->Bind_Matrix(projName, &_projMatrix), E_FAIL);

    return S_OK;
}

HRESULT Shadow::Bind_ShadowCB(Shared<ShaderCom> shader)
{
    CHECK_NULL(shader, E_FAIL);

    ShadowCB shadowCB{};

    shadowCB.lightViewMatrix = _viewMatrix;
    shadowCB.lightProjMatrix = _projMatrix;

    for (uint32 cascadeIndex = 0; cascadeIndex < CascadeCount; ++cascadeIndex)
    {
        shadowCB.cascadeLightViewMatrices[cascadeIndex] = _cascadeViewMatrices[cascadeIndex];
        shadowCB.cascadeLightProjMatrices[cascadeIndex] = _cascadeProjMatrices[cascadeIndex];
    }
    shadowCB.cascadeSplits = Vec4(
        _cascadeSplits[0],
        _cascadeSplits[1],
        _cascadeSplits[2],
        _cascadeSplits[3]);

    shadowCB.staticLightViewMatrix = _staticViewMatrix;
    shadowCB.staticLightProjMatrix = _staticProjMatrix;

    shadowCB.cascadeBlendDistance = _desc.cascadeBlendDistance;
    shadowCB.lightFar = _desc.zf;
    shadowCB.bias = _desc.bias;
    shadowCB.intensity = _desc.enabled ? _desc.intensity : 0.f;
    shadowCB.staticShadowEnabled = _staticShadowEnabled ? 1.f : 0.f;
    shadowCB.cascadeCount = CascadeCount;
    shadowCB.activeCascadeIndex = _activeCascadeIndex;

    return shader->Bind_CBufferData(shadowCB);
}

HRESULT Shadow::Bind_StaticShadowCB(Shared<ShaderCom> shader)
{
    CHECK_NULL(shader, E_FAIL);

    ShadowCB shadowCB{};

    shadowCB.lightViewMatrix = _staticViewMatrix;
    shadowCB.lightProjMatrix = _staticProjMatrix;

    for (uint32 cascadeIndex = 0; cascadeIndex < CascadeCount; ++cascadeIndex)
    {
        shadowCB.cascadeLightViewMatrices[cascadeIndex] = _cascadeViewMatrices[cascadeIndex];
        shadowCB.cascadeLightProjMatrices[cascadeIndex] = _cascadeProjMatrices[cascadeIndex];
    }
    shadowCB.cascadeSplits = Vec4(
        _cascadeSplits[0],
        _cascadeSplits[1],
        _cascadeSplits[2],
        _cascadeSplits[3]);

    shadowCB.staticLightViewMatrix = _staticViewMatrix;
    shadowCB.staticLightProjMatrix = _staticProjMatrix;

    shadowCB.cascadeBlendDistance = _desc.cascadeBlendDistance;
    shadowCB.lightFar = _desc.zf;
    shadowCB.bias = _desc.bias;
    shadowCB.intensity = _desc.enabled ? _desc.intensity : 0.f;
    shadowCB.staticShadowEnabled = _staticShadowEnabled ? 1.f : 0.f;
    shadowCB.cascadeCount = CascadeCount;
    shadowCB.activeCascadeIndex = _activeCascadeIndex;

    return shader->Bind_CBufferData(shadowCB);
}

HRESULT Shadow::Set_ActiveCascade(uint32 cascadeIndex)
{
    if (cascadeIndex >= CascadeCount)
        return E_FAIL;

    _activeCascadeIndex = cascadeIndex;
    _viewMatrix = _cascadeViewMatrices[cascadeIndex];
    _projMatrix = _cascadeProjMatrices[cascadeIndex];

    return S_OK;
}

void Shadow::Set_ActiveStaticShadow()
{
    _activeCascadeIndex = 0;
    _viewMatrix = _staticViewMatrix;
    _projMatrix = _staticProjMatrix;
}

HRESULT Shadow::Bind_ShadowLightDesc(Shared<ShaderCom> shader, const char* lightFarName, const char* biasName, const char* intensityName)
{
    if (lightFarName)   
        CHECK_FAILED(shader->Bind_RawValue(lightFarName, &_desc.zf, sizeof _desc.zf), E_FAIL);
    if (biasName)
        CHECK_FAILED(shader->Bind_RawValue(biasName, &_desc.bias, sizeof _desc.bias), E_FAIL);
    if (intensityName)
        CHECK_FAILED(shader->Bind_RawValue(intensityName, &_desc.intensity, sizeof _desc.intensity), E_FAIL);

    return S_OK;
}

void Shadow::Clear()
{
    _desc = {};
    _cascadePresets = {};
    _activeCascadeLevel = CascadeLevel::Normal;
    _hasCustomCascadePresets = false;
    _viewMatrix = Matrix::Identity;
    _projMatrix = Matrix::Identity;
    _activeCascadeIndex = 0;
    _staticViewMatrix = Matrix::Identity;
    _staticProjMatrix = Matrix::Identity;
    _staticShadowEnabled = false;

    for (uint32 cascadeIndex = 0; cascadeIndex < CascadeCount; ++cascadeIndex)
    {
        _cascadeViewMatrices[cascadeIndex] = Matrix::Identity;
        _cascadeProjMatrices[cascadeIndex] = Matrix::Identity;
        _cascadeSplits[cascadeIndex] = 0.f;
    }
}

Unique<Shadow> Shadow::Create()
{
    return make_unique<Shadow>();
}

void Shadow::Free()
{
    Base::Free();
}

const Matrix* Shadow::Get_ViewMatrix() const
{
    return &_viewMatrix;
}

const Matrix* Shadow::Get_ProjMatrix() const
{
    return &_projMatrix;
}

const ShadowLightDesc* Shadow::Get_ShadowLightDesc() const
{
    return &_desc;
}

CascadeLevel Shadow::Get_CascadeLevel() const
{
    return _activeCascadeLevel;
}

const ShadowCascadePresetSet* Shadow::Get_CascadePresets() const
{
    return &_cascadePresets;
}

void Shadow::Refresh_DefaultCascadePresets(const ShadowLightDesc& desc)
{
    ShadowCascadePreset normal{};
    normal.focusNear = desc.focusNear;
    normal.focusFar = desc.focusFar;
    normal.cascadeSplitLambda = desc.cascadeSplitLambda;
    normal.cascadeBlendDistance = desc.cascadeBlendDistance;
    normal = Sanitize_CascadePreset(normal);

    ShadowCascadePreset close = normal;
    close.focusFar = max(normal.focusNear + 1.f, normal.focusFar * 0.35f);
    close.cascadeSplitLambda = min(1.f, max(normal.cascadeSplitLambda, 0.92f));
    close = Sanitize_CascadePreset(close);

    ShadowCascadePreset farPreset = normal;
    farPreset.focusFar = max(normal.focusFar, min(desc.zf, normal.focusFar * 1.75f));
    farPreset.cascadeSplitLambda = min(1.f, max(0.f, normal.cascadeSplitLambda * 0.65f));
    farPreset = Sanitize_CascadePreset(farPreset);

    _cascadePresets.close = close;
    _cascadePresets.normal = normal;
    _cascadePresets.farRange = farPreset;
}

void Shadow::Apply_ActiveCascadePreset(ShadowLightDesc& desc) const
{
    const ShadowCascadePreset& preset = Get_CascadePreset(_cascadePresets, _activeCascadeLevel);
    desc.focusNear = preset.focusNear;
    desc.focusFar = preset.focusFar;
    desc.cascadeSplitLambda = preset.cascadeSplitLambda;
    desc.cascadeBlendDistance = preset.cascadeBlendDistance;
}
