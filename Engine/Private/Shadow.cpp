#include "pch.h"
#include "ShadowHelper.h"

using namespace ShadowHelper;

#pragma region 그림자 설명
/* === 그림자 개념 요약 ===
* 카메라 뷰 공간에서 캐스케이드 구간을 자른 뒤,
* 그 구간의 프러스텀 꼭짓점 8개를 월드 공간으로 변환한다.
* 그리고 그 8개 점을 라이트 뷰 공간으로 옮긴 뒤, 그 점들을 모두 포함하는 직교 투영 영역을 만든다.
* 렌더링할 때는 오브젝트 월드 행렬에 해당 뷰, 투영 행렬을 적용한다.
*/

/* === 그림자 볼륨 설명 ===
* receiver 볼륨 : 현재 cascade 슬라이스만 빛 카메라 공간으로 옮긴 수신 범위
* projection 볼륨 : shadow map ortho projection. receiver 볼륨에서 시작하여 안정화한 범위.
* influence 볼륨 : 현재 cascade 안에 안 보이더라도, 이 cascade에 그림자를 던질 수 있는 caster 범위.
*/

#pragma endregion

HRESULT Shadow::Update_ShadowLight(const ShadowLightDesc& desc)
{
    _desc = desc;
    _forceRenderAllCascades = true;

    if (!_hasCustomCascadePresets)
        Refresh_DefaultCascadePresets(desc);

    if (_desc.followCameraFrustum)
    {
        Apply_ActiveCascadePreset(_desc);
        return S_OK;
    }

    // Camera frustum 기반 shadow는 Render_Shadow 직전에 Update_FromCameraFrustum()에서 orthographic view/proj를 다시 계산한다.
    // 여기서 perspective matrix를 만들면 light-space zn/zf가 음수일 때 XMMatrixPerspectiveFovLH assert가 발생할 수 있다.
    _viewMatrix = XMMatrixLookAtLH(desc.eye, desc.at, Vec3::Up);
    _projMatrix = XMMatrixPerspectiveFovLH(desc.fovy, desc.aspect, desc.zn, desc.zf);

    for (uint32 cascadeIndex = 0; cascadeIndex < CascadeCount; ++cascadeIndex)
    {
        _cascadeViewMatrices[cascadeIndex] = _viewMatrix;
        _cascadeProjMatrices[cascadeIndex] = _projMatrix;
        _renderedCascadeViewMatrices[cascadeIndex] = _viewMatrix;
        _renderedCascadeProjMatrices[cascadeIndex] = _projMatrix;

        _cascadeSplits[cascadeIndex] = desc.zf;
        _renderedCascadeSplits[cascadeIndex] = desc.zf;
        _cascadeDebugInfos[cascadeIndex] = {};
        _cascadeInfluenceVolumes[cascadeIndex] = {};
        _hasRenderedCascade[cascadeIndex] = true;
    }

    return S_OK;
}

HRESULT Shadow::Set_CascadeLevel(CascadeLevel level)
{
    if (level >= CascadeLevel::Count)
        return E_FAIL;

    _activeCascadeLevel = level;
    if (_desc.followCameraFrustum)
    {
        Apply_ActiveCascadePreset(_desc);
        _forceRenderAllCascades = true;
    }

    return S_OK;
}

void Shadow::Set_CascadePresets(const ShadowCascadePresetSet& presets)
{
    _cascadePresets.close = Sanitize_CascadePreset(presets.close);
    _cascadePresets.normal = Sanitize_CascadePreset(presets.normal);
    _cascadePresets.farRange = Sanitize_CascadePreset(presets.farRange);
    _hasCustomCascadePresets = true;

    if (_desc.followCameraFrustum)
    {
        Apply_ActiveCascadePreset(_desc);
        _forceRenderAllCascades = true;
    }
}

// 카메라가 보는 영역을 기준으로 빛 방향의 shadow 카메라를 다시 만든다.
HRESULT Shadow::Update_FromCameraFrustum(
    const ShadowLightDesc& desc,
    const Shared<Camera>& camera,
    const Vec3& lightDir)
{
    if (camera == nullptr || camera->Get_Transform() == nullptr)
        return E_FAIL;

    _desc = desc;

    // 캐스케이드 구간 분할을 위한 프리셋 적용
    if (!_hasCustomCascadePresets)
        Refresh_DefaultCascadePresets(desc);
    Apply_ActiveCascadePreset(_desc);

    Vec3 dir = lightDir;
    if (dir.LengthSquared() < 0.0001f)
        dir = Vec3(1.f, -1.f, -1.f);
    dir.Normalize();

    // 카메라 프러스텀 구하기
    const float aspect = static_cast<float>(GAME->Get_ViewportWidth()) / static_cast<float>(max(1u, GAME->Get_ViewportHeight()));
    const float fovY = camera->Get_FovY();
    const float tanHalfFovY = tanf(fovY * 0.5f);    // 세로 반 크기
    const float nearZ = max(_desc.focusNear, camera->Get_Near());
    const float farZ = max(nearZ + 1.f, min(_desc.focusFar, camera->Get_Far()));
    const Matrix cameraWorld = camera->Get_Transform()->Get_WorldMatrix();

    Vec3 coverageCorners[8]{};
    Build_FrustumCorners(cameraWorld, nearZ, farZ, tanHalfFovY, aspect, coverageCorners);

    Vec3 coverageCenterWS = Vec3::Zero;
    float coverageRadius = 0.f;

    Compute_CornersBoundingSphere(coverageCorners, coverageCenterWS, coverageRadius);

    // cascade 구간 쪼개기
    float previousSplit = nearZ;
    const float lambda = min(1.f, max(0.f, _desc.cascadeSplitLambda));
    const float blendDistance = max(0.f, _desc.cascadeBlendDistance);

    for (uint32 cascadeIndex = 0; cascadeIndex < CascadeCount; ++cascadeIndex)
    {
        // 현재 캐스케이드가 실제로 담당하는 receiver 깊이 구간
        const float nominalNear = previousSplit;
        const float nominalFar = (cascadeIndex + 1u == CascadeCount)
            ? farZ
            : Compute_CascadeSplit(
                cascadeIndex,
                CascadeCount,
                nearZ,
                farZ,
                lambda);

        // 블렌드 구간 확인용 값.
        // shadow view/proj와 caster influence volume 생성에는 사용하지 않는다.
        const float expandedNear =
            max(nearZ, nominalNear - blendDistance);

        const float expandedFar =
            min(
                farZ,
                max(expandedNear + 0.001f, nominalFar + blendDistance));

        // 실제 캐스케이드 receiver 구간은 반드시 nominal 범위로 만든다.
        Vec3 sliceCorners[8]{};
        Build_FrustumCorners(
            cameraWorld,
            nominalNear,
            nominalFar,
            tanHalfFovY,
            aspect,
            sliceCorners);

        // 해당 receiver slice를 기준으로 shadow view/proj와 caster 영향 범위를 만든다.
        CascadeBounds cascadeBounds{};
        Build_CascadeBounds(
            sliceCorners,
            coverageCorners,
            coverageCenterWS,
            coverageRadius,
            dir,
            _desc,
            _cascadeViewMatrices[cascadeIndex],
            _cascadeProjMatrices[cascadeIndex],
            _cascadeInfluenceVolumes[cascadeIndex],
            cascadeBounds);

        _cascadeSplits[cascadeIndex] = nominalFar;

        // 현재 필드 이름은 expanded이므로 일단 그대로 저장.
        // 단, 이 값은 디버그 표시용일 뿐 cascade 행렬에는 사용되지 않는다.
        _cascadeDebugInfos[cascadeIndex].expandedNear = expandedNear;
        _cascadeDebugInfos[cascadeIndex].expandedFar = expandedFar;
        for (uint32 cornerIndex = 0; cornerIndex < 8u; ++cornerIndex)
            _cascadeDebugInfos[cascadeIndex].sliceCornersWS[cornerIndex] = sliceCorners[cornerIndex];
        for (uint32 pointIndex = 0u; pointIndex < 16u; ++pointIndex)
        {
            _cascadeDebugInfos[cascadeIndex].influencePointsWS[pointIndex] =
                cascadeBounds.influencePointsWS[pointIndex];
        }
        _cascadeDebugInfos[cascadeIndex].centerWS = cascadeBounds.centerWS;
        _cascadeDebugInfos[cascadeIndex].eyeWS = cascadeBounds.eyeWS;
        _cascadeDebugInfos[cascadeIndex].centerLS = cascadeBounds.centerLS;
        _cascadeDebugInfos[cascadeIndex].stableRadius = cascadeBounds.stableRadius;
        _cascadeDebugInfos[cascadeIndex].projectionMinLS = cascadeBounds.minLS;
        _cascadeDebugInfos[cascadeIndex].projectionMaxLS = cascadeBounds.maxLS;
        _cascadeDebugInfos[cascadeIndex].minLS = cascadeBounds.minLS;
        _cascadeDebugInfos[cascadeIndex].maxLS = cascadeBounds.maxLS;
        _cascadeDebugInfos[cascadeIndex].influenceMinLS = cascadeBounds.influenceMinLS;
        _cascadeDebugInfos[cascadeIndex].influenceMaxLS = cascadeBounds.influenceMaxLS;

        previousSplit = nominalFar;

        // cascade 0의 값을 기존 단일 shadow 대표값으로 저장
        if (cascadeIndex == 0u)
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

HRESULT Shadow::Bind_TransformStateMatrix(const Shared<ShaderCom>& shader, const char* viewName, const char* projName)
{
    CHECK_NULL(shader, E_FAIL);
    CHECK_FAILED(shader->Bind_Matrix(viewName, &_viewMatrix), E_FAIL);
    CHECK_FAILED(shader->Bind_Matrix(projName, &_projMatrix), E_FAIL);

    return S_OK;
}

HRESULT Shadow::Bind_ShadowCB(const Shared<ShaderCom>& shader)
{
    CHECK_NULL(shader, E_FAIL);

    const ShadowCB shadowCB = Build_ShadowCB();
    return shader->Bind_CBufferData(shadowCB);
}

ShadowCB Shadow::Build_ShadowCB() const
{
    ShadowCB shadowCB{};

    shadowCB.lightViewMatrix = _viewMatrix;
    shadowCB.lightProjMatrix = _projMatrix;

    for (uint32 cascadeIndex = 0; cascadeIndex < CascadeCount; ++cascadeIndex)
    {
        shadowCB.cascadeLightViewMatrices[cascadeIndex] =
            _hasRenderedCascade[cascadeIndex] ? _renderedCascadeViewMatrices[cascadeIndex] : _cascadeViewMatrices[cascadeIndex];
        shadowCB.cascadeLightProjMatrices[cascadeIndex] =
            _hasRenderedCascade[cascadeIndex] ? _renderedCascadeProjMatrices[cascadeIndex] : _cascadeProjMatrices[cascadeIndex];
    }

    shadowCB.cascadeSplits = Vec4(
        _hasRenderedCascade[0] ? _renderedCascadeSplits[0] : _cascadeSplits[0],
        _hasRenderedCascade[1] ? _renderedCascadeSplits[1] : _cascadeSplits[1],
        _hasRenderedCascade[2] ? _renderedCascadeSplits[2] : _cascadeSplits[2],
        _hasRenderedCascade[3] ? _renderedCascadeSplits[3] : _cascadeSplits[3]);

    shadowCB.cascadeBlendDistance = _desc.cascadeBlendDistance;
    shadowCB.lightFar = _desc.zf;
    shadowCB.bias = _desc.bias;
    shadowCB.intensity = _desc.enabled ? _desc.intensity : 0.f;
    shadowCB.cascadeCount = CascadeCount;
    shadowCB.activeCascadeIndex = _activeCascadeIndex;

    return shadowCB;
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

HRESULT Shadow::Commit_RenderedCascade(uint32 cascadeIndex)
{
    if (cascadeIndex >= CascadeCount)
        return E_FAIL;

    _renderedCascadeViewMatrices[cascadeIndex] = _cascadeViewMatrices[cascadeIndex];
    _renderedCascadeProjMatrices[cascadeIndex] = _cascadeProjMatrices[cascadeIndex];
    _renderedCascadeSplits[cascadeIndex] = _cascadeSplits[cascadeIndex];
    _hasRenderedCascade[cascadeIndex] = true;

    return S_OK;
}

bool Shadow::Consume_ForceRenderAllCascades()
{
    const bool forceRenderAll = _forceRenderAllCascades;
    _forceRenderAllCascades = false;
    return forceRenderAll;
}

HRESULT Shadow::Bind_ShadowLightDesc(const Shared<ShaderCom>& shader, const char* lightFarName, const char* biasName, const char* intensityName)
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
    _forceRenderAllCascades = true;

    for (uint32 cascadeIndex = 0; cascadeIndex < CascadeCount; ++cascadeIndex)
    {
        _cascadeViewMatrices[cascadeIndex] = Matrix::Identity;
        _cascadeProjMatrices[cascadeIndex] = Matrix::Identity;
        _renderedCascadeViewMatrices[cascadeIndex] = Matrix::Identity;
        _renderedCascadeProjMatrices[cascadeIndex] = Matrix::Identity;
        _cascadeInfluenceVolumes[cascadeIndex] = {};
        _cascadeSplits[cascadeIndex] = 0.f;
        _renderedCascadeSplits[cascadeIndex] = 0.f;
        _cascadeDebugInfos[cascadeIndex] = {};
        _hasRenderedCascade[cascadeIndex] = false;
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

const Matrix* Shadow::Get_CascadeViewMatrices() const
{
    return _cascadeViewMatrices;
}

const Matrix* Shadow::Get_CascadeProjMatrices() const
{
    return _cascadeProjMatrices;
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

const ShadowCascadeDebugInfo* Shadow::Get_CascadeDebugInfos() const
{
    return _cascadeDebugInfos;
}

const ShadowCascadeInfluenceVolume* Shadow::Get_CascadeInfluenceVolumes() const
{
    return _cascadeInfluenceVolumes;
}

// desc 하나를 받아 어떤 범위를 shadow에 쓸지를 세 개의 프리셋으로 정한다.
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

// 현재 프리셋 값으로 덮는다.
void Shadow::Apply_ActiveCascadePreset(ShadowLightDesc& desc) const
{
    const ShadowCascadePreset& preset = Get_CascadePreset(_cascadePresets, _activeCascadeLevel);
    desc.focusNear = preset.focusNear;
    desc.focusFar = preset.focusFar;
    desc.cascadeSplitLambda = preset.cascadeSplitLambda;
    desc.cascadeBlendDistance = preset.cascadeBlendDistance;
}
