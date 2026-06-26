#include "Environment_Manager.h"

#include "GameInstance.h"
#include "LightingPipeline.h"
#include "ShaderCom.h"
#include "Texture.h"

NS_BEGIN(Engine)

Environment_Manager::Environment_Manager(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device{ device }
    , _context{ context }
{
}

HRESULT Environment_Manager::Change_IBL(const wchar_t* irradiancePath, const wchar_t* prefilterPath)
{
    Shared<Texture> irradianceTexture = Texture::Create(_device, _context, irradiancePath, 1);
    if (irradianceTexture == nullptr)
        return E_FAIL;

    Shared<Texture> prefilterTexture = Texture::Create(_device, _context, prefilterPath, 1);
    if (prefilterTexture == nullptr)
        return E_FAIL;

    _iblIrradianceTexture = irradianceTexture;
    _iblPrefilterTexture = prefilterTexture;

    return S_OK;
}

HRESULT Environment_Manager::Bind_IBLTextures(ShaderCom* shader) const
{
    if (shader == nullptr)
        return E_FAIL;

    if (_iblIrradianceTexture != nullptr)
    {
        if (FAILED(_iblIrradianceTexture->Bind_ShaderResourceView(shader, "g_IrradianceMap", 0)))
            return E_FAIL;
    }
    else if (FAILED(shader->Bind_SRV("g_IrradianceMap", nullptr)))
    {
        return E_FAIL;
    }

    if (_iblPrefilterTexture != nullptr)
    {
        if (FAILED(_iblPrefilterTexture->Bind_ShaderResourceView(shader, "g_PrefilterMap", 0)))
            return E_FAIL;
    }
    else if (FAILED(shader->Bind_SRV("g_PrefilterMap", nullptr)))
    {
        return E_FAIL;
    }

    if (_iblBrdfLutTexture != nullptr)
    {
        if (FAILED(_iblBrdfLutTexture->Bind_ShaderResourceView(shader, "g_BRDFLUT", 0)))
            return E_FAIL;
    }
    else if (FAILED(shader->Bind_SRV("g_BRDFLUT", nullptr)))
    {
        return E_FAIL;
    }

    return S_OK;
}

void Environment_Manager::Begin_SoulLikeIBLOverride()
{
    if (_soulLikeIBLOverrideActive)
        return;

    Shared<LightingPipeline> lightingPipeline = GAME->Get_LightingPipeline();
    if (lightingPipeline == nullptr)
        return;

    _savedSoulLikeDeferredLightingCB = lightingPipeline->Get_DeferredLightingParams();
    _soulLikeIBLOverrideActive = true;

    DeferredLightingCB soulLikeLighting = _savedSoulLikeDeferredLightingCB;
    soulLikeLighting.iblEnabled = 1u;
    soulLikeLighting.iblIntensity = 0.70f;
    soulLikeLighting.iblDiffuseIntensity = 0.70f;
    soulLikeLighting.iblSpecularIntensity = 0.70f;
    soulLikeLighting.iblPrefilterMaxMip = 8.0f;
    lightingPipeline->Set_DeferredLightingParams(soulLikeLighting);
}

void Environment_Manager::End_SoulLikeIBLOverride()
{
    if (!_soulLikeIBLOverrideActive)
        return;

    Shared<LightingPipeline> lightingPipeline = GAME->Get_LightingPipeline();
    if (lightingPipeline != nullptr)
        lightingPipeline->Set_DeferredLightingParams(_savedSoulLikeDeferredLightingCB);

    _savedSoulLikeDeferredLightingCB = DeferredLightingCB{};
    _soulLikeIBLOverrideActive = false;
}

void Environment_Manager::Set_FogZones(const FogZoneCB& fogZoneCB)
{
    _fogZoneCB = fogZoneCB;
}

const FogZoneCB& Environment_Manager::Get_FogZones() const
{
    return _fogZoneCB;
}

FogZoneCB Environment_Manager::Build_ActiveFogZones(const Vec3& cameraPosition)
{
    _activeFogAABBZoneIndices.clear();
    _activeFogOBBZoneIndices.clear();

    FogZoneCB activeCB = _fogZoneCB;
    activeCB.aabbCount = 0;
    activeCB.obbCount = 0;

    const float cullingDistanceSq = _fogZoneCullingDistance * _fogZoneCullingDistance;
    const int32 sourceAABBCount = min(_fogZoneCB.aabbCount, FogZoneCB::MAX_FOG_ZONES);
    for (int32 i = 0; i < sourceAABBCount; ++i)
    {
        const FogZoneEntry& zone = _fogZoneCB.aabbZones[i];
        const bool visible = !_fogZoneCullingEnabled ||
            DistanceSqToAABB(cameraPosition, zone.zoneMin, zone.zoneMax) <= cullingDistanceSq;

        if (!visible)
            continue;

        activeCB.aabbZones[activeCB.aabbCount++] = zone;
        _activeFogAABBZoneIndices.push_back(static_cast<uint32>(i));
    }

    const int32 sourceOBBCount = min(_fogZoneCB.obbCount, FogZoneCB::MAX_FOG_OBB_ZONES);
    for (int32 i = 0; i < sourceOBBCount; ++i)
    {
        const FogOBBZoneEntry& zone = _fogZoneCB.obbZones[i];
        const bool visible = !_fogZoneCullingEnabled ||
            DistanceSqToLocalAABB(cameraPosition, zone.worldToLocal, zone.halfExtents) <= cullingDistanceSq;

        if (!visible)
            continue;

        activeCB.obbZones[activeCB.obbCount++] = zone;
        _activeFogOBBZoneIndices.push_back(static_cast<uint32>(i));
    }

    return activeCB;
}

void Environment_Manager::Set_VolumetricLightShaftZones(const VolumetricLightShaftZoneCB& lightShaftZoneCB)
{
    _lightShaftZoneCB = lightShaftZoneCB;
}

const VolumetricLightShaftZoneCB& Environment_Manager::Get_VolumetricLightShaftZones() const
{
    return _lightShaftZoneCB;
}

VolumetricLightShaftZoneCB Environment_Manager::Build_ActiveVolumetricLightShaftZones(const Vec3& cameraPosition)
{
    _activeLightShaftZoneIndices.clear();

    VolumetricLightShaftZoneCB activeCB{};
    const float cullingDistanceSq = _lightShaftZoneCullingDistance * _lightShaftZoneCullingDistance;
    const uint32 sourceCount = min(_lightShaftZoneCB.zoneCount, VolumetricLightShaftZoneCB::MAX_LIGHT_SHAFT_ZONES);
    for (uint32 i = 0; i < sourceCount; ++i)
    {
        const VolumetricLightShaftZoneEntry& zone = _lightShaftZoneCB.zones[i];
        const bool visible = !_lightShaftZoneCullingEnabled ||
            DistanceSqToLocalAABB(cameraPosition, zone.worldToLocal, zone.halfExtents) <= cullingDistanceSq;

        if (!visible)
            continue;

        activeCB.zones[activeCB.zoneCount++] = zone;
        _activeLightShaftZoneIndices.push_back(i);
    }

    return activeCB;
}

Unique<Environment_Manager> Environment_Manager::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    Unique<Environment_Manager> instance{ new Environment_Manager(device, context) };

    (void)instance->Change_IBL(
        L"../../../Client/Bin/Resources/Textures/Shader/IBL/irradiance_sunset.dds",
        L"../../../Client/Bin/Resources/Textures/Shader/IBL/perfilter_sunset.dds");

    instance->_iblBrdfLutTexture = Texture::Create(
        device,
        context,
        L"../../../Client/Bin/Resources/Textures/Shader/IBL/BRDF_LUT.dds",
        1);

    return instance;
}

void Environment_Manager::Free()
{
    _iblIrradianceTexture.reset();
    _iblPrefilterTexture.reset();
    _iblBrdfLutTexture.reset();

    __super::Free();
}

float Environment_Manager::DistanceSqToAABB(const Vec3& point, const Vec3& minPoint, const Vec3& maxPoint)
{
    const float dx = point.x < minPoint.x ? minPoint.x - point.x : (point.x > maxPoint.x ? point.x - maxPoint.x : 0.f);
    const float dy = point.y < minPoint.y ? minPoint.y - point.y : (point.y > maxPoint.y ? point.y - maxPoint.y : 0.f);
    const float dz = point.z < minPoint.z ? minPoint.z - point.z : (point.z > maxPoint.z ? point.z - maxPoint.z : 0.f);
    return dx * dx + dy * dy + dz * dz;
}

float Environment_Manager::DistanceSqToLocalAABB(const Vec3& point, const Matrix& worldToLocal, const Vec3& halfExtents)
{
    const Vec3 localPoint = Vec3::Transform(point, worldToLocal);
    return DistanceSqToAABB(localPoint, Vec3(-halfExtents.x, -halfExtents.y, -halfExtents.z), halfExtents);
}

NS_END
