#include "MapInstancedMesh.h"
#include "GameInstance.h"
#include "ComputeShaderCom.h"
#include "ComputeStructuredBuffer.h"
#include "InstanceCullParams.h"

IMPLEMENT_REFLECTION(MapInstancedMesh)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "MapInstancedMesh";
    info.category = "World";
    return true;
}

MapInstancedMesh::MapInstancedMesh(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

MapInstancedMesh::MapInstancedMesh(const MapInstancedMesh& prototype)
    : GameObject{ prototype }
{
}

HRESULT MapInstancedMesh::Initialize_Prototype()
{
    return S_OK;
}

HRESULT MapInstancedMesh::Initialize(void* arg)
{
    if (FAILED(__super::Initialize(arg)))
    {
        return E_FAIL;
    }

    auto pDesc = static_cast<MAPINSTANCEDMESH_DESC*>(arg);

    if (FAILED(Ready_Components(pDesc->modelPath, pDesc->model)))
    {
        return E_FAIL;
    }

    vector<Matrix> matrices;
    matrices.reserve(pDesc->transforms.size());
    for (auto& t : pDesc->transforms)
    {
        Matrix world = Matrix::CreateScale(t._scale)
            * Matrix::CreateFromQuaternion(Quat(t._rotation.x, t._rotation.y, t._rotation.z, t._rotation.w))
            * Matrix::CreateTranslation(t._position);
        matrices.push_back(world);
    }

    if (FAILED(CreateInstanceBuffer(matrices)))
    {
        return E_FAIL;
    }

    _cullShader = ComputeShaderCom::Create(_device, _context,
        L"../../../Client/Bin/ShaderFiles/Compute/Shader_Compute_InstanceCull.hlsl",
        "CS_Main");

    CreateCullBuffers(matrices, pDesc->transforms);

    return S_OK;
}

void MapInstancedMesh::Priority_Update(float timeDelta)
{
}

void MapInstancedMesh::Update(float timeDelta)
{
}

void MapInstancedMesh::Late_Update(float timeDelta)
{
    if (_instanceCount == 0)
    {
        return;
    }

    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

HRESULT MapInstancedMesh::Render()
{
    if (_instanceCount == 0 || !_model || !_shader)
    {
        return S_OK;
    }

    if (FAILED(Bind_ShaderResources()))
    {
        return E_FAIL;
    }

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes; ++i)
    {
        const Shared<Material> material = _model->Get_Material(i);
        if (material == nullptr)
        {
            continue;
        }

        if (FAILED(_shader->Bind_MaterialTextures(*material)))
        {
            continue;
        }

        if (FAILED(_shader->Bind_CBufferData(material->Get_CB())))
        {
            return E_FAIL;
        }

        const uint32 passIndex = static_cast<uint32>(material->Get_PassIndex());
        if (FAILED(_shader->Begin(passIndex)))
        {
            return E_FAIL;
        }

        if (FAILED(_model->RenderInstanced(i, _instanceVB.Get(), sizeof(Matrix), _instanceCount)))
        {
            return E_FAIL;
        }
    }

    return S_OK;
}

HRESULT MapInstancedMesh::Ready_Components(const wstring& modelPath, Shared<ModelCom> sharedModel)
{
    if (FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxMeshInstanceExact"), _shader)))
    {
        return E_FAIL;
    }

    if (sharedModel)
    {
        _model = sharedModel;
    }
    else
    {
        string pathA = String::ToString(modelPath);
        _model = ModelCom::Create(_device, _context, ModelType::NonAnim, pathA.c_str(), Matrix::CreateScale(0.01f));
        if (!_model)
            return E_FAIL;
    }

    return S_OK;
}

HRESULT MapInstancedMesh::CreateInstanceBuffer(const vector<Matrix>& matrices)
{
    const uint32 count = static_cast<uint32>(matrices.size());
    if (count == 0)
    {
        return S_OK;
    }

    _instanceCapacity = count;

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(Matrix) * _instanceCapacity;
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = matrices.data();

    if (FAILED(_device->CreateBuffer(&bd, &sd, _instanceVB.GetAddressOf())))
    {
        return E_FAIL;
    }

    _instanceCount = count;
    return S_OK;
}

HRESULT MapInstancedMesh::Bind_ShaderResources()
{
    if (FAILED(GAME->Bind_CameraCB(_shader)))
    {
        return E_FAIL;
    }

    Matrix identity = Matrix::Identity;
    if (FAILED(_shader->Bind_Matrix("g_PreTransformMatrix", &identity)))
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT MapInstancedMesh::CreateCullBuffers(const vector<Matrix>& matrices, const vector<InstanceTransform>& transforms)
{
    const uint32 count = static_cast<uint32>(matrices.size());
    if (count == 0 || !_cullShader)
    {
        return S_OK;
    }

    _instanceSrcBuffer = ComputeStructuredBuffer::Create(_device, _context, sizeof(Matrix), count, matrices.data());
    if (!_instanceSrcBuffer)
    {
        return E_FAIL;
    }

    Vec3 localMin{}, localMax{};
    const bool hasBounds = _model && _model->Try_Get_LocalBounds(localMin, localMax);
    const Matrix preTransform = _model ? _model->Get_PreLocalTransformMatrix() : Matrix::Identity;

    vector<Vec4> spheres;
    spheres.reserve(count);

    for (uint32 i = 0; i < count; ++i)
    {
        if (hasBounds)
        {
            BoundingBox localBox;
            BoundingBox::CreateFromPoints(localBox,
                XMVectorSet(localMin.x, localMin.y, localMin.z, 0.f),
                XMVectorSet(localMax.x, localMax.y, localMax.z, 0.f));

            BoundingBox worldBox;
            localBox.Transform(worldBox, preTransform * matrices[i]);

            BoundingSphere sphere;
            BoundingSphere::CreateFromBoundingBox(sphere, worldBox);
            spheres.push_back({ sphere.Center.x, sphere.Center.y, sphere.Center.z, sphere.Radius });
        }
        else
        {
            spheres.push_back({ matrices[i].m[3][0], matrices[i].m[3][1], matrices[i].m[3][2], 500.f });
        }
    }


}

void MapInstancedMesh::DispatchCull()
{

}

array<Vec4, 6> MapInstancedMesh::ExtractFrustumPlanes(const Matrix& viewProj)
{

}

Shared<MapInstancedMesh> MapInstancedMesh::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MapInstancedMesh>(device, context);
    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MapInstancedMesh");
        return nullptr;
    }
    return instance;
}

Shared<GameObject> MapInstancedMesh::Clone(void* arg)
{
    auto clone = make_shared<MapInstancedMesh>(*this);
    if (FAILED(clone->Initialize(arg)))
    {
        return nullptr;
    }
    return clone;
}

void MapInstancedMesh::Free()
{
    __super::Free();
}
