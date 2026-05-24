#pragma once
#include "GameObject.h"
#include "VIBuffer_TerrainUE.h"
#include "MapEditorDefines.h"

NS_BEGIN(Engine)
class ShaderCom;
NS_END

NS_BEGIN(MapEditor)

class METerrainActor final : public GameObject
{
public:
    METerrainActor(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual ~METerrainActor() = default;

public:
    void Set_CameraFar(float camFar) { _cameraFar = camFar; }

public:
    HRESULT Initialize(const VIBuffer_TerrainUE::TerrainDesc& desc);
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;

private:
    Shared<ShaderCom> _shader;
    Shared<VIBuffer_TerrainUE> _vibuffer;
    float _cameraFar = 1000.f;

    ComPtr<ID3D11ShaderResourceView> _whiteSRV;

private:
    HRESULT Bind_ShaderResources();

public:
    static Shared<METerrainActor> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const VIBuffer_TerrainUE::TerrainDesc& desc);
    virtual Shared<GameObject> Clone(void* arg) override { return nullptr; }
    virtual void Free() override;
};

NS_END
