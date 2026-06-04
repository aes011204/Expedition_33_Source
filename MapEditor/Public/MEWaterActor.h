#pragma once
#include "MapEditorDefines.h"
#include "GameObject.h"
#include "Shader_CBuffer_Define.h"
#include "VIBuffer_Plane.h"

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
    void Set_CameraInfo(float camfar, const Vec4& camPos) { _cameraFar = camfar; _cameraPos = camPos; }
    void Set_WaterParams(const WaterCB& params)
    {
        float t = _waterCB.elapsedTime;
        _waterCB = params;
        _waterCB.elapsedTime = t;
    }
    void Set_LargeWaveNormalSRV(ID3D11ShaderResourceView* srv) { _largeWaveNormalSRV = srv; }
    void Set_SmallWaveNormalSRV(ID3D11ShaderResourceView* srv) { _smallWaveNormalSRV = srv; }
    void Set_SeafoamSRV(ID3D11ShaderResourceView* srv) { _seafoamSRV = srv; }
    void Set_DepthSRV(ID3D11ShaderResourceView* srv) { _depthSRV = srv; }
    void Set_CubemapSRV(ID3D11ShaderResourceView* srv) { _cubemapSRV = srv; }

public:
    virtual HRESULT Initialize(const VIBuffer_Plane::DESC& desc);
    virtual void Update(float timeDelta) override;
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;

private:
    Shared<ShaderCom> _shader;
    Shared<VIBuffer_Plane> _vibuffer;

    WaterCB _waterCB = {};
    float _cameraFar = 1000.f;
    Vec4 _cameraPos = {};

    ComPtr<ID3D11ShaderResourceView> _largeWaveNormalSRV;
    ComPtr<ID3D11ShaderResourceView> _smallWaveNormalSRV;
    ComPtr<ID3D11ShaderResourceView> _seafoamSRV;
    ComPtr<ID3D11ShaderResourceView> _depthSRV;
    ComPtr<ID3D11ShaderResourceView> _cubemapSRV;

private:
    HRESULT Bind_ShaderResources();

public:
    static Shared<MEWaterActor> Create(ComPtr<Device> device, ComPtr<Context> context, const VIBuffer_Plane::DESC& desc);
    virtual Shared<GameObject> Clone(void* arg) override { return nullptr; }
    void Free() override;
};

NS_END
