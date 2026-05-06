#pragma once
#include "MapEditorDefines.h"

NS_BEGIN(Engine)
class ModelCom;
class ShaderCom;
NS_END

NS_BEGIN(MapEditor)

class MEStaticMeshActor
{
public:
    MEStaticMeshActor() = default;
    ~MEStaticMeshActor() = default;

public:
    HRESULT Render(const Matrix& worldMatrix);

private:
    Shared<ModelCom> _model;
    Shared<ShaderCom> _shader;

public:
    static Shared<MEStaticMeshActor> Create(
        ComPtr<Device> device,
        ComPtr<Context> context,
        const wstring& modelPath,
        Shared<ShaderCom> shader);

    static Shared<MEStaticMeshActor> Create(Shared<ModelCom> model, Shared<ShaderCom> shader);
};

NS_END
