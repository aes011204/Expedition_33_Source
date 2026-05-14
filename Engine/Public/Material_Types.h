#pragma once

#include "Base.h"

NS_BEGIN(Engine)

// 모델 추출용
enum class MaterialRawTextureType : uint8
{
    Diffuse = 1,
    Emissive = 4,
    Normals = 6,
    Opacity = 8,
    BaseColor = 12,
    NormalCamera = 13,
    EmissionColor = 14,
    Metalness = 15,
    Roughness = 16,
    AmbientOcclusion = 17,
    GLTF_MetallicRoughness = 27,
};

// 클라이언트가 실제 사용할 슬롯
enum class MaterialTextureSlot : uint8
{
    BaseColor,
    Normal,
    Emissive,
    OpacityMask,
    ORM,

    HairDepth,
    HairID,

    IrisHeight,
    IrisMask,

    END
};

// Raw -> 클라 매핑용
inline MaterialTextureSlot ResolveMaterialTextureSlot(MaterialRawTextureType rawType)
{
    switch (rawType)
    {
    case MaterialRawTextureType::Diffuse:
    case MaterialRawTextureType::BaseColor:
        return MaterialTextureSlot::BaseColor;

    case MaterialRawTextureType::Normals:
    case MaterialRawTextureType::NormalCamera:
        return MaterialTextureSlot::Normal;

    case MaterialRawTextureType::Emissive:
    case MaterialRawTextureType::EmissionColor:
        return MaterialTextureSlot::Emissive;

    case MaterialRawTextureType::Opacity:
        return MaterialTextureSlot::OpacityMask;

    case MaterialRawTextureType::Metalness:
    case MaterialRawTextureType::Roughness:
    case MaterialRawTextureType::AmbientOcclusion:
    case MaterialRawTextureType::GLTF_MetallicRoughness:
        return MaterialTextureSlot::ORM;

    default:
        return MaterialTextureSlot::END;
    }
}

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
    Opaque,         // Nonblend::Opaque
    Masked,         // Nonblend::Masked(discard)
    Translucent,    // 현재 구소 상 blend 패스는 따로 뺄 수가 없음. 확장 or 걍 의미적으로만 둠.
    Additive,       // 마찬가지

    END
};

enum class MappingInputKind : uint8
{
    Skeletal,
    StaticMesh,
    Fx,
};

enum class MaterialRefKind : uint8
{
    MI,
    M,
    Unknown,
};

enum class MaterialSourceKind : uint8
{
    SK,
    SM,
    FX,
};

NS_END
