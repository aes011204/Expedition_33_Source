#pragma once

// 엔진 전역에서 공유하는 고정 상수 모음. Client_Constants도 있어요
namespace Engine::ShaderCB
{
    inline constexpr ::string_view Material     = "MaterialBuffer";
    inline constexpr ::string_view Camera       = "CameraBuffer";
    inline constexpr ::string_view Object       = "ObjectBuffer";
    inline constexpr ::string_view Shadow       = "ShadowBuffer";
    inline constexpr ::string_view Deferred     = "DeferredBuffer";
    inline constexpr ::string_view Light        = "LightBuffer";
    inline constexpr ::string_view PostProcess  = "PostProcessBuffer";
}
