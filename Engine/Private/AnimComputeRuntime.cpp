#include "AnimComputeRuntime.h"
#include "ModelCom.h"
#include "Animation.h"
#include "Mesh.h"
#include "SkeletalMesh.h"
#include "ComputeShaderCom.h"
#include "ComputeStructuredBuffer.h"
#include "GameInstance.h"
#include "Bone.h"

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
    CHECK_NULL(_model, E_FAIL);

    AnimComputeFrameContext context{};
    Build_FrameContext(timeDelta, context);

    _model->Step_SampleAnimationPose(context.modelContext);
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

    CHECK_FAILED(_model->Ensure_ComputeSkinningBuffers(), E_FAIL);
    CHECK_FAILED(_model->Ensure_ComputeSkinningConstantBuffer(), E_FAIL);

    if (!_skinningDirty)
        return S_OK;

    for (uint32 meshIndex = 0; meshIndex < _model->_meshes.size(); ++meshIndex)
    {
        const Shared<ComputeStructuredBuffer>& sourceVertexBuffer = _model->_meshSourceVertexBuffers[meshIndex];
        const Shared<ComputeStructuredBuffer>& meshBoneMatrixBuffer = _model->_meshBoneMatrixBuffers[meshIndex];
        const Shared<ComputeStructuredBuffer>& computedVertexBuffer = _model->_meshComputedVertexBuffers[meshIndex];

        if (sourceVertexBuffer == nullptr || meshBoneMatrixBuffer == nullptr || computedVertexBuffer == nullptr)
            continue;

        const uint32 vertexCount = computedVertexBuffer->Get_ElementCount();
        if (vertexCount == 0)
            continue;

        CHECK_FAILED(_model->Update_ComputeSkinningConstantBuffer(vertexCount), E_FAIL);

        CHECK_FAILED(_computeSkinningShaderCom->Bind_SRV(0, sourceVertexBuffer->Get_SRV()), E_FAIL);
        CHECK_FAILED(_computeSkinningShaderCom->Bind_SRV(1, meshBoneMatrixBuffer->Get_SRV()), E_FAIL);
        CHECK_FAILED(_computeSkinningShaderCom->Bind_UAV(0, computedVertexBuffer->Get_UAV()), E_FAIL);
        CHECK_FAILED(_computeSkinningShaderCom->Bind_ConstantBuffer(0, _model->_computeSkinningConstantBuffer.Get()), E_FAIL);

        CHECK_FAILED(_computeSkinningShaderCom->Dispatch((vertexCount + 63) / 64, 1, 1), E_FAIL);

        if (meshIndex >= _model->_meshComputedDrawVertexBuffers.size() ||
            _model->_meshComputedDrawVertexBuffers[meshIndex] == nullptr)
            return E_FAIL;

        _context->CopyResource(
            _model->_meshComputedDrawVertexBuffers[meshIndex].Get(),
            computedVertexBuffer->Get_Buffer());
    }

    _skinningDirty = false;

    return S_OK;
}

void AnimComputeRuntime::Build_FrameContext(float timeDelta, AnimComputeFrameContext& outContext)
{
    _model->Begin_AnimationPipeline(timeDelta, outContext.modelContext);
    outContext.blendRatio = 1.f;
    outContext.hasBlendTarget = false;
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

HRESULT AnimComputeRuntime::Build_ComputeCombinedBoneCache()
{
    if (_model == nullptr)
        return E_FAIL;

    if (!_model->_isSkeletal)
        return S_OK;

    const uint32 numBones = static_cast<uint32>(_model->_numBones);
    if (numBones == 0)
    {
        _model->Clear_ComputeCombinedBoneMatrices();
        return S_OK;
    }

    vector<Matrix> combinedBones{};
    combinedBones.resize(numBones, Matrix::Identity);

    for (uint32 boneIndex = 0; boneIndex < numBones; ++boneIndex)
    {
        const Matrix* combinedMatrix = _model->_bones[boneIndex]->Get_CombinedTransformationMatrixPtr();
        if (combinedMatrix != nullptr)
            combinedBones[boneIndex] = *combinedMatrix;
    }

    _model->Set_ComputeCombinedBoneMatrices(combinedBones);
    return S_OK;
}

HRESULT AnimComputeRuntime::Update_GlobalCombinedBoneBuffer_FromCPU()
{
    if (_model == nullptr)
        return E_FAIL;

    if (!_model->_isSkeletal)
        return S_OK;

    const vector<Matrix>& combinedBones = _model->_computeCombinedBoneMatrices;
    if (combinedBones.empty())
        return S_OK;

    CHECK_NULL(_globalCombinedBoneBuffer, E_FAIL);

    CHECK_FAILED(
        _globalCombinedBoneBuffer->Update_Data(
            combinedBones.data(),
            static_cast<uint32>(combinedBones.size())),
        E_FAIL);

    return S_OK;
}

HRESULT AnimComputeRuntime::Step_BoneRemap(const AnimComputeFrameContext& context)
{
    UNREFERENCED_PARAMETER(context);

    CHECK_NULL(_computeBoneRemapShaderCom, E_FAIL);
    CHECK_NULL(_globalCombinedBoneBuffer, E_FAIL);

    if (_model->_meshBoneMatrixBuffers.size() != _model->_meshes.size())
        _model->_meshBoneMatrixBuffers.resize(_model->_meshes.size());

    for (uint32 meshIndex = 0; meshIndex < _model->_meshes.size(); ++meshIndex)
    {
        const auto& meshBoneRefs = _model->_computeAnimationSource.meshBoneRefs[meshIndex];
        if (meshBoneRefs.empty())
            continue;

        const uint32 matrixCount = static_cast<uint32>(meshBoneRefs.size());

        if (_model->_meshBoneMatrixBuffers[meshIndex] == nullptr ||
            _model->_meshBoneMatrixBuffers[meshIndex]->Get_ElementCount() < matrixCount)
        {
            _model->_meshBoneMatrixBuffers[meshIndex] =
                ComputeStructuredBuffer::Create(
                    _device,
                    _context,
                    sizeof(Matrix),
                    matrixCount);

            if (_model->_meshBoneMatrixBuffers[meshIndex] == nullptr)
                return E_FAIL;
        }

        CHECK_NULL(_model->_computeAnimationSource.meshBoneRefBuffers[meshIndex], E_FAIL);

        CHECK_FAILED(_computeBoneRemapShaderCom->Bind_SRV(0, _globalCombinedBoneBuffer->Get_SRV()), E_FAIL);
        CHECK_FAILED(_computeBoneRemapShaderCom->Bind_SRV(1, _model->_computeAnimationSource.meshBoneRefBuffers[meshIndex]->Get_SRV()), E_FAIL);
        CHECK_FAILED(_computeBoneRemapShaderCom->Bind_UAV(0, _model->_meshBoneMatrixBuffers[meshIndex]->Get_UAV()), E_FAIL);

        CHECK_FAILED(_computeBoneRemapShaderCom->Dispatch((matrixCount + 63) / 64, 1, 1), E_FAIL);
    }

    return S_OK;
}

HRESULT AnimComputeRuntime::Finalize_ModelFrame(const AnimComputeFrameContext& context)
{
    _model->Step_FinalizeAnimationFrame(context.modelContext);

    return S_OK;
}

void AnimComputeRuntime::Reset()
{
    if (_model != nullptr)
        _model->Clear_ComputeCombinedBoneMatrices();

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
