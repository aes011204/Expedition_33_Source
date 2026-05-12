#pragma once
#include "MapEditorDefines.h"
#include "GameObject.h"

NS_BEGIN(Engine)
class ModelCom;
class ShaderCom;
NS_END

NS_BEGIN(MapEditor)

class MEStaticMeshActor final : public GameObject
{
public:
    MEStaticMeshActor(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MEStaticMeshActor(const MEStaticMeshActor& prototype);
    virtual ~MEStaticMeshActor() = default;

public:
    Shared<ModelCom> GetModel() const { return _model; }
    void Set_Model(Shared<ModelCom> model);
    void Set_CameraFar(float camfar) { _cameraFar = camfar; }

public:
    virtual HRESULT Initialize(void* arg) override;
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;

    HRESULT RenderID(const Matrix& worldMatrix, uint32 id);

private:
    Shared<ModelCom> _model;
    Shared<ShaderCom> _shader;

    float _cameraFar = 1000.f;

private:
    HRESULT Ready_Components();
    HRESULT Bind_ShaderResources();

public:
    static Shared<MEStaticMeshActor> Create(ComPtr<Device> device, ComPtr<Context> context);
    virtual Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
