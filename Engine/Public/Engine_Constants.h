#pragma once

// 엔진 전역에서 공유하는 고정 상수 모음. Client_Constants도 있어요
namespace Engine::ShaderCB
{
    inline constexpr ::string_view Material     = "MaterialBuffer";
    inline constexpr ::string_view Camera       = "CameraBuffer";
    inline constexpr ::string_view Object       = "ObjectBuffer";
    inline constexpr ::string_view Shadow       = "ShadowBuffer";
    inline constexpr ::string_view ScreenSpace  = "ScreenSpaceBuffer";
    inline constexpr ::string_view Light       = "LightBuffer";
    inline constexpr ::string_view SSAO        = "SSAOBuffer";
    inline constexpr ::string_view PostProcess = "PostProcessBuffer";
}

namespace Engine::DebugPerf
{
#ifdef _DEBUG
    // 성능 평가의 sample_interval 값. 변경하면 체크 주기를 줄이거나 늘릴 수 있다.
    inline constexpr float kRuntimePerfSampleIntervalSec = 0.25f;
#endif
}
