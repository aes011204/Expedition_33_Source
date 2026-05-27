#include "AnimComputeRuntime.h"
#include "ModelCom.h"
#include "Animation.h"
#include "Mesh.h"
#include "SkeletalMesh.h"
#include "ComputeShaderCom.h"
#include "ComputeStructuredBuffer.h"
#include "GameInstance.h"
#include "Bone.h"
#include "Debug_Profile.h"

AnimComputeRuntime::AnimComputeRuntime(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device(device), _context(context)
{
}

HRESULT AnimComputeRuntime::Initialize(const CreateDesc& desc)
{
    if (desc.model == nullptr)
        return E_INVALIDARG;

    _model = desc.model;

    _computeBoneRemapShaderCom      = desc.boneRemapShader;
    _computeSkinningShaderCom       = desc.skinningShader;

    if (_computeBoneRemapShaderCom      == nullptr ||
        _computeSkinningShaderCom       == nullptr)
    {
        LOG_ERROR("ComputeShader 생성 실패");

        return E_FAIL;
    }

    Reset();

    CHECK_FAILED(_model->Ensure_AnimationComputeSource(), E_FAIL);
    _skinningDirty = true;

    return S_OK;
}

HRESULT AnimComputeRuntime::Update_Frame(float timeDelta)
{
    ENGINE_PROFILE_SCOPE_DETAIL("AnimCompute::UpdateFrame");

    CHECK_NULL(_model, E_FAIL);

    AnimComputeFrameContext context{};
    Build_FrameContext(timeDelta, context);

    {
        ENGINE_PROFILE_SCOPE_DETAIL("AnimCompute::SamplePose");
        _model->Step_SampleAnimationPose(context.modelContext);
    }
    _model->Evaluate_AnimationModifiers(timeDelta);
    _model->Step_RebuildCombinedBones();

    CHECK_FAILED(_model->Ensure_AnimationComputeSource(), E_FAIL); // source cache가 없으면 생성
    CHECK_FAILED(Ensure_RuntimeBuffers(), E_FAIL);
    CHECK_FAILED(Build_ComputeCombinedBoneCache(), E_FAIL);
    CHECK_FAILED(Update_GlobalCombinedBoneBuffer_FromCPU(), E_FAIL);
    CHECK_FAILED(Step_BoneRemap(context), E_FAIL);      // global combined -> mesh-local bone matrices
    CHECK_FAILED(Finalize_ModelFrame(context), E_FAIL); // notify / root motion / dirty는 ModelCom 재사용

    _skinningDirty = true;

    return S_OK;
}

HRESULT AnimComputeRuntime::Prepare_ComputeSkinning()
{
    if (_model == nullptr)
        return E_FAIL;

    if (!_model->_isSkeletal)
        return S_OK;

    if (_computeSkinningShaderCom == nullptr)
        return E_FAIL;

    CHECK_FAILED(Ensure_RuntimeBoneBuffers(), E_FAIL);
    CHECK_FAILED(Ensure_RuntimeSkinningBuffers(), E_FAIL);
    CHECK_FAILED(Ensure_RuntimeSkinningConstantBuffer(), E_FAIL);

    if (!_skinningDirty)
        return S_OK;

    for (uint32 meshIndex = 0; meshIndex < _model->_meshes.size(); ++meshIndex)
    {
        const Shared<ComputeStructuredBuffer>& sourceVertexBuffer = _meshSourceVertexBuffers[meshIndex];
        const Shared<ComputeStructuredBuffer>& meshBoneMatrixBuffer = _meshBoneMatrixBuffers[meshIndex];
        const Shared<ComputeStructuredBuffer>& computedVertexBuffer = _meshComputedVertexBuffers[meshIndex];

        if (sourceVertexBuffer == nullptr || meshBoneMatrixBuffer == nullptr || computedVertexBuffer == nullptr)
            continue;

        const uint32 vertexCount = computedVertexBuffer->Get_ElementCount();
        if (vertexCount == 0)
            continue;

        ComputeSkinningCBData cbData{};
        cbData.vertexCount = vertexCount;
        _context->UpdateSubresource(_computeSkinningConstantBuffer.Get(), 0, nullptr, &cbData, 0, 0);

        CHECK_FAILED(_computeSkinningShaderCom->Bind_SRV(0, sourceVertexBuffer->Get_SRV()), E_FAIL);
        CHECK_FAILED(_computeSkinningShaderCom->Bind_SRV(1, meshBoneMatrixBuffer->Get_SRV()), E_FAIL);
        CHECK_FAILED(_computeSkinningShaderCom->Bind_UAV(0, computedVertexBuffer->Get_UAV()), E_FAIL);
        CHECK_FAILED(_computeSkinningShaderCom->Bind_ConstantBuffer(0, _computeSkinningConstantBuffer.Get()), E_FAIL);

        CHECK_FAILED(_computeSkinningShaderCom->Dispatch((vertexCount + 63) / 64, 1, 1), E_FAIL);

        if (meshIndex >= _meshComputedDrawVertexBuffers.size() ||
            _meshComputedDrawVertexBuffers[meshIndex] == nullptr)
            return E_FAIL;

        _context->CopyResource(
            _meshComputedDrawVertexBuffers[meshIndex].Get(),
            computedVertexBuffer->Get_Buffer());
    }

    _skinningDirty = false;

    return S_OK;
}

HRESULT AnimComputeRuntime::Prepare_BoneMatrixBuffers()
{
    if (_model == nullptr)
        return E_FAIL;

    if (!_model->_isSkeletal || _model->_numBones == 0)
        return S_OK;

    return Ensure_RuntimeBoneBuffers();
}

HRESULT AnimComputeRuntime::Bind_PreparedBoneMatrices(ShaderCom* shader, const char* constantName, uint32 meshIndex)
{
    if (_model == nullptr || shader == nullptr || constantName == nullptr)
        return E_FAIL;

    if (!_model->_isSkeletal || _model->_numBones == 0)
        return S_OK;

    if (meshIndex >= _model->_meshes.size())
        return E_FAIL;

    if (meshIndex >= _meshBoneMatrixBuffers.size())
        return E_FAIL;

    const Shared<ComputeStructuredBuffer>& meshBoneMatrixBuffer = _meshBoneMatrixBuffers[meshIndex];
    if (meshBoneMatrixBuffer == nullptr)
        return S_OK;

    return shader->Bind_SRV(constantName, meshBoneMatrixBuffer->Get_SRV()); // 준비된 bone buffer bind
}

HRESULT AnimComputeRuntime::Render_ComputeSkinned(uint32 meshIndex)
{
    if (_model == nullptr)
        return E_FAIL;

    if (meshIndex >= _model->_meshes.size())
        return E_FAIL;

    if (meshIndex >= _meshComputedDrawVertexBuffers.size())
        return E_FAIL;

    if (_meshComputedDrawVertexBuffers[meshIndex] == nullptr)
        return E_FAIL;

    return _model->_meshes[meshIndex]->Render_ComputeSkinned(
        _meshComputedDrawVertexBuffers[meshIndex].Get(),
        sizeof(VTXMESH)); // computed 결과 draw VB는 runtime이 들고 있으므로 여기서 바로 렌더한다
}

HRESULT AnimComputeRuntime::Ensure_RuntimeBuffers()
{
    if (_model == nullptr)
        return E_FAIL;

    const uint32 numBones = static_cast<uint32>(_model->_numBones);
    if (numBones == 0)
        return S_OK;

    if (_globalCombinedBoneBuffer == nullptr || _globalCombinedBoneBuffer->Get_ElementCount() < numBones)
    {
        _globalCombinedBoneBuffer = ComputeStructuredBuffer::Create(
            _device, _context, sizeof(Matrix), std::max<uint32>(1u, numBones));
        if (_globalCombinedBoneBuffer == nullptr)
            return E_FAIL;
    }

    return S_OK;
}

HRESULT AnimComputeRuntime::Ensure_RuntimeBoneBuffers()
{
    if (_model == nullptr)
        return E_FAIL;

    // CPU mesh bone matrices 계산은 여전히 Model에서 진행
    CHECK_FAILED(_model->Ensure_MeshBoneMatrices(), E_FAIL);

    const vector<vector<Matrix>>& meshBoneCaches = _model->_meshBoneMatrixCaches;

    if (_meshBoneMatrixBuffers.size() != meshBoneCaches.size())
        _meshBoneMatrixBuffers.resize(meshBoneCaches.size());

    for (uint32 meshIndex = 0; meshIndex < meshBoneCaches.size(); ++meshIndex)
    {
        const vector<Matrix>& meshBoneMatrices = meshBoneCaches[meshIndex];
        if (meshBoneMatrices.empty())
            continue;

        const uint32 matrixCount = static_cast<uint32>(meshBoneMatrices.size());

        if (_meshBoneMatrixBuffers[meshIndex] == nullptr ||
            _meshBoneMatrixBuffers[meshIndex]->Get_ElementCount() < matrixCount)
        {
            // GPU bone matrix buffer 생성 책임을 runtime으로 옮긴다.
            _meshBoneMatrixBuffers[meshIndex] =
                ComputeStructuredBuffer::Create(_device, _context, sizeof(Matrix), matrixCount, meshBoneMatrices.data()); 

            if (_meshBoneMatrixBuffers[meshIndex] == nullptr)
                return E_FAIL;
        }
        else
        {
            // bone matrix structured buffer 갱신도 runtime이 담당한다.
            CHECK_FAILED(
                _meshBoneMatrixBuffers[meshIndex]->Update_Data(meshBoneMatrices.data(), matrixCount), 
                E_FAIL);
        }
    }

    return S_OK;
}

HRESULT AnimComputeRuntime::Ensure_RuntimeSkinningBuffers()
{
    if (_model == nullptr)
        return E_FAIL;

    if (!_model->_isSkeletal)
        return S_OK;

    if (_meshSourceVertexBuffers.size() != _model->_meshes.size())
        _meshSourceVertexBuffers.resize(_model->_meshes.size());

    if (_meshComputedVertexBuffers.size() != _model->_meshes.size())
        _meshComputedVertexBuffers.resize(_model->_meshes.size());

    if (_meshComputedDrawVertexBuffers.size() != _model->_meshes.size())
        _meshComputedDrawVertexBuffers.resize(_model->_meshes.size());

    for (uint32 meshIndex = 0; meshIndex < _model->_meshes.size(); ++meshIndex)
    {
        const Shared<SkeletalMesh> skeletalMesh =
            dynamic_pointer_cast<SkeletalMesh>(_model->_meshes[meshIndex]);

        if (skeletalMesh == nullptr)
            continue;

        const vector<VTXANIMMESH>& sourceVertices = skeletalMesh->Get_SourceVertices();
        if (sourceVertices.empty())
            continue;

        if (_meshSourceVertexBuffers[meshIndex] == nullptr ||
            _meshSourceVertexBuffers[meshIndex]->Get_ElementCount() != sourceVertices.size())
        {
            _meshSourceVertexBuffers[meshIndex] = ComputeStructuredBuffer::Create(
                _device,
                _context,
                sizeof(VTXANIMMESH),
                static_cast<uint32>(sourceVertices.size()),
                sourceVertices.data()); 

            if (_meshSourceVertexBuffers[meshIndex] == nullptr)
                return E_FAIL;
        }

        if (_meshComputedVertexBuffers[meshIndex] == nullptr ||
            _meshComputedVertexBuffers[meshIndex]->Get_ElementCount() != sourceVertices.size())
        {
            _meshComputedVertexBuffers[meshIndex] = ComputeStructuredBuffer::Create(
                _device,
                _context,
                sizeof(VTXMESH),
                static_cast<uint32>(sourceVertices.size())); 

            if (_meshComputedVertexBuffers[meshIndex] == nullptr)
                return E_FAIL;
        }

        if (_meshComputedDrawVertexBuffers[meshIndex] == nullptr)
        {
            D3D11_BUFFER_DESC drawVertexBufferDesc{};
            drawVertexBufferDesc.ByteWidth = sizeof(VTXMESH) * static_cast<uint32>(sourceVertices.size());
            drawVertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
            drawVertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            drawVertexBufferDesc.CPUAccessFlags = 0;
            drawVertexBufferDesc.MiscFlags = 0;
            drawVertexBufferDesc.StructureByteStride = sizeof(VTXMESH);

            CHECK_FAILED(
                _device->CreateBuffer(
                    &drawVertexBufferDesc,
                    nullptr,
                    _meshComputedDrawVertexBuffers[meshIndex].ReleaseAndGetAddressOf()),
                E_FAIL); 
        }
    }

    return S_OK;
}

HRESULT AnimComputeRuntime::Ensure_RuntimeSkinningConstantBuffer()
{
    if (_computeSkinningConstantBuffer != nullptr)
        return S_OK;

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = sizeof(ComputeSkinningCBData);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    CHECK_FAILED(
        _device->CreateBuffer(&desc, nullptr, _computeSkinningConstantBuffer.GetAddressOf()),
        E_FAIL); 

    return S_OK;
}

HRESULT AnimComputeRuntime::Build_ComputeCombinedBoneCache()
{
    if (_model == nullptr || !_model->_isSkeletal)
        return S_OK;
    const uint32 numBones = static_cast<uint32>(_model->_numBones);
    if (numBones == 0)
    {
        _model->Access_CombinedBoneMatrixCache().clear();
        return S_OK;
    }

    vector<Matrix>& combinedBoneCache = _model->Access_CombinedBoneMatrixCache();

    if (combinedBoneCache.size() != numBones)
       combinedBoneCache.resize(numBones, Matrix::Identity);

    for (uint32 boneIndex = 0; boneIndex < numBones; ++boneIndex)
    {
        const Matrix* combinedMatrix = _model->_bones[boneIndex]->Get_CombinedTransformationMatrixPtr();
        combinedBoneCache[boneIndex] = combinedMatrix ? *combinedMatrix : Matrix::Identity;
    }

    return S_OK;
}

HRESULT AnimComputeRuntime::Update_GlobalCombinedBoneBuffer_FromCPU()
{
    if (_model == nullptr || !_model->_isSkeletal)
        return S_OK;

    const vector<Matrix>& combinedBoneCache = _model->Get_CombinedBoneMatrixCache();
    if (combinedBoneCache.empty())
        return S_OK;

    CHECK_NULL(_globalCombinedBoneBuffer, E_FAIL);

    CHECK_FAILED(
        _globalCombinedBoneBuffer->Update_Data(
            combinedBoneCache.data(),
            static_cast<uint32>(combinedBoneCache.size())),
        E_FAIL);

    return S_OK;
}

HRESULT AnimComputeRuntime::Step_BoneRemap(const AnimComputeFrameContext& context)
{
    UNREFERENCED_PARAMETER(context);

    CHECK_NULL(_computeBoneRemapShaderCom, E_FAIL);
    CHECK_NULL(_globalCombinedBoneBuffer, E_FAIL);

    if (_meshBoneMatrixBuffers.size() != _model->_meshes.size())
        _meshBoneMatrixBuffers.resize(_model->_meshes.size());

    for (uint32 meshIndex = 0; meshIndex < _model->_meshes.size(); ++meshIndex)
    {
        const auto& meshBoneRefs = _model->_computeAnimationSource.meshBoneRefs[meshIndex];
        if (meshBoneRefs.empty())
            continue;

        const uint32 matrixCount = static_cast<uint32>(meshBoneRefs.size());

        if (_meshBoneMatrixBuffers[meshIndex] == nullptr ||
            _meshBoneMatrixBuffers[meshIndex]->Get_ElementCount() < matrixCount)
        {
            _meshBoneMatrixBuffers[meshIndex] =
                ComputeStructuredBuffer::Create(
                    _device,
                    _context,
                    sizeof(Matrix),
                    matrixCount);

            if (_meshBoneMatrixBuffers[meshIndex] == nullptr)
                return E_FAIL;
        }

        CHECK_NULL(_model->_computeAnimationSource.meshBoneRefBuffers[meshIndex], E_FAIL);

        CHECK_FAILED(_computeBoneRemapShaderCom->Bind_SRV(0, _globalCombinedBoneBuffer->Get_SRV()), E_FAIL);
        CHECK_FAILED(_computeBoneRemapShaderCom->Bind_SRV(1, _model->_computeAnimationSource.meshBoneRefBuffers[meshIndex]->Get_SRV()), E_FAIL);
        CHECK_FAILED(_computeBoneRemapShaderCom->Bind_UAV(0, _meshBoneMatrixBuffers[meshIndex]->Get_UAV()), E_FAIL);

        CHECK_FAILED(_computeBoneRemapShaderCom->Dispatch((matrixCount + 63) / 64, 1, 1), E_FAIL);
    }

    return S_OK;
}

void AnimComputeRuntime::Build_FrameContext(float timeDelta, AnimComputeFrameContext& outContext)
{
    _model->Begin_AnimationPipeline(timeDelta, outContext.modelContext);
    outContext.blendRatio = 1.f;
    outContext.hasBlendTarget = false;
}

HRESULT AnimComputeRuntime::Finalize_ModelFrame(const AnimComputeFrameContext& context)
{
    _model->Step_FinalizeAnimationFrame(context.modelContext);

    return S_OK;
}

void AnimComputeRuntime::Reset()
{
    if (_model != nullptr)
        _model->Access_CombinedBoneMatrixCache().clear();

    _globalCombinedBoneBuffer = nullptr;

    _skinningDirty = true;
}

Shared<AnimComputeRuntime> AnimComputeRuntime::Create(const ComPtr<Device>& device, const ComPtr<Context>& context,
    const CreateDesc& desc)
{
    auto instance = make_shared<AnimComputeRuntime>(device, context);

    if (FAILED(instance->Initialize(desc)))
    {
        MSG_BOX("Failed to Created : AnimComputeRuntime");
        return nullptr;
    }

    return instance;
}

void AnimComputeRuntime::Free()
{
    Reset();

    Base::Free();
}
