#pragma once

#include "Engine_Define.h"

NS_BEGIN(Engine)

struct RenderViewHotspotEntry
{
    uintptr_t objectKey = 0;
    RenderGroup renderGroup = RenderGroup::END;
    ObjectType objectType = ObjectType::None;
    ShadowCasterType shadowCasterType = ShadowCasterType::None;
    string className = {};
    string sourcePrefab = {};
    wstring sourceModelPath = {};
    Vec3 worldPosition = {};
    bool hasWorldBounds = false;
    Vec3 worldBoundsCenter = {};
    Vec3 worldBoundsExtents = {};
    uint32 renderHotspotInstanceCount = 0;
};

struct RenderViewHotspotSnapshot
{
    bool hasCamera = false;
    Vec3 cameraPosition = {};
    Vec3 cameraForward = {};
    uint32 totalSubmissions = 0;
    uint32 uniqueObjectCount = 0;
    uint32 nonBlendRenderedCount = 0;
    uint32 shadowRenderedCount = 0;
    array<uint32, ETOI(RenderGroup::END)> renderGroupCounts = {};
    array<uint32, 4> shadowCascadeRenderedCounts = {};
    vector<RenderViewHotspotEntry> entries = {};
};

namespace RT
{
    inline constexpr const wchar_t* Diffuse = L"Target_Diffuse";
    inline constexpr const wchar_t* Normal = L"Target_Normal";
    inline constexpr const wchar_t* Depth = L"Target_Depth";
    inline constexpr const wchar_t* Material = L"Target_Material";
    inline constexpr const wchar_t* Emissive = L"Target_Emissive";

    inline constexpr const wchar_t* SSAO = L"Target_SSAO";
    inline constexpr const wchar_t* SSAOBlur = L"Target_SSAOBlur";

    inline constexpr const wchar_t* Shade = L"Target_Shade";
    inline constexpr const wchar_t* Ambient = L"Target_Ambient";
    inline constexpr const wchar_t* Specular = L"Target_Specular";

    inline constexpr const wchar_t* LightDepth = L"Target_LightDepth";              // Dynamic Shadow
#ifdef _DEBUG
    inline constexpr const wchar_t* LightDepthDebug0 = L"Target_LightDepth_Debug0";
    inline constexpr const wchar_t* LightDepthDebug1 = L"Target_LightDepth_Debug1";
    inline constexpr const wchar_t* LightDepthDebug2 = L"Target_LightDepth_Debug2";
    inline constexpr const wchar_t* LightDepthDebug3 = L"Target_LightDepth_Debug3";
    inline constexpr const wchar_t* FroxelDebug = L"Target_FroxelDebug";
#endif

    inline constexpr const wchar_t* HDRColor = L"Target_HDRColor";
    inline constexpr const wchar_t* DistortionSource = L"Target_DistortionSource"; // 왜곡 pass가 읽을 HDRColor snapshot이다.
    inline constexpr const wchar_t* DistortionField = L"Target_DistortionField";   // 왜곡 geometry가 누적할 screen-space offset/weight field다.
    inline constexpr const wchar_t* LightShaftMaskHalf = L"Target_LightShaftMaskHalf";
    inline constexpr const wchar_t* LightShaftBlurHalf = L"Target_LightShaftBlurHalf";
    inline constexpr const wchar_t* BackgroundMask = L"Target_BackgroundMask";
    inline constexpr const wchar_t* ScreenFxColorA = L"Target_ScreenFxColorA";
    inline constexpr const wchar_t* ScreenFxColorB = L"Target_ScreenFxColorB";
    inline constexpr const wchar_t* ScreenCapture = L"Target_ScreenCapture";
    inline constexpr const wchar_t* BloomHalfA = L"Target_BloomHalfA";
    inline constexpr const wchar_t* BloomHalfB = L"Target_BloomHalfB";
    inline constexpr const wchar_t* BloomQuarterA = L"Target_BloomQuarterA";
    inline constexpr const wchar_t* BloomQuarterB = L"Target_BloomQuarterB";
    inline constexpr const wchar_t* BloomEighthA = L"Target_BloomEighthA";
    inline constexpr const wchar_t* BloomEighthB = L"Target_BloomEighthB";
    inline constexpr const wchar_t* DOFCoCHalf = L"Target_DOFCoCHalf";
    inline constexpr const wchar_t* DOFNearHalfA = L"Target_DOFNearHalfA";
    inline constexpr const wchar_t* DOFNearHalfB = L"Target_DOFNearHalfB";
    inline constexpr const wchar_t* DOFFarHalfA = L"Target_DOFFarHalfA";
    inline constexpr const wchar_t* DOFFarHalfB = L"Target_DOFFarHalfB";
    inline constexpr const wchar_t* DOFFarQuarterA = L"Target_DOFFarQuarterA";
    inline constexpr const wchar_t* DOFFarQuarterB = L"Target_DOFFarQuarterB";
    inline constexpr const wchar_t* AmbientVisibilityFrontDepth = L"Target_AmbientVisibilityFrontDepth";
    inline constexpr const wchar_t* AmbientVisibilityBackDepth = L"Target_AmbientVisibilityBackDepth";
    inline constexpr const wchar_t* AmbientVisibilityAmount = L"Target_AmbientVisibilityAmount";
    inline constexpr const wchar_t* AmbientVisibility = L"Target_AmbientVisibility";
}

namespace MRT
{
    inline constexpr const wchar_t* GameObjects = L"MRT_GameObjects";
    inline constexpr const wchar_t* SSAO = L"MRT_SSAO";
    inline constexpr const wchar_t* SSAOBlur = L"MRT_SSAOBlur";
    inline constexpr const wchar_t* LightAcc = L"MRT_LightAcc";
    inline constexpr const wchar_t* ShadowObjects = L"MRT_ShadowObjects";
    inline constexpr const wchar_t* HDR = L"MRT_HDRColor";
    inline constexpr const wchar_t* DistortionField = L"MRT_DistortionField";
    inline constexpr const wchar_t* LightShaftMaskHalf = L"MRT_LightShaftMaskHalf";
    inline constexpr const wchar_t* LightShaftBlurHalf = L"MRT_LightShaftBlurHalf";
    inline constexpr const wchar_t* ScreenFxA = L"MRT_ScreenFxA";
    inline constexpr const wchar_t* ScreenFxB = L"MRT_ScreenFxB";
    inline constexpr const wchar_t* BloomHalfA = L"MRT_BloomHalfA";
    inline constexpr const wchar_t* BloomHalfB = L"MRT_BloomHalfB";
    inline constexpr const wchar_t* BloomQuarterA = L"MRT_BloomQuarterA";
    inline constexpr const wchar_t* BloomQuarterB = L"MRT_BloomQuarterB";
    inline constexpr const wchar_t* BloomEighthA = L"MRT_BloomEighthA";
    inline constexpr const wchar_t* BloomEighthB = L"MRT_BloomEighthB";
    inline constexpr const wchar_t* DOFPreFilterHalf = L"MRT_DOFPreFilterHalf";
    inline constexpr const wchar_t* DOFNearHalfB = L"MRT_DOFNearHalfB";
    inline constexpr const wchar_t* DOFFarHalfB = L"MRT_DOFFarHalfB";
    inline constexpr const wchar_t* DOFFarQuarterA = L"MRT_DOFFarQuarterA";
    inline constexpr const wchar_t* DOFFarQuarterB = L"MRT_DOFFarQuarterB";
    inline constexpr const wchar_t* AmbientVisibilityInterval = L"MRT_AmbientVisibilityInterval";
    inline constexpr const wchar_t* AmbientVisibility = L"MRT_AmbientVisibility";
#ifdef _DEBUG
    inline constexpr const wchar_t* FroxelDebug = L"MRT_FroxelDebug";
#endif
}


static constexpr bool UseDSV = true;
static constexpr bool NotUseDSV = false;
static constexpr bool ClearRTV = true;
static constexpr bool NotClearRTV = false;

#pragma region RENDERER MACRO
#define ENDMRT_RETURN_IF_FAILED(expr)       \
    do                                      \
    {                                       \
        if (FAILED(expr))                   \
        {                                   \
            GAME->End_MRT();                \
            return;                         \
        }                                   \
    } while (false)

#define RETURN_LOG_IF_FAILED(expr, message)         \
    do                                              \
    {                                               \
        if (FAILED(expr))                           \
        {                                           \
            DEBUG_LOG_FAIL(message);                \
            return;                                 \
        }                                           \
    } while (false)

#define RETURN_FAIL_LOG_IF_FAILED(expr, message)    \
    do                                              \
    {                                               \
        if (FAILED(expr))                           \
        {                                           \
            DEBUG_LOG_FAIL(message);                \
            return E_FAIL;                          \
        }                                           \
    } while (false)


#ifdef _DEBUG
#define DEBUG_LOG_FAIL(message) this->DumpCurrentOMState(message)
#else
#define DEBUG_LOG_FAIL(message) ((void)0)
#endif

#define ENDMRT_RETURN_LOG_IF_FAILED(expr, message)  \
    do                                              \
    {                                               \
        if (FAILED(expr))                           \
        {                                           \
            DEBUG_LOG_FAIL(message);                \
            GAME->End_MRT();                        \
            return;                                 \
        }                                           \
    } while (false)

#define ENDMRT_RETURN_FAIL_LOG_IF_FAILED(expr, message)  \
    do                                              \
    {                                               \
        if (FAILED(expr))                           \
        {                                           \
            DEBUG_LOG_FAIL(message);                \
            GAME->End_MRT();                        \
            return E_FAIL;                          \
        }                                           \
    } while (false)


#define COMBINEDHDR_RETURN_LOG_IF_FAILED(expr, message)    \
    do                                                      \
    {                                                       \
        if (FAILED(expr))                                   \
        {                                                   \
            DEBUG_LOG_FAIL(message);                        \
            if (postProcess)                                \
                GAME->End_MRT();                            \
            return;                                         \
        }                                                   \
    } while (false)
#pragma endregion

NS_END
