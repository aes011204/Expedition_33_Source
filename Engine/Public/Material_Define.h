#pragma once

#include <MaterialCB_OverrideFields.inl>

#include "Material_Types.h"
#include "Shader_CBuffer_Define.h"

NS_BEGIN(Engine)

// 에디터 등에서 편집한 렌더 상태를 Runtime Material에 복사한다.
struct MaterialRenderState
{
    MaterialBlendMode blendMode = MaterialBlendMode::Opaque;

    bool twoSided = false;                                      // 양면 렌더링
    bool alphaClip = false;
    float opacityMaskClipValue = 0.3333f;
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

// 모델 컴포넌트에서만 사용되는 구조체인데 헤더가 길어져서 여기로 뺌
struct TextureOverride
{
    bool enabled = false;
    wstring path{};
};

struct MaterialInstanceOverride
{
    bool overrideTemplateEnabled = false;
    MaterialShaderTemplate templateId = MaterialShaderTemplate::OpaquePBR;

    bool overrideRenderStateEnabled = false;

    MaterialRenderState renderState{};

    struct MaterialCBOverrideMask
    {
#define MODEL_COM_CB_MASK_FIELD(_key, _member, _kind) bool _member = false;
        MATERIAL_CB_OVERRIDE_FIELDS(MODEL_COM_CB_MASK_FIELD)
#undef MODEL_COM_CB_MASK_FIELD

            bool Any() const
        {
            bool any = false;
#define MODEL_COM_CB_MASK_ANY(_key, _member, _kind) any = any || _member;
            MATERIAL_CB_OVERRIDE_FIELDS(MODEL_COM_CB_MASK_ANY)
#undef MODEL_COM_CB_MASK_ANY
                return any;
        }

        void Clear()
        {
#define MODEL_COM_CB_MASK_CLEAR(_key, _member, _kind) _member = false;
            MATERIAL_CB_OVERRIDE_FIELDS(MODEL_COM_CB_MASK_CLEAR)
#undef MODEL_COM_CB_MASK_CLEAR
        }
    };

    MaterialCBOverrideMask cbMask{};
    MaterialCB cb{};
    int32 forcedPassIndex = -1;

    TextureOverride baseColor{};
    TextureOverride normal{};
    TextureOverride orm{};
    TextureOverride orm2{};
    TextureOverride orm3{};
    TextureOverride emissive{};
    TextureOverride opacityMask{};

    TextureOverride hairDepth{};
    TextureOverride hairID{};
    TextureOverride irisHeight{};
    TextureOverride irisMask{};
};

// 추출한 모델 컴포넌트가 가진 메쉬 정보(인덱스)와 동일하게 materialIndex 별로 ModelCom::_materialBindings에 들어간다. 
struct MaterialBinding
{
    string baseBundleKey{};                 // .material.json 이름
    MaterialInstanceOverride instance{};    // 이 오브젝트만 덮어쓸 값
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
