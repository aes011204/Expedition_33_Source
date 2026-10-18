#pragma once

#include "Material_Types.h"
#include "Material_Define.h"
#include "MaterialCB_OverrideFields.inl"

NS_BEGIN(MainEditor)

// Material_View에서 어떤 JSON을 읽을지 선택하는 값
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

// FModel/언리얼 원본 머티리얼 참조 정보
struct MaterialRef
{
    string objectName{};
    string objectPath{};
    string assetName{};
    MaterialRefKind kind = MaterialRefKind::Unknown;
};

// SK, SM, FX 중 어디서 나온 매핑인지 구분
enum class MaterialSourceKind : uint8
{
    SK,
    SM,
    FX,
};

// 모델 materialIndex 또는 FX renderer slot이 어떤 MaterialRef를 가리키는지 저장
struct MaterialMapping
{
    MaterialSourceKind source = MaterialSourceKind::SK;
    string sourceFile{};

    uint32 materialIndex = 0;
    uint32 originalMaterialIndex = 0;
    string slotName{};

    string emitter{};
    string rendererType{};
    uint32 rendererIndex = 0;
    uint32 slotIndex = 0;

    MaterialRef material{};
};

// MI_.json 파싱 결과 + Material_View  편집 상태 + Save Bundle 용 상태 + Apply 시 명령 기준이 되는 구조체
struct MaterialInstanceData
{
    string assetName{};

    Engine::MaterialShaderTemplate templateId = Engine::MaterialShaderTemplate::OpaquePBR;
    Engine::MaterialRenderState renderState{};
    // ModelCom instance override에서 renderState를 덮어쓸지 여부
    bool renderStateOverrideEnabled = false;
    // -1이면 자동, 0 이상이면 해당 pass를 강제한다.
    int forcedPassIndex = -1;
    Engine::MaterialCB cb{};

    // instance 편집 시 "필드별 override" 기준. (legacy 없음)
    struct MaterialCBOverrideMask
    {
#define EDITOR_CB_MASK_FIELD(_key, _member, _kind) bool _member = false;
        MATERIAL_CB_OVERRIDE_FIELDS(EDITOR_CB_MASK_FIELD)
#undef EDITOR_CB_MASK_FIELD

        bool Any() const
        {
            bool any = false;
#define EDITOR_CB_MASK_ANY(_key, _member, _kind) any = any || _member;
            MATERIAL_CB_OVERRIDE_FIELDS(EDITOR_CB_MASK_ANY)
#undef EDITOR_CB_MASK_ANY
            return any;
        }

        void Clear()
        {
#define EDITOR_CB_MASK_CLEAR(_key, _member, _kind) _member = false;
            MATERIAL_CB_OVERRIDE_FIELDS(EDITOR_CB_MASK_CLEAR)
#undef EDITOR_CB_MASK_CLEAR
        }
    };

    MaterialCBOverrideMask cbMask{};

    // Bundle(.material.json) 저장용 기본 슬롯 사용 여부.
    bool baseSlotEnabledBaseColor = false;
    bool baseSlotEnabledNormal = false;
    bool baseSlotEnabledOrm = false;
    bool baseSlotEnabledEmissive = false;
    bool baseSlotEnabledOpacityMask = false;
    bool baseSlotEnabledHairDepth = false;
    bool baseSlotEnabledHairID = false;
    bool baseSlotEnabledIrisHeight = false;
    bool baseSlotEnabledIrisMask = false;

    // ModelCom instance override 적용용 슬롯 사용 여부.
    bool instanceOverrideEnabledBaseColor = false;
    bool instanceOverrideEnabledNormal = false;
    bool instanceOverrideEnabledOrm = false;
    bool instanceOverrideEnabledEmissive = false;
    bool instanceOverrideEnabledOpacityMask = false;
    bool instanceOverrideEnabledHairDepth = false;
    bool instanceOverrideEnabledHairID = false;
    bool instanceOverrideEnabledIrisHeight = false;
    bool instanceOverrideEnabledIrisMask = false;

    wstring baseColorPath{};
    wstring normalPath{};
    wstring ormPath{};
    wstring emissivePath{};
    wstring opacityMaskPath{};
    wstring hairDepthPath{};
    wstring hairIDPath{};
    wstring irisHeightPath{};
    wstring irisMaskPath{};
};

// 파싱/추론 과정에서 남기는 에디터용 메모
struct HeuristicNote
{
    string kind{};
    string detail{};
};

NS_END
