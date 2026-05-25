#pragma once

#include "Engine_Define.h"

NS_BEGIN(Engine)

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
    inline constexpr const wchar_t* StaticLightDepth = L"Target_StaticLightDepth";  // Static Shadow

    inline constexpr const wchar_t* HDRColor = L"Target_HDRColor";
    inline constexpr const wchar_t* BackgroundMask = L"Target_BackgroundMask";
    inline constexpr const wchar_t* ScreenFxColor = L"Target_ScreenFxColor";
    inline constexpr const wchar_t* BloomHalfA = L"Target_BloomHalfA";
    inline constexpr const wchar_t* BloomHalfB = L"Target_BloomHalfB";
    inline constexpr const wchar_t* BloomQuarterA = L"Target_BloomQuarterA";
    inline constexpr const wchar_t* BloomQuarterB = L"Target_BloomQuarterB";
    inline constexpr const wchar_t* BloomEighthA = L"Target_BloomEighthA";
    inline constexpr const wchar_t* BloomEighthB = L"Target_BloomEighthB";
}

namespace MRT
{
    inline constexpr const wchar_t* GameObjects = L"MRT_GameObjects";
    inline constexpr const wchar_t* SSAO = L"MRT_SSAO";
    inline constexpr const wchar_t* SSAOBlur = L"MRT_SSAOBlur";
    inline constexpr const wchar_t* LightAcc = L"MRT_LightAcc";
    inline constexpr const wchar_t* ShadowObjects = L"MRT_ShadowObjects";
    inline constexpr const wchar_t* StaticShadowObjects = L"MRT_StaticShadowObjects";
    inline constexpr const wchar_t* HDR = L"MRT_HDRColor";
    inline constexpr const wchar_t* ScreenFx = L"MRT_ScreenFx";
    inline constexpr const wchar_t* BloomHalfA = L"MRT_BloomHalfA";
    inline constexpr const wchar_t* BloomHalfB = L"MRT_BloomHalfB";
    inline constexpr const wchar_t* BloomQuarterA = L"MRT_BloomQuarterA";
    inline constexpr const wchar_t* BloomQuarterB = L"MRT_BloomQuarterB";
    inline constexpr const wchar_t* BloomEighthA = L"MRT_BloomEighthA";
    inline constexpr const wchar_t* BloomEighthB = L"MRT_BloomEighthB";
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
