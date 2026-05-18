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
    inline constexpr const wchar_t* Specular = L"Target_Specular";

    inline constexpr const wchar_t* LightDepth = L"Target_LightDepth";

    inline constexpr const wchar_t* HDRColor = L"Target_HDRColor";
    inline constexpr const wchar_t* BloomExtract = L"Target_BloomExtract";
    inline constexpr const wchar_t* BloomBlur = L"Target_Blur";
}

namespace MRT
{
    inline constexpr const wchar_t* GameObjects = L"MRT_GameObjects";
    inline constexpr const wchar_t* SSAO = L"MRT_SSAO";
    inline constexpr const wchar_t* SSAOBlur = L"MRT_SSAOBlur";
    inline constexpr const wchar_t* LightAcc = L"MRT_LightAcc";
    inline constexpr const wchar_t* ShadowObjects = L"MRT_ShadowObjects";
    inline constexpr const wchar_t* HDR = L"MRT_HDRColor";
    inline constexpr const wchar_t* BloomExtract = L"MRT_BloomExtract";
    inline constexpr const wchar_t* BloomBlur = L"MRT_BloomBlur";
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


#ifdef _DEBUG
#define DEBUG_LOG_FAIL(message) logFail(message)
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
