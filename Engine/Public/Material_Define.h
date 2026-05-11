#pragma once

#include "Material_Types.h"

NS_BEGIN(Engine)

// 에디터 등에서 편집한 렌더 상태를 Runtime Material에 복사한다.
struct MaterialRenderState
{
    MaterialBlendMode blendMode = MaterialBlendMode::Opaque;

    bool twoSided = false;                                      // 양면 렌더링
    bool alphaClip = false;
    float opacityMaskClipValue = 0.3333f;
};

struct alignas(16) MaterialCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::Material;

    Vec4 baseColorTint{ 1.f, 1.f, 1.f, 1.f };
    Vec4 emissiveColor{ 1.f, 1.f, 1.f, 1.f };

    // MI json의 Scalars.Value_Glow 계열
    float emissiveIntensity{ 0.f };

    // MI json의 Scalars.Glow_Mask_Mult 계열
    float emissiveMaskMul{ 1.f };

    // MI json의 Normal/Bump Multiplier 또는 NormalIntensity 계열
    float normalStrength{ 1.f };

    // 셰이더에서 clip 기준으로 사용
    float opacityMaskClipValue{ 0.3333f };

    // ORM 보정값
    float aoScale{ 1.f };           // ORM.r = AO
    float roughnessScale{ 1.f };    // ORM.g = Roughness
    float metalnessScale{ 1.f };    // ORM.b = Metalness
    float _pad0{ 0.f };

    Vec2 uvScale{ 1.f, 1.f };
    Vec2 uvOffset{ 0.f, 0.f };
    float uvRotation{ 0.f };

    uint32 materialMask{ 0 };
    float _pad1[2]{};
};

// 저장/에셋/데이터 단계에서의 텍스처 슬롯 정보
struct MaterialTextureDesc
{
    MaterialTextureSlot slot = MaterialTextureSlot::END;

    wstring path;

    uint32 textureIndex = 0;

    Vec2 uvScale = Vec2(1.f, 1.f);
    Vec2 uvOffset = Vec2(0.f, 0.f);
    float uvRotation = 0.f;

    float blendFactor = 1.f;
    uint32 blendOp = 0;
    uint32 uvChannel = 0;
    uint32 wrapMode = 0;
};

// 해당 머테리얼을 어떤 방식으로 렌더링할 것인지 정하는 규격
struct MaterialTemplate
{
    MaterialShaderTemplate id = MaterialShaderTemplate::OpaquePBR;

    string name;

    string shaderKey;
    string passName;

    MaterialRenderState renderStateDefaults{};

    bool supportsBaseColor = true;
    bool supportsNormal = true;
    bool supportsEmissive = true;
    bool supportsOpacityMask = false;
    bool supportsORM = true;

    bool isFX = false;
    bool isUnlit = false;
};

struct MaterialDesc
{
    string name{};

    MaterialShaderTemplate templateId = MaterialShaderTemplate::OpaquePBR;
    MaterialRenderState renderState{};
    MaterialCB cb{};

    vector<MaterialTextureDesc> textures{};
};


// ==========================================================================
// 예시 모음
// ==========================================================================

#pragma region MaterialTemplate 예시
/*
    BaseColor 사용
    Normal 사용 가능
    ORM 사용 가능
    Emissive 사용 가능
    OpacityMask 기본 미사용
    BlendMode = Opaque
    DepthWrite = true
 */

/*
    BaseColor 사용
    Normal 사용 가능
    ORM 사용 가능
    Emissive 사용 가능
    OpacityMask 사용 가능
    BlendMode = Masked
    DepthWrite = true
    alphaClip = true
 */
#pragma endregion



NS_END
