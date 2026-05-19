#pragma once

#include "Base.h"
#include "AnimCompute_Types.h"

NS_BEGIN(Engine)

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
    };

public:
    AnimComputeRuntime(const ComPtr<Device>& device, const ComPtr<Context>& context);
    ~AnimComputeRuntime() override = default;

public:
    HRESULT Update_Frame(float timeDelta);      // update에서 1회 호출
    HRESULT Prepare_ComputeSkinning();          // render/shadow 직전 호출

private:
    HRESULT Initialize(const CreateDesc& desc);

    void Build_FrameContext(float timeDelta, AnimComputeFrameContext& outContext);
    HRESULT Ensure_RuntimeBuffers();
    HRESULT Update_GlobalCombinedBoneBuffer_FromCPU();

private:
    HRESULT Build_ComputeCombinedBoneCache();
    HRESULT Step_BoneRemap(const AnimComputeFrameContext& context);
    HRESULT Finalize_ModelFrame(const AnimComputeFrameContext& context);

    void Reset();

private:
    Shared<ComputeShaderCom> _computeSkinningShaderCom{};
    Shared<ComputeShaderCom> _computeBoneRemapShaderCom{};

    vector<Matrix> _combinedBoneCache;

private:
    ComPtr<Device>  _device;
    ComPtr<Context> _context;

    ModelCom* _model = nullptr;

    Shared<ComputeStructuredBuffer> _globalCombinedBoneBuffer{};

    bool _skinningDirty = true;

public:
    static Shared<AnimComputeRuntime> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const CreateDesc& desc);
    void Free() override;
};

NS_END
