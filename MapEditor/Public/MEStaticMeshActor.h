#pragma once
#include "MapEditorDefines.h"

NS_BEGIN(Engine)
class Model;
class Shader;
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
    Shared<Model> _model;
    Shared<Shader> _shader;

public:
    static Shared<MEStaticMeshActor> Create(
        ComPtr<Device> device,
        ComPtr<Context> context,
        const wstring& modelPath,
        Shared<Shader> shader);
};

NS_END
