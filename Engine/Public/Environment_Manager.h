#pragma once

#include "Base.h"
#include "Shader_CBuffer_Define.h"

NS_BEGIN(Engine)

class ShaderCom;
class Texture;

class ENGINE_DLL Environment_Manager final : public Base
{
private:
    Environment_Manager(const ComPtr<Device>& device, const ComPtr<Context>& context);

public:
    ~Environment_Manager() override = default;

    HRESULT Change_IBL(const wchar_t* irradiancePath, const wchar_t* prefilterPath);
    HRESULT Bind_IBLTextures(ShaderCom* shader) const;

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

    const vector<uint32>& Get_ActiveFogAABBZoneIndices() const { return _activeFogAABBZoneIndices; }
    const vector<uint32>& Get_ActiveFogOBBZoneIndices() const { return _activeFogOBBZoneIndices; }
    const vector<uint32>& Get_ActiveVolumetricLightShaftZoneIndices() const { return _activeLightShaftZoneIndices; }

    static Unique<Environment_Manager> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    void Free() override;

private :
    float DistanceSqToAABB(const Vec3& point, const Vec3& minPoint, const Vec3& maxPoint);
    float DistanceSqToLocalAABB(const Vec3& point, const Matrix& worldToLocal, const Vec3& halfExtents);


private:
    ComPtr<Device>                  _device;
    ComPtr<Context>                 _context;
    Shared<Texture>                 _iblIrradianceTexture;
    Shared<Texture>                 _iblPrefilterTexture;
    Shared<Texture>                 _iblBrdfLutTexture;
    FogZoneCB                       _fogZoneCB{};
    VolumetricLightShaftZoneCB      _lightShaftZoneCB{};
    bool                            _fogZoneCullingEnabled = true;
    bool                            _lightShaftZoneCullingEnabled = true;
    float                           _fogZoneCullingDistance = 100.f;
    float                           _lightShaftZoneCullingDistance = 20.f;
    vector<uint32>                  _activeFogAABBZoneIndices{};
    vector<uint32>                  _activeFogOBBZoneIndices{};
    vector<uint32>                  _activeLightShaftZoneIndices{};
};

NS_END
