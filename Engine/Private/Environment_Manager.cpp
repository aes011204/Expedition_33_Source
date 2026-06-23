#include "Environment_Manager.h"

NS_BEGIN(Engine)

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
    _activeFogZoneIndices.clear();

    FogZoneCB activeCB = _fogZoneCB;
    activeCB.aabbCount = 0;
    activeCB.sphereCount = 0;

    const float cullingDistanceSq = _fogZoneCullingDistance * _fogZoneCullingDistance;
    const int32 sourceCount = min(_fogZoneCB.aabbCount, FogZoneCB::MAX_FOG_ZONES);
    for (int32 i = 0; i < sourceCount; ++i)
    {
        const FogZoneEntry& zone = _fogZoneCB.zones[i];
        const bool visible = !_fogZoneCullingEnabled ||
            DistanceSqToAABB(cameraPosition, zone.zoneMin, zone.zoneMax) <= cullingDistanceSq;

        if (!visible)
            continue;

        activeCB.zones[activeCB.aabbCount++] = zone;
        _activeFogZoneIndices.push_back(static_cast<uint32>(i));
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

Unique<Environment_Manager> Environment_Manager::Create()
{
    return Unique<Environment_Manager>(new Environment_Manager());
}

void Environment_Manager::Free()
{
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
