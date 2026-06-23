#pragma once

#include "Base.h"
#include "Shader_CBuffer_Define.h"

NS_BEGIN(Engine)

class ENGINE_DLL Environment_Manager final : public Base
{
private:
    Environment_Manager() = default;

public:
    ~Environment_Manager() override = default;

    void Set_FogZones(const FogZoneCB& fogZoneCB);
    const FogZoneCB& Get_FogZones() const;
    FogZoneCB Build_ActiveFogZones(const Vec3& cameraPosition);

    void Set_VolumetricLightShaftZones(const VolumetricLightShaftZoneCB& lightShaftZoneCB);
    const VolumetricLightShaftZoneCB& Get_VolumetricLightShaftZones() const;
    VolumetricLightShaftZoneCB Build_ActiveVolumetricLightShaftZones(const Vec3& cameraPosition);

    void Set_FogZoneCullingEnabled(bool enabled) { _fogZoneCullingEnabled = enabled; }
    bool Is_FogZoneCullingEnabled() const { return _fogZoneCullingEnabled; }
    void Set_FogZoneCullingDistance(float distance) { _fogZoneCullingDistance = distance < 0.f ? 0.f : distance; }
    float Get_FogZoneCullingDistance() const { return _fogZoneCullingDistance; }

    void Set_VolumetricLightShaftZoneCullingEnabled(bool enabled) { _lightShaftZoneCullingEnabled = enabled; }
    bool Is_VolumetricLightShaftZoneCullingEnabled() const { return _lightShaftZoneCullingEnabled; }
    void Set_VolumetricLightShaftZoneCullingDistance(float distance) { _lightShaftZoneCullingDistance = distance < 0.f ? 0.f : distance; }
    float Get_VolumetricLightShaftZoneCullingDistance() const { return _lightShaftZoneCullingDistance; }

    const vector<uint32>& Get_ActiveFogZoneIndices() const { return _activeFogZoneIndices; }
    const vector<uint32>& Get_ActiveVolumetricLightShaftZoneIndices() const { return _activeLightShaftZoneIndices; }

    static Unique<Environment_Manager> Create();
    void Free() override;

private :
    float DistanceSqToAABB(const Vec3& point, const Vec3& minPoint, const Vec3& maxPoint);
    float DistanceSqToLocalAABB(const Vec3& point, const Matrix& worldToLocal, const Vec3& halfExtents);


private:
    FogZoneCB                       _fogZoneCB{};
    VolumetricLightShaftZoneCB      _lightShaftZoneCB{};
    bool                            _fogZoneCullingEnabled = true;
    bool                            _lightShaftZoneCullingEnabled = true;
    float                           _fogZoneCullingDistance = 100.f;
    float                           _lightShaftZoneCullingDistance = 20.f;
    vector<uint32>                  _activeFogZoneIndices{};
    vector<uint32>                  _activeLightShaftZoneIndices{};
};

NS_END
