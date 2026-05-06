#include "StaticMesh.h"
#include "ShaderCom.h"


NS_BEGIN(Engine)

StaticMesh::StaticMesh(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Mesh{ device, context }
{
}
StaticMesh::StaticMesh(const StaticMesh& prototype)
    : Mesh{ prototype }
{
}

HRESULT StaticMesh::Initialize_Prototype(const ModelMeshData& meshData, const Matrix& preTransformMatrix)
{
    strcpy_s(_name, meshData.name);
    _materialIndex = meshData.materialIndex;
    _preTransformMatrix = preTransformMatrix;

    _numVertexBuffers = 1;
    _numVertices = meshData.numVertices;
    _numIndices = meshData.numIndices;
    _vertexStride = sizeof(VTXMESH);
    _indexStride = 4;
    _primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    /* Vertex Buffer */
    VTXMESH* verticesData = static_cast<VTXMESH*>(meshData.vertices);
    VTXMESH* vertices = new VTXMESH[_numVertices];

    for (uint32 i = 0; i < _numVertices; ++i)
    {
        vertices[i].position= XMVector3TransformCoord(XMLoadFloat3(&verticesData[i].position), preTransformMatrix);
        vertices[i].normal = XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&verticesData[i].normal), preTransformMatrix));
        vertices[i].tangent = XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&verticesData[i].tangent), preTransformMatrix));
        vertices[i].binormal = XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&verticesData[i].binormal), preTransformMatrix));
        vertices[i].texcoord = verticesData[i].texcoord;
    }

    D3D11_BUFFER_DESC VertexBufferDesc{};
    VertexBufferDesc.ByteWidth = _vertexStride * _numVertices;
    VertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    VertexBufferDesc.StructureByteStride = _vertexStride;

    D3D11_SUBRESOURCE_DATA VertexInitialData{};
    VertexInitialData.pSysMem = vertices;
    if (FAILED(_device->CreateBuffer(&VertexBufferDesc, &VertexInitialData, _vb.GetAddressOf())))
    {
        Safe_Delete_Array(vertices);
        return E_FAIL;
    }
    Safe_Delete_Array(vertices);

    /* Index Buffer */
    uint32* indices = static_cast<uint32*>(meshData.indices);

    D3D11_BUFFER_DESC IndexBufferDesc{};
    IndexBufferDesc.ByteWidth = _indexStride * _numIndices;
    IndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    IndexBufferDesc.StructureByteStride = _indexStride;

    D3D11_SUBRESOURCE_DATA IndexInitialData{};
    IndexInitialData.pSysMem = indices;
    if (FAILED(_device->CreateBuffer(&IndexBufferDesc, &IndexInitialData, _ib.GetAddressOf())))
        return E_FAIL;

    return S_OK;
}

HRESULT StaticMesh::Initialize(void* arg)
{
    return VIBuffer::Initialize(arg);
}

HRESULT StaticMesh::Bind_BoneMatrices(class ShaderCom* shader, const char* constantName, const vector<Shared<class Bone>>& bones)
{
    Matrix finalMat = Matrix::Identity * _preTransformMatrix;
    return shader->Bind_Matrix(constantName, &finalMat);
}

Shared<StaticMesh> StaticMesh::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const ModelMeshData& meshData, const Matrix& preTransformMatrix)
{
    auto instance = make_shared<StaticMesh>(device, context);

    if (FAILED(instance->Initialize_Prototype(meshData, preTransformMatrix)))
    {
        LOG_ERROR("Failed to Created : StaticMesh");
        return nullptr;
    }
    return instance;
}

Shared<Component> StaticMesh::Clone(void* arg) { return nullptr; }

void StaticMesh::Free()
{
    Mesh::Free();
}

NS_END
