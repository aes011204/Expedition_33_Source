# Material System (Base `.material.json` + Object Instance Overrides)

이 문서는 **이 대화에서 정리/적용한 “Material(자산) + MaterialInstance(오브젝트별)” 구조**를 기준으로,
런타임/에디터에서 어떤 구조체/함수들이 어떤 목적과 흐름으로 쓰이는지 정리합니다.

> 핵심 목표  
> - `.material.json`(Base 자산)을 바꾸면 “그 자산을 쓰는 모든 오브젝트”에 일괄 반영  
> - 오브젝트(ModelCom)에서 **사용자가 명시적으로 수정한 값만** Instance override로 남음  
> - 런타임 프레임마다 파일 IO/JSON 파싱 없음 (로드/변경 시점에만 적용)

---

## 1) 큰 그림: Base vs Instance

### Base (자산)
- 파일: `Client/Bin/Resources/Materials/*.material.json`
- 의미: “어떤 모델(materialIndex)이 어떤 MI(=assetName) 인스턴스를 쓰는지 + 그 MI의 기본 값”
- 적용 대상: 동일 bundleKey를 쓰는 모든 오브젝트

### Instance (오브젝트별 오버라이드)
- 저장 위치: **ModelCom이 포함된 오브젝트 JSON(프리팹/맵 등)**의 `material_bindings`
- 의미: “이 오브젝트만 base 위에 덮어쓸 값들(텍스처/렌더상태/CB 일부 필드)”
- 적용 대상: 해당 오브젝트 1개

---

## 2) 런타임 구성요소

### 2.1 `Engine::Material`
- 파일: `Engine/Public/Material.h`
- 역할: 렌더에 실제로 바인딩되는 최종 Material 상태를 들고 있음
  - `Set_RenderState(const MaterialRenderState&)`
  - `Set_CB(const MaterialCB&)`
  - `Override_TextureSlot(MaterialTextureSlot slot, uint32 index, const wstring& path)`
- NOTE: opacity mask 슬롯은 텍스처가 없으면 아예 사용하지 않는 정책을 유지하기 위해 빈 문자열을 전달해서 clear 한다.

### 2.2 `Engine::MaterialBuilder` (자산 캐시 + JSON 파서)
- 파일: `Engine/Public/MaterialBuilder.h`, `Engine/Private/MaterialBuilder.cpp`
- 역할:
  1) `MaterialsRoot` 아래 `*.material.json`을 재귀 스캔 → `key(stem)->path` 인덱싱
  2) 번들(자산)을 1회 파싱해서 캐시 (`GetOrLoadBundle`)
  3) 오브젝트 JSON의 `material_bindings`/`material_overrides`를 파싱해서 ModelCom의 binding 구조를 채움
- 생성/소유:
  - `Asset_Manager`가 `MaterialBuilder::Create()`로 생성해서 소유
  - ModelCom은 `GAME->Get_AssetManager()->Get_MaterialBuilder()`를 통해 접근

#### 2.2.1 key 규칙
- 기본 규칙: `bundleKey = model stem(lowercase)`
- `.material.json`이 `Foo.material.json`처럼 `.material` suffix를 포함해도,
  - `To_Key()`에서 `.material` suffix 제거 + lowercase 정규화

#### 2.2.2 자산 캐시 구조 (`BundleAsset`)
- `MaterialBuilder::BundleAsset`
  - `materialIndexToAssetName`: `materialIndex -> assetName(MI)`
  - `instancesByAssetName`: `assetName -> BaseInstanceData(renderState/cb/texture paths + overrides flags)`
- 적용 시점에는 ModelCom이 materialIndex로 해당 base instance를 찾아 base를 적용합니다.

#### 2.2.3 오브젝트 JSON 파서 API
- `bool Try_ApplyMaterialBindingsFromJson(ModelCom& model, const json& data) const;`
  - `material_bindings` 섹션을 읽어 `model.Get_MaterialBindings()`를 채움
  - CB는 **필드별 override만 지원**(레거시 없음)
- `bool Try_ApplyModelMaterialOverridesFromJson(ModelCom& model, const json& data) const;`
  - legacy `material_overrides[]`를 instance의 texture override로 흡수
  - GUID가 있으면 `GAME->Resolve_AssetPath(guid)`로 경로를 복원
- `bool Try_GetBaseCBForMaterial(std::string_view bundleKey, uint32 materialIndex, MaterialCB& outCB) const;`
  - 에디터(Edit Instance)에서 base 값을 “읽기 전용 프리뷰”로 보여주기 위한 헬퍼

### 2.3 `Engine::ModelCom` (binding 저장소 + 합성 적용)
- 파일: `Engine/Public/ModelCom.h`, `Engine/Private/ModelCom.cpp`
- 역할:
  - 모델 로드 후 Material 슬롯 수에 맞게 binding 저장소 준비
  - base(bundle) + instance(override)를 **이벤트 기반으로 합성 적용**
  - 오브젝트 JSON 저장 시 `material_bindings` 직렬화

#### 2.3.1 binding 데이터 구조
- `ModelCom::MaterialBinding`
  - `string baseBundleKey`
  - `MaterialInstanceOverride instance`
- `_materialBindings`는 `Get_NumMaterials()`와 동일 크기를 유지
- `Ensure_MaterialBindings()`:
  - `_materials.size()`에 맞춰 `_materialBindings.resize()`
  - 기본 `baseBundleKey = model stem(lowercase)`

#### 2.3.2 instance override 구조
- `ModelCom::MaterialInstanceOverride`
  - `overrideRenderStateEnabled + renderState`
  - `cbMask + cb` (**CB는 필드별 mask 기반**)
  - texture overrides: `TextureOverride { enabled, path }`를 슬롯별로 보관

#### 2.3.3 적용 흐름(중요)
- `Initialize_Prototype()`에서 `.model` 로드 완료 후:
  - `Ensure_MaterialBindings()`
  - `Apply_BaseMaterialFromBundle_AllSlots()` (slot 전체 1회)
- `From_Json()`에서:
  - (model reload 여부 결정 후)
  - `MaterialBuilder`가 `material_bindings`/`material_overrides`를 파싱해 `_materialBindings`를 채움
  - `Apply_BaseMaterialFromBundle_AllSlots()`로 base+instance 합성 반영

#### 2.3.4 `Apply_EffectiveMaterial(materialIndex)` 합성 규칙
1) **Base 적용**
   - `MaterialBuilder->GetOrLoadBundle(binding.baseBundleKey)`
   - `bundle.materialIndexToAssetName[materialIndex]`로 base instance 선택
   - `material->Set_RenderState(base.renderState)`
   - `material->Set_CB(base.cb)`
   - 텍스처는 base override flag가 true인 슬롯만 `Override_TextureSlot`
   - opacity mask는 base override가 false면 `Override_TextureSlot(OpacityMask, 0, L"")`로 clear
2) **Instance 적용**
   - renderState: `overrideRenderStateEnabled`가 true면 덮어씀
   - CB: `cbMask.Any()==true`이면
     - base 적용 이후의 CB를 복사한 `effective`에 “mask가 켜진 필드만” 덮어쓰고 `Set_CB(effective)`
   - 텍스처: slot별 `TextureOverride.enabled==true`만 덮어씀

---

## 3) CB 필드별 override: 확장 가능한 단일 소스

### 3.1 필드 목록의 단일 소스 (X-macro)
- 파일: `Engine/Public/MaterialCB_OverrideFields.inl`
- 목적:
  - PBR/HDR 등으로 `MaterialCB` 필드가 늘어도 “목록 1줄 추가”로 확장되게 함
  - 런타임(ModelCom)과 에디터(Material_View)가 같은 필드 집합을 공유

### 3.2 mask 저장 방식(용량/명확성)
- `cb_overrides`는 **true인 필드만** JSON에 기록
- `cb`도 **override가 켜진 필드만** JSON에 기록
- override 체크되지 않은 필드는 JSON에 없고, base를 따라갑니다.

---

## 4) JSON 포맷(현재 기준)

### 4.1 `.material.json` (Base asset bundle)
- 주요 섹션:
  - `instances[]`: `assetName`(MI) 별 base 데이터(텍스처/CB/RenderState + overrides flags)
  - `mappings[]`: `materialIndex -> ref.assetName` 연결

### 4.2 ModelCom JSON: `material_bindings` (Instance)
예시(개념):
```json
{
  "material_bindings": [
    {
      "material_index": 0,
      "base_bundle_key": "lunebody",
      "instance_override": {
        "override_render_state_enabled": true,
        "render_state": { "twoSided": true, "blendMode": 1 },
        "cb_overrides": { "baseColorTint": true, "normalStrength": true },
        "cb": {
          "baseColorTint": [1, 0.5, 0.5, 1],
          "normalStrength": 0.8
        },
        "textures": {
          "base_color": { "enabled": true, "texture_guid": "..." }
        }
      }
    }
  ]
}
```

### 4.3 ModelCom JSON: `material_overrides` (legacy)
- 예전 포맷이 남아있는 경우를 위해 파서는 유지하지만,
- 내부적으로는 `material_bindings[].instance_override.textures`로 흡수됩니다.

---

## 5) 에디터(Material_View) 흐름

### 5.1 모드 개요
- `Create With Json`: MappingSrc(SK/SM/FX) → MI 해석 → `.material.json` 생성/저장
- `Edit With material.json`: 번들 파일을 직접 선택해서 로드/편집/저장
- `Edit With ModelCom`: 선택된 ModelCom을 기준으로 “instance override”를 편집/적용

### 5.2 Selected ModelCom UI
- Mesh → MaterialIndex 매핑을 먼저 보여줘서 “어느 mesh가 어느 슬롯을 쓰는지” 혼동을 줄입니다.
- Material slot dropdown으로 편집할 슬롯을 고릅니다.

### 5.3 Edit Instance(=Edit With ModelCom)에서 CB 오버라이드 기준
- 각 CB 필드마다:
  - `[Override]` 체크박스
  - `Base:` 프리뷰(가능하면 bundleKey 기반으로 base 값을 조회)
  - 체크 ON일 때만 instance 값 편집 활성화
- `Clear Instance Overrides (slot)`:
  - 해당 슬롯의 CB mask 전부 OFF + 텍스처 override 플래그 OFF

### 5.4 Apply 동작
- `Apply To Selected ModelCom`은 (Edit With ModelCom 모드에서)
  - runtime Material을 직접 “base처럼” 덮지 않고,
  - ModelCom의 `MaterialInstanceOverride(cbMask+cb+textures)`를 갱신한 다음
  - `Apply_EffectiveMaterial(materialIndex)`로 합성 적용합니다.

---

## 6) 실무 체크리스트

### base를 수정했는데 일부 오브젝트가 따라오지 않는다
- 해당 오브젝트의 ModelCom JSON에서 `cb_overrides`에 그 필드가 켜져 있는지 확인
- 켜져 있다면 “의도적으로 instance가 base를 덮는 상태”입니다.

### instance 값을 껐는데도 렌더가 이상하다(특히 opacity mask)
- opacity mask는 “없으면 clear” 정책이므로, base/instance에서 override가 꺼졌을 때 슬롯이 비어야 정상입니다.

### 새로운 PBR/HDR 필드를 추가하고 싶다
1) `Engine/Public/Shader_CBuffer_Define.h`의 `MaterialCB`에 필드 추가
2) `Engine/Public/MaterialCB_OverrideFields.inl`에 필드 1줄 추가
3) UI/JSON/런타임 적용이 동일 필드 목록을 통해 확장되는지 확인

