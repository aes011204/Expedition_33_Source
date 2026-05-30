#pragma once
#include "Material.h"

namespace
{
    namespace VtxTexPassIndex
    {
        using DefaultPass = std::integral_constant<uint32, 0>;
    }

    namespace VtxNorTexPassIndex
    {
        constexpr uint32 DefaultPass = 0;
    }

    namespace VtxMeshPassIndex
    {
        constexpr uint32 DefaultPass = 0;
        constexpr uint32 ShadowPass = 1;
        constexpr uint32 NoTexturePass = 2;

        constexpr uint32 MaskedPass = 3;
        constexpr uint32 TwoSidedPass = 4;
        constexpr uint32 MaskedTwoSidedPass = 5;
        constexpr uint32 IDPass = 6;
        constexpr uint32 OpaqueARPass = 7;
        constexpr uint32 OpaqueARTwoSidedPass = 8;
        constexpr uint32 PackedAR_NMEPass = 9;
        constexpr uint32 PackedAR_NMETwoSidedPass = 10;
        constexpr uint32 LandscapeBlendPass = 11;
    }

    namespace VtxInstanceMeshExactPassIndex
    {
        constexpr uint32 DefaultPass = 0;
        constexpr uint32 ShadowPass = 1;
        constexpr uint32 NoTexturePass = 2;
        constexpr uint32 MaskedPass = 3;
        constexpr uint32 TwoSidedPass = 4;
        constexpr uint32 MaskedTwoSidedPass = 5;
        constexpr uint32 IDPass = 6;
        constexpr uint32 OpaqueARPass = 7;
        constexpr uint32 OpaqueARTwoSidedPass = 8;
    }

    namespace VtxCubePassIndex
    {
        constexpr uint32 DefaultPass = 0;
    }

    namespace VtxAnimMeshPassIndex
    {
        constexpr uint32 OpaquePass = 0;
        constexpr uint32 MaskedPass = 1;
        constexpr uint32 OpaqueTwoSidedPass = 2;
        constexpr uint32 MaskedTwoSidedPass = 3;
        constexpr uint32 ShadowPass = 4;
    }

}

NS_BEGIN(Engine)

class ShaderPassFinder
{
public:
    static uint32 Find_PassIndex(ShaderType shaderType, const MaterialRenderState& renderState)
    {
        switch (shaderType)
        {
        case ShaderType::VtxTex:
            return Find_VtxTexPassIndex(renderState);

        case ShaderType::VtxNorTex:
            return Find_VtxNorTexPassIndex(renderState);

        case ShaderType::VtxMesh:
            return Find_VtxMeshPassIndex(renderState);

        case ShaderType::VtxInstanceMeshExact:
            return Find_VtxInstanceMeshExactPassIndex(renderState);

        case ShaderType::VtxAnimMesh:
            return Find_VtxAnimMeshPassIndex(renderState);

        case ShaderType::VtxCube:
            return Find_VtxCubePassIndex(renderState);

        // TODO 이 부분이 필요한가?
        //case ShaderType::BillboardParticle:
        //    return Find_BillboardParticlePassIndex(renderState);

        //case ShaderType::UI:
        //    return Find_UIPassIndex(renderState);

        default:
            return 0;
        }
    }

private:

    static uint32 Find_VtxTexPassIndex(const MaterialRenderState& renderState)
    {
        UNREFERENCED_PARAMETER(renderState);

        return VtxTexPassIndex::DefaultPass::value;
    }

    static uint32 Find_VtxNorTexPassIndex(const MaterialRenderState& renderState)
    {
        UNREFERENCED_PARAMETER(renderState);

        return VtxNorTexPassIndex::DefaultPass;
    }

    static uint32 Find_VtxMeshPassIndex(const MaterialRenderState& renderState)
    {
        const bool isMasked =
            renderState.blendMode == MaterialBlendMode::Masked ||
            renderState.alphaClip;

        if (isMasked)
        {
            if (renderState.twoSided)
                return VtxMeshPassIndex::MaskedTwoSidedPass;

            return VtxMeshPassIndex::MaskedPass;
        }

        if (renderState.twoSided)
            return VtxMeshPassIndex::TwoSidedPass;

        return VtxMeshPassIndex::DefaultPass;
    }

    static uint32 Find_VtxInstanceMeshExactPassIndex(const MaterialRenderState& renderState)
    {
        const bool isMasked =
            renderState.blendMode == MaterialBlendMode::Masked ||
            renderState.alphaClip;

        if (isMasked)
        {
            if (renderState.twoSided)
                return VtxInstanceMeshExactPassIndex::MaskedTwoSidedPass;

            return VtxInstanceMeshExactPassIndex::MaskedPass;
        }

        if (renderState.twoSided)
            return VtxInstanceMeshExactPassIndex::TwoSidedPass;

        return VtxInstanceMeshExactPassIndex::DefaultPass;
    }

    static uint32 Find_VtxCubePassIndex(const MaterialRenderState& renderState)
    {
        UNREFERENCED_PARAMETER(renderState);

        return VtxCubePassIndex::DefaultPass;
    }

    static uint32 Find_VtxAnimMeshPassIndex(const MaterialRenderState& renderState)
    {
        const bool isMasked =
            renderState.blendMode == MaterialBlendMode::Masked ||
            renderState.alphaClip;

        if (isMasked)
        {
            if (renderState.twoSided)
                return VtxAnimMeshPassIndex::MaskedTwoSidedPass;

            return VtxAnimMeshPassIndex::MaskedPass;
        }

        if (renderState.twoSided)
            return VtxAnimMeshPassIndex::OpaqueTwoSidedPass;

        return VtxAnimMeshPassIndex::OpaquePass;
    }

private :
    static bool Is_Masked(const MaterialRenderState& renderState)
    {
        return renderState.blendMode == MaterialBlendMode::Masked ||
            renderState.alphaClip;
    }
};

NS_END

