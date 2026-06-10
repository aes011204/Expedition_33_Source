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

        _cascadeSplits[cascadeIndex] = desc.zf;
        _cascadeDebugInfos[cascadeIndex] = {};
        _cascadeInfluenceVolumes[cascadeIndex] = {};
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
    const bool dumpShadowCascadeTrace = GAME->KeyDown(KEY_TYPE::T) && GAME->KeyDown(KEY_TYPE::H);

    // cascade 구간 쪼개기
    float previousSplit = nearZ;
    const float lambda = min(1.f, max(0.f, _desc.cascadeSplitLambda));
    const float blendDistance = max(0.f, _desc.cascadeBlendDistance);

    if (dumpShadowCascadeTrace)
    {
        LOG_WARN(
            "[ShadowTrace] cameraNear={} cameraFar={} focusNear={} focusFar={} appliedNear={} appliedFar={} fovY={} aspect={} lambda={} blend={} coverageRadius={} lightDir=({:.3f},{:.3f},{:.3f})",
            camera->Get_Near(),
            camera->Get_Far(),
            _desc.focusNear,
            _desc.focusFar,
            nearZ,
            farZ,
            fovY,
            aspect,
            lambda,
            blendDistance,
            coverageRadius,
            dir.x, dir.y, dir.z);
    }

    for (uint32 cascadeIndex = 0; cascadeIndex < CascadeCount; ++cascadeIndex)
    {
        const float nominalNear = previousSplit;
        const float nominalFar = (cascadeIndex + 1u == CascadeCount)
            ? farZ
            : Compute_CascadeSplit(cascadeIndex, CascadeCount, nearZ, farZ, lambda);
        const float expandedNear = max(nearZ, nominalNear - blendDistance);
        const float expandedFar = min(farZ, max(expandedNear + 0.001f, nominalFar + blendDistance));

        // 현재 cascade가 담당할 깊이 구간, 월드의 코너를 구한다.
        Vec3 sliceCorners[8]{};
        Build_FrustumCorners(cameraWorld, expandedNear, expandedFar, tanHalfFovY, aspect, sliceCorners);

        // 해당 코너 구간을 빛 공간에서 감쌀 shadow camera 만든다.
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
        _cascadeDebugInfos[cascadeIndex].expandedNear = expandedNear;
        _cascadeDebugInfos[cascadeIndex].expandedFar = expandedFar;
        _cascadeDebugInfos[cascadeIndex].minLS = cascadeBounds.minLS;
        _cascadeDebugInfos[cascadeIndex].maxLS = cascadeBounds.maxLS;
        _cascadeDebugInfos[cascadeIndex].influenceMinLS = cascadeBounds.influenceMinLS;
        _cascadeDebugInfos[cascadeIndex].influenceMaxLS = cascadeBounds.influenceMaxLS;
        previousSplit = nominalFar;

        if (dumpShadowCascadeTrace)
        {
            LOG_WARN(
                "[ShadowTrace][C{}] nominalNear={} nominalFar={} expandedNear={} expandedFar={} centerWS=({:.3f},{:.3f},{:.3f}) eyeWS=({:.3f},{:.3f},{:.3f}) centerLS=({:.3f},{:.3f},{:.3f}) radius={}",
                cascadeIndex,
                nominalNear,
                nominalFar,
                expandedNear,
                expandedFar,
                cascadeBounds.centerWS.x, cascadeBounds.centerWS.y, cascadeBounds.centerWS.z,
                cascadeBounds.eyeWS.x, cascadeBounds.eyeWS.y, cascadeBounds.eyeWS.z,
                cascadeBounds.centerLS.x, cascadeBounds.centerLS.y, cascadeBounds.centerLS.z,
                cascadeBounds.stableRadius);
            LOG_WARN(
                "[ShadowTrace][C{}] receiverMinLS=({:.3f},{:.3f},{:.3f}) receiverMaxLS=({:.3f},{:.3f},{:.3f}) projMinLS=({:.3f},{:.3f},{:.3f}) projMaxLS=({:.3f},{:.3f},{:.3f})",
                cascadeIndex,
                cascadeBounds.receiverMinLS.x, cascadeBounds.receiverMinLS.y, cascadeBounds.receiverMinLS.z,
                cascadeBounds.receiverMaxLS.x, cascadeBounds.receiverMaxLS.y, cascadeBounds.receiverMaxLS.z,
                cascadeBounds.minLS.x, cascadeBounds.minLS.y, cascadeBounds.minLS.z,
                cascadeBounds.maxLS.x, cascadeBounds.maxLS.y, cascadeBounds.maxLS.z);
            LOG_WARN(
                "[ShadowTrace][C{}] influenceMinLS=({:.3f},{:.3f},{:.3f}) influenceMaxLS=({:.3f},{:.3f},{:.3f}) sliceUpstream={} coverageUpstream={} coverageRadius={} centerToCoverage={} extrusion={} texel=({:.5f},{:.5f})",
                cascadeIndex,
                cascadeBounds.influenceMinLS.x, cascadeBounds.influenceMinLS.y, cascadeBounds.influenceMinLS.z,
                cascadeBounds.influenceMaxLS.x, cascadeBounds.influenceMaxLS.y, cascadeBounds.influenceMaxLS.z,
                cascadeBounds.sliceMaxUpstream,
                cascadeBounds.coverageMaxUpstream,
                cascadeBounds.coverageRadius,
                cascadeBounds.centerToCoverageDistance,
                cascadeBounds.extrusionDistance,
                cascadeBounds.texelX,
                cascadeBounds.texelY);
        }

        // cascade 구간이 0일 때 대표값으로 저장, 별 의미 없음
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

    shadowCB.cascadeBlendDistance = _desc.cascadeBlendDistance;
    shadowCB.lightFar = _desc.zf;
    shadowCB.bias = _desc.bias;
    shadowCB.intensity = _desc.enabled ? _desc.intensity : 0.f;
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

    for (uint32 cascadeIndex = 0; cascadeIndex < CascadeCount; ++cascadeIndex)
    {
        _cascadeViewMatrices[cascadeIndex] = Matrix::Identity;
        _cascadeProjMatrices[cascadeIndex] = Matrix::Identity;
        _cascadeInfluenceVolumes[cascadeIndex] = {};
        _cascadeSplits[cascadeIndex] = 0.f;
        _cascadeDebugInfos[cascadeIndex] = {};
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
