#include "PostProcessPipeline.h"

NS_BEGIN(Engine)

PostProcessPipeline::PostProcessPipeline(ComPtr<Device> device, ComPtr<Context> context)
    : _device{ device }
    , _context{ context }
{
}

HRESULT PostProcessPipeline::Initialize(PostProcessFeature initialFlag)
{
    _featureFlag = initialFlag;
    _postProcessCB.featureMask = To_Underlying(_featureFlag);

    return S_OK;
}

void PostProcessPipeline::Set_PostProcessFeatures(PostProcessFeature featureFlag)
{
    _featureFlag = featureFlag;
    _postProcessCB.featureMask = To_Underlying(_featureFlag);
}

void PostProcessPipeline::Set_PostProcessFeature(PostProcessFeature featureFlag, bool enable)
{
    Set_Flag(_featureFlag, featureFlag, enable);
    _postProcessCB.featureMask = To_Underlying(_featureFlag);
}

bool PostProcessPipeline::Has_PostProcessFeature(PostProcessFeature featureFlag) const
{
    return Has_Flag(_featureFlag, featureFlag);
}

void PostProcessPipeline::Set_PostProcessParams(const PostProcessCB& postProcessCB)
{
    _postProcessCB = postProcessCB;
    _postProcessCB.featureMask = To_Underlying(_featureFlag);
}

const PostProcessCB& PostProcessPipeline::Get_PostProcessParams() const
{
    return _postProcessCB;
}

PostProcessCB& PostProcessPipeline::Get_PostProcessParams()
{
    return _postProcessCB;
}

PostProcessFeature PostProcessPipeline::Get_PostProcessFeatures() const
{
    return _featureFlag;
}

Shared<PostProcessPipeline> PostProcessPipeline::Create(ComPtr<Device> device, ComPtr<Context> context)
{
    Shared<PostProcessPipeline> instance = make_shared<PostProcessPipeline>(device, context);

    if (FAILED(instance->Initialize(
        PostProcessFeature::HDR |
        PostProcessFeature::Exposure |
        PostProcessFeature::ToneMapping |
        PostProcessFeature::Bloom |
        PostProcessFeature::Gamma)))
    {
        MSG_BOX("Failed to Create : PostProcessPipeline");
        return nullptr;
    }

    return instance;
}

void PostProcessPipeline::Free()
{
}

NS_END
