#pragma once

#include "Base.h"
#include "Shader_CBuffer_Define.h"

NS_BEGIN(Engine)

struct PostProcessCB;

class ENGINE_DLL PostProcessPipeline : public Base
{
public:
    explicit PostProcessPipeline(ComPtr<Device> device, ComPtr<Context> context);
    virtual ~PostProcessPipeline() = default;

public:
    HRESULT Initialize(PostProcessFeature initialFlag);

    void Set_PostProcessFeatures(PostProcessFeature featureFlag);
    void Set_PostProcessFeature(PostProcessFeature featureFlag, bool enable);
    bool Has_PostProcessFeature(PostProcessFeature featureFlag) const;

    void Set_PostProcessParams(const PostProcessCB& postProcessCB);
    const PostProcessCB& Get_PostProcessParams() const;
    PostProcessCB& Get_PostProcessParams();

    PostProcessFeature Get_PostProcessFeatures() const;

private:
    ComPtr<Device>              _device;
    ComPtr<Context>             _context;

    PostProcessFeature          _featureFlag = PostProcessFeature::None;
    PostProcessCB               _postProcessCB{};

public:
    static Shared<PostProcessPipeline> Create(ComPtr<Device> device, ComPtr<Context> context);
    virtual void Free() override;
};

NS_END
