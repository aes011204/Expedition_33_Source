#include "StaticMesh.h"

StaticMesh::StaticMesh(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : VIBuffer{ device, context }
{
}
StaticMesh::StaticMesh(const StaticMesh& prototype)
    : VIBuffer{ prototype }
{
}

HRESULT StaticMesh::Initialize_Prototype(ModelType type, const aiMesh* aiMesh, class Model* model, FXMMATRIX preTransformMatrix)
{
    return S_OK;
}

HRESULT StaticMesh::Initialize(void* arg)
{
    return VIBuffer::Initialize(arg);
}

HRESULT StaticMesh::Bind_BoneMatrices(class Shader* shader, const char* constantName, const vector<Shared<class Bone>>& bones)
{
    return S_OK;
}

HRESULT StaticMesh::Ready_VertexBuffer_For_NonAnim(const aiMesh* aiMesh, FXMMATRIX preTransformMatrix)
{
    return S_OK;
}

HRESULT StaticMesh::Ready_VertexBuffer_For_Anim(const aiMesh* aiMesh, class Model* model)
{
    return S_OK;
}

Shared<StaticMesh> StaticMesh::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, ModelType type, const aiMesh* aiMesh, class Model* model, FXMMATRIX preTransformMatrix)
{
    auto instance = make_shared<StaticMesh>(device, context);

    if (FAILED(instance->Initialize_Prototype(type, aiMesh, model, preTransformMatrix)))
    {
        MSG_BOX("Failed to Created : StaticMesh");
        return nullptr;
    }
    return instance;
}

Shared<Component> StaticMesh::Clone(void* arg)
{
    return nullptr;
}

void StaticMesh::Free()
{
    VIBuffer::Free();
}
