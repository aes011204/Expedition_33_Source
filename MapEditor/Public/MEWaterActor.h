#pragma once
#include "MapEditorDefines.h"
#include "GameObject.h"
#include "Shader_CBuffer_Define.h"
#include "VIBuffer_Landscape.h"

NS_BEGIN(Engine)
class ShaderCom;
NS_END

NS_BEGIN(MapEditor)

class MEWaterActor final : public GameObject
{
public:
    MEWaterActor(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual ~MEWaterActor() = default;

public:
    void Set_CameraInfo(float far, const Vec4& camPos) { _cameraFar = far; _cameraPos = camPos; }
    void Set_WaterParams(const WaterCB& params) { _waterCB = params; }
    void Set_LargeWaveNormalSRV(ID3D11ShaderResourceView* srv) { _largeWaveNormalSRV = srv; }
    void Set_SmallWaveNormalSRV(ID3D11ShaderResourceView* srv) { _smallWaveNormalSRV = srv; }
    void Set_SeafoamSRV(ID3D11ShaderResourceView* srv) { _seafoamSRV = srv; }
    void Set_DepthSRV(ID3D11ShaderResourceView* srv) { _depthSRV = srv; }
    void Set_CubemapSRV(ID3D11ShaderResourceView* srv) { _cubemapSRV = srv; }

public:
    virtual HRESULT Initialize(const VIBuffer_Landscape::LAND_DESC& desc);
    virtual void Update(float timeDelta) override;
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;

private:
    Shared<ShaderCom> _shader;
    Shared<VIBuffer_Landscape> _vibuffer;

    WaterCB _waterCB = {};
    float _cameraFar = 1000.f;
    Vec4 _cameraPos = {};
    float _lodDist[2] = { 50.f, 150.f };

    ComPtr<ID3D11ShaderResourceView> _largeWaveNormalSRV;
    ComPtr<ID3D11ShaderResourceView> _smallWaveNormalSRV;
    ComPtr<ID3D11ShaderResourceView> _seafoamSRV;
    ComPtr<ID3D11ShaderResourceView> _depthSRV;
    ComPtr<ID3D11ShaderResourceView> _cubemapSRV;

private:
    HRESULT Bind_ShaderResources();

public:
    static Shared<MEWaterActor> Create(ComPtr<Device> device, ComPtr<Context> context, const VIBuffer_Landscape::LAND_DESC& desc);
    void Free() override;
};

NS_END
