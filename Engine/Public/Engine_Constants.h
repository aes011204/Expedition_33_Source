#pragma once

// 엔진 전역에서 공유하는 고정 상수 모음. Client_Constants도 있어요
// NOTE:
// ConstantBuffer enum과 ShaderCB::Names 배열 순서는 반드시 동기화되어야 한다.
// 새 ConstantBuffer를 추가할 때는 Engine_Constants의 ShaderCB 이름과 Names 배열도 함께 갱신해야 한다.
#include <array>
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
    inline constexpr ::string_view Hair        = "HairDescBuffer";
    inline constexpr ::string_view DrawFeature = "DrawFeatureBuffer";

    inline constexpr ::array<::string_view, ETOI(ConstantBuffer::END)> Names =
    {
        Material,
        Camera,
        Object,
        Shadow,
        ScreenSpace,
        Light,
        SSAO,
        PostProcess,
        Hair,
        DrawFeature
    };
}

namespace Engine::ShaderSRV
{
    inline constexpr ::string_view MaterialTextures = "g_MaterialTextures";
    inline constexpr ::string_view DrawFeatureTextures = "g_DrawFeatureTextures";

    // Draw feature texture slot 순서와 1:1 대응되는 라벨
    inline constexpr std::array<::string_view, 4> DrawFeatureTextureSlotNames = {
        "FEATURE_TEX_DISSOLVE_NOISE",
        "FEATURE_TEX_DISSOLVE_MASK",
        "FEATURE_TEX_DISTORTION_FLOW",
        "FEATURE_TEX_DISTORTION_NOISE"
    };
}

namespace Engine::DebugPerf
{
#ifdef _DEBUG
    // 성능 평가의 sample_interval 값. 변경하면 체크 주기를 줄이거나 늘릴 수 있다.
    inline constexpr float kRuntimePerfSampleIntervalSec = 0.25f;
#endif
}
