#include "SkeletalMesh.h"

#include "AnimCompute_Types.h"
#include "Bone.h"
#include "ShaderCom.h"

NS_BEGIN(Engine)

SkeletalMesh::SkeletalMesh(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Mesh{ device, context } {}

SkeletalMesh::SkeletalMesh(const SkeletalMesh& prototype)
    : Mesh{ prototype } {}

HRESULT SkeletalMesh::Initialize_Prototype(const ModelMeshData& meshData, const Matrix& preTransformMatrix)
{
    _name = meshData.name;
    _materialIndex = meshData.materialIndex;
    _preTransformMatrix = preTransformMatrix;

    _numVertexBuffers = 1;
    _numVertices = meshData.numVertices;
    _numIndices = meshData.numIndices;
    _vertexStride = sizeof(VTXANIMMESH);
    _indexStride = 4;
    _primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    _numBones = meshData.numBoneRefs;
    if (_numBones > 0 && meshData.boneIndices && meshData.offsetMatrices)
    {
        uint32* boneIndices = static_cast<uint32*>(meshData.boneIndices);
        Matrix* offsets = static_cast<Matrix*>(meshData.offsetMatrices);
        _boneIndices.assign(boneIndices, boneIndices + _numBones);
        _offsetMatrices.assign(offsets, offsets + _numBones);
    }

    /* Vertex Buffer */
    VTXANIMMESH* verticesData = static_cast<VTXANIMMESH*>(meshData.vertices);
    VTXANIMMESH* vertices = new VTXANIMMESH[_numVertices];

    _sourceVertices.resize(_numVertices);

    for (uint32 i = 0; i < _numVertices; ++i)
    {
        vertices[i] = verticesData[i];
        _sourceVertices[i] = verticesData[i];
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
    D3D11_BUFFER_DESC IndexBufferDesc{};
    IndexBufferDesc.ByteWidth = _indexStride * _numIndices;
    IndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    IndexBufferDesc.StructureByteStride = _indexStride;

    D3D11_SUBRESOURCE_DATA IndexInitialData{};
    IndexInitialData.pSysMem = meshData.indices;
    if (FAILED(_device->CreateBuffer(&IndexBufferDesc, &IndexInitialData, _ib.GetAddressOf())))
        return E_FAIL;

    return S_OK;
}

HRESULT SkeletalMesh::Initialize(void* arg)
{
    return Mesh::Initialize(arg);
}

HRESULT SkeletalMesh::Build_MeshBoneMatrices(const vector<Matrix>& combinedMatrices,
    vector<Matrix>& outBoneMatrices) const
{
    if (_numBones == 0)
    {
        outBoneMatrices.clear();
        return S_OK;
    }

    if (combinedMatrices.empty())
        return E_FAIL;

    outBoneMatrices.resize(_numBones);

    for (uint32 i = 0; i < _numBones; ++i)
    {
        const uint32 sourceBoneIndex = _boneIndices[i];
        if (sourceBoneIndex >= combinedMatrices.size())
            return E_FAIL;

        outBoneMatrices[i] = _offsetMatrices[i] * combinedMatrices[sourceBoneIndex];
    }

    return S_OK;
}

HRESULT SkeletalMesh::Build_ComputeMeshBoneRefs(vector<AnimComputeMeshBoneRef>& outRefs) const
{
    outRefs.resize(_boneIndices.size());

    for (size_t i = 0; i < _boneIndices.size(); ++i)
    {
        outRefs[i].sourceBoneIndex = _boneIndices[i];
        outRefs[i].offsetMatrix = _offsetMatrices[i];
    }

    return S_OK;
}

HRESULT SkeletalMesh::Bind_BoneMatrices(ShaderCom* shader, const char* constantName, const vector<Shared<Bone>>& bones)
{
    for (uint32 i = 0; i < _numBones; ++i)
    {
        _boneMatrices[i] = _offsetMatrices[i] * *bones[_boneIndices[i]]->Get_CombinedTransformationMatrixPtr();
    }

    return shader->Bind_Matrices(constantName, _boneMatrices, _numBones);
}

HRESULT SkeletalMesh::Bind_BoneMatrices(ShaderCom* shader, const char* constantName,
    const vector<Matrix>& combinedMatrices)
{
    vector<Matrix> meshBoneMatrices;
    CHECK_FAILED(Build_MeshBoneMatrices(combinedMatrices, meshBoneMatrices), E_FAIL);

    if (meshBoneMatrices.empty())
        return S_OK;

    return shader->Bind_Matrices(
        constantName,
        meshBoneMatrices.data(),
        static_cast<uint32>(meshBoneMatrices.size()));
}

Shared<SkeletalMesh> SkeletalMesh::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, const ModelMeshData& meshData, const Matrix& preTransformMatrix)
{
    auto prototype = make_shared<SkeletalMesh>(device, context);

    if (FAILED(prototype->Initialize_Prototype(meshData, preTransformMatrix)))
    {
        MSG_BOX("Failed to Created : SkeletalMesh");
        return nullptr;
    }

    return prototype;
}

Shared<Component> SkeletalMesh::Clone(void* arg)
{
    return nullptr;
}

void SkeletalMesh::Free()
{
    Mesh::Free();
}


NS_END
