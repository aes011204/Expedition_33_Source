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

    // 슬롯별로 실제로 적용할지 여부를 따로 들고있다
    // 경로만 비어있는 경우(미해결)와, 사용자가 일부러 비운 경우(지우기)를 구분하려고 필요하다
    bool baseColorOverride = false;
    bool normalOverride = false;
    bool ormOverride = false;
    bool emissiveOverride = false;
    bool opacityMaskOverride = false;

    wstring baseColorPath{};
    wstring normalPath{};
    wstring ormPath{};
    wstring emissivePath{};
    wstring opacityMaskPath{};
};

// 파싱/추론 과정에서 남기는 에디터용 메모
struct HeuristicNote
{
    string kind{};
    string detail{};
};

NS_END
