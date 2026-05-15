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

    inline constexpr const wchar_t* Shade = L"Target_Shade";
    inline constexpr const wchar_t* Specular = L"Target_Specular";

    inline constexpr const wchar_t* LightDepth = L"Target_LightDepth";

    inline constexpr const wchar_t* HDRColor = L"Target_HDRColor";
}

namespace MRT
{
    inline constexpr const wchar_t* GameObjects = L"MRT_GameObjects";
    inline constexpr const wchar_t* LightAcc = L"MRT_LightAcc";
    inline constexpr const wchar_t* ShadowObjects = L"MRT_ShadowObjects";
    inline constexpr const wchar_t* HDR = L"MRT_HDRColor";
}

NS_END
