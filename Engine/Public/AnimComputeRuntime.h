#pragma once

#include "Base.h"
#include "AnimCompute_Types.h"

NS_BEGIN(Engine)

class ShaderCom;
class ComputeStructuredBuffer;
class ComputeShaderCom;
class ModelCom;

class ENGINE_DLL AnimComputeRuntime : public Base
{
public:
    struct CreateDesc
    {
        ModelCom* model{ nullptr };

        Shared<ComputeShaderCom> boneRemapShader{};
        Shared<ComputeShaderCom> skinningShader{};
        Shared<ComputeShaderCom> morphShader{};
    };

public:
    AnimComputeRuntime(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~AnimComputeRuntime() override = default;

public:
    HRESULT Update_Frame(float timeDelta);      // update에서 1회 호출
    HRESULT Prepare_ComputeSkinning();          // render/shadow 직전 호출
    HRESULT Prepare_BoneMatrixBuffers();
    HRESULT Bind_PreparedBoneMatrices(ShaderCom* shader, const char* constantName, uint32 meshIndex);
    HRESULT Render_ComputeSkinned(uint32 meshIndex);

private:
    HRESULT Initialize(const CreateDesc& desc);

    void    Build_FrameContext(float timeDelta, AnimComputeFrameContext& outContext);

    HRESULT Ensure_RuntimeBuffers();
    HRESULT Ensure_RuntimeBoneBuffers();            // CPU mesh bone 캐시 -> GPU Bone 업로드
    HRESULT Ensure_RuntimeSkinningBuffers();        // compute input/ouput/draw vertex buffer 준비
    HRESULT Ensure_RuntimeSkinningConstantBuffer(); // compute constant buffer 준비

    HRESULT Update_GlobalCombinedBoneBuffer_FromCPU();

private: // Morph 메시
    HRESULT Ensure_RuntimeMorphBuffers();
    HRESULT Set_MorphVertices(uint32 meshIndex, const MorphMeshData& meshData, uint32 targetCount);
    HRESULT Ensure_RuntimeMorphConstantBuffer();
    HRESULT Update_MorphWeightBuffer(const vector<float>* weights);
    HRESULT Prepare_MorphDeform();

private:
    HRESULT Build_ComputeCombinedBoneCache();
    HRESULT Step_BoneRemap(const AnimComputeFrameContext& context);
    HRESULT Finalize_ModelFrame(const AnimComputeFrameContext& context);

    void Reset();

private:
    ComPtr<Device>  _device;
    ComPtr<Context> _context;

    ModelCom* _model = nullptr;

    Shared<ComputeShaderCom>        _computeSkinningShaderCom{};
    Shared<ComputeShaderCom>        _computeBoneRemapShaderCom{};
    Shared<ComputeStructuredBuffer> _globalCombinedBoneBuffer{};

    vector<Shared<ComputeStructuredBuffer>> _meshBoneMatrixBuffers;
    vector<Shared<ComputeStructuredBuffer>> _meshSourceVertexBuffers;       // VTXANIMMESH input
    vector<Shared<ComputeStructuredBuffer>> _meshComputedVertexBuffers;     // compute output structured buffer
    vector<ComPtr<Buffer>>                  _meshComputedDrawVertexBuffers; // 실제 draw에 사용할 vertex buffer
    ComPtr<Buffer>                          _computeSkinningConstantBuffer{};

    bool _skinningDirty = true; // compute 결과를 다시 만들어야하면 true

private:
    Shared<ComputeShaderCom> _computeMorphShaderCom{};
    vector<Shared<ComputeStructuredBuffer>> _meshMorphVertexRangeBuffers;
    vector<Shared<ComputeStructuredBuffer>> _meshMorphInfluenceBuffers;
    vector<Shared<ComputeStructuredBuffer>> _meshMorphedSourceVertexBuffers;

    Shared<ComputeStructuredBuffer> _morphWeightBuffer{};
    ComPtr<Buffer> _morphDeformConstantBuffer{};

    bool _morphDirty = false;

public:
    static Shared<AnimComputeRuntime> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const CreateDesc& desc);
    void Free() override;
};

NS_END
