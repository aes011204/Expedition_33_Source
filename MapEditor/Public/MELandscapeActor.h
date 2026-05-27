#pragma once
#include "GameObject.h"
#include "VIBuffer_Landscape.h"
#include "MapEditorDefines.h"

NS_BEGIN(Engine)
class ShaderCom;
NS_END

NS_BEGIN(MapEditor)

class MELandscapeActor final : public GameObject 
{
public:
    MELandscapeActor(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual ~MELandscapeActor() = default;

public:
    void Set_CameraFar(float camFar) { _cameraFar = camFar; }
    Shared<VIBuffer_Landscape> Get_VIBuffer() { return _vibuffer; }

public:
    HRESULT Initialize(const VIBuffer_Landscape::LAND_DESC& desc);
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;

private:
    Shared<ShaderCom> _shader;
    Shared<VIBuffer_Landscape> _vibuffer;
    float _cameraFar = 1000.f;

    ComPtr<ID3D11ShaderResourceView> _whiteSRV;

private:
    HRESULT Bind_ShaderResources();

public:
    static Shared<MELandscapeActor> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const VIBuffer_Landscape::LAND_DESC& desc);
    virtual Shared<GameObject> Clone(void* arg) override { return nullptr; }
    virtual void Free() override;
};

NS_END
