#include "pch.h"
#include "AnimationNotify.h"



NS_BEGIN(Engine)

AnimationNotify::AnimationNotify() : Base {} {}

AnimationNotify::AnimationNotify(const AnimationNotify& rhs)
    : Base{ rhs } {}

HRESULT AnimationNotify::Initialize()
{
    return S_OK;
}

void AnimationNotify::Update_Notify(float timeDelta)
{

}

Shared<AnimationNotify> AnimationNotify::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto prototype = make_shared<AnimationNotify>();

    if (FAILED(prototype->Initialize()))
    {
        MSG_BOX("Failed to Created : AnimationNotify");
        return nullptr;
    }

    return prototype;
}

Shared<AnimationNotify> AnimationNotify::Clone()
{
    return make_shared<AnimationNotify>(*this);
}

void AnimationNotify::Free()
{
    Base::Free();
}

NS_END
