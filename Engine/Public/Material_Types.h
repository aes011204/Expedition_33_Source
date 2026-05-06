#pragma once

#include "Base.h"

NS_BEGIN(Engine)

enum class MaterialTextureSlot : uint8
{
    BaseColor,
    Normal,
    ORM,
    Emissive,
    OpacityMask,
    Opacity,

    END
};

enum class MaterialShaderTemplate : uint8
{
    OpaquePBR,
    MaskedPBR,
    EmissivePBR,
    TransparentPBR,

    END
};

enum class MaterialBlendMode : uint8
{
    Opaque,
    Masked,
    Translucent
};

struct MaterialRenderState
{
    bool twoSided = false;
    MaterialBlendMode blendMode = MaterialBlendMode::Opaque;

    float opacityMaskClipValue = 0.3333f;
};

NS_END
