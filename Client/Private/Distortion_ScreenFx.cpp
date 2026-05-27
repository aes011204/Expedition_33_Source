#include "pch.h"
#include "Distortion_ScreenFx.h"


Distortion_ScreenFx::Distortion_ScreenFx()
    : ScreenFx_Instance{} {}

HRESULT Distortion_ScreenFx::Initialize(ScreenFxDesc* desc)
{
    

    return ScreenFx_Instance::Initialize(desc);
}

HRESULT Distortion_ScreenFx::Bind_ShaderResources(ShaderCom* shader)
{
    return ScreenFx_Instance::Bind_ShaderResources(shader);
}

Shared<Distortion_ScreenFx> Distortion_ScreenFx::Create(ScreenFxDesc* desc)
{
    auto instance = make_shared<Distortion_ScreenFx>();
    CHECK_FAILED(instance->Initialize(desc), nullptr);
    return instance;
}
