# MapEditor ModelOverride Material 확장 기술서

## 1. 목적

새 MaterialView 창을 만드는 것이 아니다. 기존 MapEditor의 `ModelOverride` 흐름을 확장해서 MainEditor `Material_View`의 `Edit With ModelCom` 계열 기능을 흡수한다.

최종 목표는 다음이다.

```text
MapEditor 오브젝트 선택
-> PanelModelOverride에서 선택 모델의 material 상태 편집
-> Apply로 현재 MapEditor 렌더 핸들에 즉시 반영
-> Save로 Materials/<MapName>/<ModelStem>.material.json 저장
-> Import/Export/MapLoader에서 기존 override와 신규 material_bindings 모두 복원
```

핵심 원칙:

- `PanelModelOverride`를 확장한다.
- `ModelOverrideEntry`를 확장한다.
- 기존 `passIndex`, `textureSlots`, `materialCB` 구조는 유지한다.
- 신규 `material_bindings` 구조를 `ModelOverrideEntry`에 추가한다.
- 기존 v1 `instances/mappings` material json도 계속 읽는다.
- 신규 저장은 v2 `material_bindings`를 우선한다.

## 2. 확정 정책

- 저장 단위는 `model 단위`다.
- 같은 맵 안에서 같은 모델을 쓰는 오브젝트는 같은 material override를 공유한다.
- per-object material override는 이번 범위에서 제외한다.
- 현재 map name은 `SceneContext::_mapName` 전역 기준으로 쓴다.
- 쓸데없는 json 파일이 생기지 않도록 실제 override가 있는 모델만 저장한다.
- 저장/로드/편집 버튼은 기존 `PanelModelOverride` 안에 둔다.
- 별도 `PanelMaterialView` 또는 새 MaterialView 창은 만들지 않는다.
- Map object enum/string 계약을 material json에 같이 저장해서 파싱 문제를 막는다.

## 3. 현재 구조

현재 관련 구조는 다음이다.

- `SceneContext::_selected`
  - 현재 선택된 `EditorObject`
- `SceneContext::_mapName`
  - 현재 맵 이름
- `SceneContext::_modelOverrides`
  - model filename 기준 override 저장소
- `PanelModelOverride`
  - 현재 pass / texture slot override UI
- `ModelOverrideEntry`
  - 현재 legacy override 데이터
- `ModelOverrideEntry::ExportToNativeMaterial`
  - 현재 `Materials/<MapName>/<ModelStem>.material.json` 저장 함수
- `SceneContext::Import_MapInstance`
  - `Materials/<MapName>` 폴더 material json 읽기
- `SceneContext::Export_MapInstance`
  - `MapInstance/<MapName>` 분할 저장 및 material export

이번 작업은 이 구조를 버리지 않고 확장한다.

## 4. ModelOverrideEntry 확장

현재 구조:

```cpp
struct ModelOverrideEntry
{
    ShaderType shaderType = ShaderType::VtxMesh;
    int32 passIndex = -1;
    map<int32, string> textureSlots;
    bool hasMaterialCB = false;
    Engine::MaterialCB materialCB{};

    void ExportToNativeMaterial(const string& modelPath, const string& mapName) const;
};
```

확장 구조:

```cpp
enum class EMapMaterialObjectKind : uint8
{
    StaticMesh,
    InstancedMesh,
    Landscape,
    Water,
    Unsupported,
};

enum class EMapMaterialSchema : uint8
{
    LegacyInstancesMappings = 1,
    MaterialBindingsV2 = 2,
};

struct ModelOverrideEntry
{
    ShaderType shaderType = ShaderType::VtxMesh;

    // 기존 legacy override. 유지한다.
    int32 passIndex = -1;
    map<int32, string> textureSlots;
    bool hasMaterialCB = false;
    Engine::MaterialCB materialCB{};

    // 신규 ModelCom material override.
    bool hasMaterialBindings = false;
    EMapMaterialObjectKind materialObjectKind = EMapMaterialObjectKind::Unsupported;
    string baseBundleKey{};
    json materialBindingsJson = json::array();
    json materialMetaJson = json::object();

    bool HasLegacyOverride() const;
    bool HasMaterialBindingsOverride() const;
    bool HasAnyMaterialOverride() const;

    void LoadLegacyMaterialJson(const json& root);
    void LoadMaterialBindingsJson(const json& root);
    json BuildMaterialJson(const string& modelPath, const string& mapName) const;
    void ExportToNativeMaterial(const string& modelPath, const string& mapName) const;
};
```

분기 규칙:

```text
hasMaterialBindings == true
-> v2 material_bindings 포맷 저장/적용

hasMaterialBindings == false && legacy override 존재
-> 기존 v1 instances/mappings 포맷 저장/적용

override 없음
-> material json 생성하지 않음
```

## 5. Map object enum 계약

`EObjectType`은 에디터 내부 타입이고, material json에는 별도 문자열을 저장한다.

```cpp
EMapMaterialObjectKind To_MapMaterialObjectKind(EObjectType type)
{
    switch (type)
    {
    case EObjectType::StaticMesh:
        return EMapMaterialObjectKind::StaticMesh;
    case EObjectType::InstancedMesh:
        return EMapMaterialObjectKind::InstancedMesh;
    case EObjectType::Landscape:
        return EMapMaterialObjectKind::Landscape;
    case EObjectType::Water:
        return EMapMaterialObjectKind::Water;
    default:
        return EMapMaterialObjectKind::Unsupported;
    }
}

const char* To_String(EMapMaterialObjectKind kind);
EMapMaterialObjectKind Parse_MapMaterialObjectKind(const string& value);
```

저장 예시:

```json
{
  "schema": "map_model_material",
  "version": 2,
  "map_object_kind": "StaticMesh"
}
```

사용 목적:

- StaticMesh와 InstancedMesh pass option 분리
- 잘못된 object type에 material json이 적용되는 문제 방지
- v1 legacy 파일과 v2 신규 파일 분기
- 이후 Landscape/Water 확장 시 파싱 안정성 확보

## 6. 저장 경로 계약

현재 맵 이름이 `<MapName>`, 모델 stem이 `<ModelStem>`이면 저장 경로는 다음이다.

```text
Client/Bin/Resources/Materials/<MapName>/<ModelStem>.material.json
```

예시:

```text
Client/Bin/Resources/Materials/SmallLevel_MF_Zone_01_Mk1/Cloud_Mesh.material.json
```

규칙:

- `<MapName>`은 `SceneContext::_mapName` 기준이다.
- `<ModelStem>`은 선택 오브젝트의 실제 model path 기준이다.
- 같은 모델은 material 파일 하나만 가진다.
- override가 없는 모델은 파일을 만들지 않는다.

## 7. 선택 모델 해석

`PanelModelOverride`에서 선택된 오브젝트를 기준으로 target context를 계산한다.

```cpp
struct ModelOverrideTargetContext
{
    Shared<EditorObject> object;
    Shared<ModelCom> model;

    string modelFilename;      // Cloud_Mesh.model
    string modelStem;          // Cloud_Mesh
    fs::path modelPath;

    string mapName;
    string baseBundleKey;

    EObjectType objectType = EObjectType::Fail;
    EMapMaterialObjectKind materialObjectKind = EMapMaterialObjectKind::Unsupported;

    fs::path materialDir;
    fs::path materialFile;
};
```

StaticMesh:

```cpp
obj->_type == EObjectType::StaticMesh
obj->_modelPath
obj->_renderHandle->GetModel()
```

InstancedMesh:

```cpp
obj->_type == EObjectType::InstancedMesh
obj->_instancedModelPath
obj->_instancedRenderHandle->GetModel()
```

`MEInstancedMeshActor`에 `GetModel()`이 없다면 `MEStaticMeshActor`처럼 추가한다.

지원 제외:

- SpawnPoint
- EncounterZone
- TriggerVolume
- CameraZone
- PostProcessVolume
- Light

Landscape/Water는 별도 파이프라인으로 두고 이번 `ModelCom` material override 대상에서는 막는다.

## 8. base bundle key 계산

분할 map material 파일은 `Materials/<MapName>`에 저장하지만, base texture는 모델 원본 폴더의 material bundle에서 온다.

```cpp
string Build_BaseBundleKey(const fs::path& sourceModelPath)
{
    string key = sourceModelPath.parent_path().filename().string();
    if (key.empty())
        key = sourceModelPath.stem().string();
    return String::ToLowerCopy(key);
}
```

예시:

```text
modelPath = Client/Bin/Resources/Map/SmallLevel_MF_Zone_01/Cloud_Mesh.model
baseBundleKey = smalllevel_mf_zone_01
```

이 값을 `SmallLevel_MF_Zone_01_Mk1`로 잡으면 기존 texture base를 못 찾을 수 있다.

## 9. PanelModelOverride UI 확장

기존 `PanelModelOverride` UI는 유지한다.

추가 섹션:

```text
Selected Model Material
```

표시 정보:

- Selected Object
- Object Type
- Material Object Kind
- Model Path
- Material File
- Base Bundle Key
- Material Slot Count

추가 버튼:

- `Load From Selected Model`
  - 선택 렌더 핸들의 `ModelCom` 현재 material 상태를 편집 상태로 가져온다.
- `Load Map Material`
  - `Materials/<MapName>/<ModelStem>.material.json`을 읽어 `_modelOverrides[modelFilename]`와 선택 모델에 적용한다.
- `Apply To Selected`
  - `_modelOverrides[modelFilename]`의 현재 값을 선택 모델에 즉시 적용한다.
- `Save Map Material`
  - `_modelOverrides[modelFilename]`를 material json으로 저장한다.
- `Clear Material Override`
  - 신규 material binding override만 비운다. legacy pass/texture/CB 삭제 여부는 별도 확인을 둔다.

MainEditor `Material_View`에서 가져올 기능:

- material slot 목록
- mesh -> materialIndex 표시
- pass 선택
- render state override
- CB field override
- texture slot override
- `Set_InstanceOverride()` + `Apply_EffectiveMaterial()` 적용 방식

가져오지 않을 기능:

- MainEditor `Editor_Context` 접근
- MainEditor `Notification_Manager`
- MainEditor 전용 file dialog 기본 경로
- `Create With Json`
- 공용 bundle 생성용 `Save_BundleFile()` 직접 사용

## 10. 신규 v2 저장 포맷

`ModelOverrideEntry::BuildMaterialJson()`가 만드는 기본 포맷:

```json
{
  "schema": "map_model_material",
  "version": 2,
  "map_name": "SmallLevel_MF_Zone_01_Mk1",
  "map_object_kind": "StaticMesh",
  "editor_modelPath": "../../../Client/Bin/Resources/Map/SmallLevel_MF_Zone_01/Cloud_Mesh.model",
  "editor_modelStem": "Cloud_Mesh",
  "base_bundle_key": "smalllevel_mf_zone_01",
  "material_bindings": [
    {
      "material_index": 0,
      "base_bundle_key": "smalllevel_mf_zone_01",
      "instance_override": {
        "force_pass_index": -1,
        "cb_overrides": {
          "baseColorTint": true,
          "roughnessScale": true
        },
        "cb": {
          "baseColorTint": [1.0, 1.0, 1.0, 1.0],
          "roughnessScale": 1.0
        },
        "textures": {
          "base_color": {
            "enabled": true,
            "texture_path": "Map/MapInstance/SmallLevel_MF_Zone_01_Mk1/Textures/BaseColor/Cloud_Base.png"
          }
        }
      }
    }
  ]
}
```

저장 규칙:

- `material_bindings`만 신규 material override의 핵심 데이터다.
- `ModelCom::To_Json()` 전체를 저장하지 않는다.
- `base_bundle_key`는 binding마다 명시한다.
- `map_object_kind`는 문자열로 저장한다.
- override가 없으면 저장하지 않는다.

## 11. 기존 v1 호환

기존 파일:

```json
{
  "instances": [
    {
      "assetName": "Cloud_Mesh_Inst",
      "editor_modelPath": "../../../Client/Bin/Resources/Map/SmallLevel_MF_Zone_01/Cloud_Mesh.model",
      "editor_shaderType": 2,
      "forcePassIndex": -1,
      "cb": {},
      "textures": {}
    }
  ],
  "mappings": [
    {
      "materialIndex": 0,
      "ref": {
        "assetName": "Cloud_Mesh_Inst"
      }
    }
  ]
}
```

호환 정책:

- v1 파일을 읽으면 `ModelOverrideEntry`의 legacy 필드를 채운다.
- 필요하면 v1을 내부 v2 편집 상태로 변환할 수 있다.
- v2 편집/저장 이후에는 `hasMaterialBindings = true`가 되고 v2로 저장한다.
- v2 데이터가 없으면 기존 v1 저장을 유지한다.

## 12. Apply 구현

`Apply To Selected`는 `_modelOverrides[modelFilename]` 값을 선택 모델에 반영한다.

v2 경로:

```cpp
json materialJson = entry.BuildMaterialJson(modelPath, mapName);
model->From_Json(materialJson);
```

또는 UI 편집 중 즉시 반영:

```cpp
model->Set_InstanceOverride(materialIndex, overrideData);
model->Apply_EffectiveMaterial(materialIndex);
```

legacy 경로:

- `passIndex`는 현재 material 전체 또는 slot 단위에 forced pass로 적용
- `textureSlots`는 기존 `Override_TextureSlot` 경로 유지
- `materialCB`는 기존 `hasMaterialCB` 기준 적용

중요:

- Apply는 저장이 아니다.
- Save를 눌러야 `Materials/<MapName>/<ModelStem>.material.json`에 남는다.

## 13. Save 구현

`Save Map Material` 절차:

1. 선택 오브젝트에서 `ModelOverrideTargetContext` 계산
2. `_modelOverrides[modelFilename]`를 얻거나 생성
3. 기존 legacy 필드는 유지
4. UI 편집 상태를 `ModelOverrideEntry` 신규 필드에 반영
5. `HasAnyMaterialOverride()` 검사
6. override가 없으면 저장하지 않음
7. `Materials/<MapName>` 폴더 생성
8. `ExportToNativeMaterial(modelPath, mapName)` 호출

`ExportToNativeMaterial()` 분기:

```text
hasMaterialBindings
-> v2 material_bindings 저장

legacy override 있음
-> 기존 v1 instances/mappings 저장

override 없음
-> 파일 생성 안 함
```

## 14. Load 구현

`Load Map Material` 절차:

1. 선택 오브젝트에서 target context 계산
2. material file 존재 확인
3. json parse
4. v2면 `entry.LoadMaterialBindingsJson(root)`
5. v1이면 `entry.LoadLegacyMaterialJson(root)`
6. `_modelOverrides[modelFilename] = entry`
7. `map_object_kind`가 현재 선택 object와 호환되는지 확인
8. 선택 모델에 Apply
9. UI 편집 상태 갱신

호환 체크:

```text
map_object_kind == StaticMesh    -> EObjectType::StaticMesh만 허용
map_object_kind == InstancedMesh -> EObjectType::InstancedMesh만 허용
map_object_kind 없음             -> legacy로 보고 model filename 기준으로 허용
```

## 15. Export_MapInstance 변경

현재 `SceneContext::Export_MapInstance()`는 unique model 목록을 만들고 모든 모델에 대해 default material export를 호출할 수 있다.

문제:

- override가 없는 모델에도 json이 생긴다.
- 모델 단위 material 파일이 많아진다.
- 나중에 파싱할 때 의미 없는 파일이 섞인다.

변경:

```cpp
for (const auto& [modelPathW, model] : uniqueModels)
{
    string modelPath = String::ToString(modelPathW);
    string filename = fs::path(modelPath).filename().string();

    auto it = _modelOverrides.find(filename);
    if (it == _modelOverrides.end())
        continue;

    if (!it->second.HasAnyMaterialOverride())
        continue;

    it->second.ExportToNativeMaterial(modelPath, mapName);
}
```

즉 `defaultEntry.ExportToNativeMaterial()`는 제거한다.

## 16. Import_MapInstance 변경

`SceneContext::Import_MapInstance()`는 `Materials/<MapName>`을 읽을 때 v1/v2를 모두 `_modelOverrides`에 넣는다.

분기:

```text
root contains "material_bindings"
-> ModelOverrideEntry::LoadMaterialBindingsJson(root)

root contains "instances" and "mappings"
-> ModelOverrideEntry::LoadLegacyMaterialJson(root)
```

키:

```text
editor_modelPath filename 우선
없으면 material file stem + ".model"
```

결과:

```cpp
_modelOverrides[modelFilename] = entry;
```

## 17. Render handle 적용

MapEditor 화면에 적용할 때는 선택 object의 render handle model에 적용한다.

대상:

- StaticMesh: `obj->_renderHandle->GetModel()`
- InstancedMesh: `obj->_instancedRenderHandle->GetModel()`

적용 시점:

- `Load Map Material`
- `Apply To Selected`
- Import 후 render handle 생성/갱신 시
- texture/pass/CB 편집 중 live apply가 필요할 때

권장 helper:

```cpp
bool Apply_ModelOverrideToEditorObject(
    SceneContext& scene,
    const Shared<EditorObject>& obj);
```

이 helper는 `modelFilename`으로 `_modelOverrides`를 찾고 legacy/v2 중 맞는 경로로 적용한다.

## 18. MapLoader와 런타임

MapLoader는 `Materials/<MapName>` 폴더를 읽는다.

필수 규칙:

- v2 `material_bindings`가 있으면 그대로 `ModelCom::From_Json()` 적용
- v1 `instances/mappings`가 있으면 adapter 변환
- base material은 먼저 원본 model json의 `StaticMaterials` 이름으로 적용
- 그 뒤 map material override를 적용
- `base_bundle_key`는 model source parent folder 기준으로 보정

이 순서를 유지해야 검은 material 문제가 재발하지 않는다.

## 19. 구현 순서

1. `EMapMaterialObjectKind`, `EMapMaterialSchema` 및 변환 함수 추가
2. `ModelOverrideEntry`에 v2 material binding 필드 추가
3. `HasLegacyOverride()`, `HasMaterialBindingsOverride()`, `HasAnyMaterialOverride()` 구현
4. `LoadLegacyMaterialJson()`, `LoadMaterialBindingsJson()`, `BuildMaterialJson()` 구현
5. `PanelModelOverride`에 선택 모델 material 편집 섹션 추가
6. StaticMesh target context resolve 구현
7. InstancedMesh target context resolve 구현
8. MainEditor `Material_View`의 `EditWithModelCom` 편집/Apply 로직을 `PanelModelOverride` 안으로 이식
9. `Save Map Material` 구현
10. `Load Map Material` 구현
11. `SceneContext::Export_MapInstance()` default material export 제거
12. `SceneContext::Import_MapInstance()` v1/v2 분기 로드 구현
13. MapLoader v2 direct read 보강
14. static mesh, instanced mesh, 기존 v1 파일, 기존 map export/import 회귀 테스트

## 20. 검증 체크리스트

### StaticMesh

1. `SmallLevel_MF_Zone_01_Mk1` import
2. StaticMesh 선택
3. `PanelModelOverride`에서 material 편집
4. `Apply To Selected`
5. 화면 반영 확인
6. `Save Map Material`
7. `Materials/SmallLevel_MF_Zone_01_Mk1/<ModelStem>.material.json` 생성 확인
8. MapEditor 재import 후 반영 확인
9. Client MapLoader 로드 후 반영 확인

### InstancedMesh

1. InstancedMesh 선택
2. pass/texture/CB override 편집
3. `Apply To Selected`
4. instance group 전체 반영 확인
5. 저장/재로드 확인

### 불필요 파일 방지

1. override 없는 모델 선택
2. `Save Map Material`
3. 저장 차단 확인
4. `Export_MapInstance` 후 override 없는 모델 json이 생기지 않는지 확인

### enum 파싱

1. `map_object_kind`를 `StaticMesh`로 저장
2. InstancedMesh에 잘못 적용 시 차단 확인
3. `map_object_kind` 없는 v1 파일은 legacy로 정상 로드 확인

## 21. 결론

- 새 MaterialView 창을 만들지 않는다.
- 기존 `PanelModelOverride`와 `ModelOverrideEntry`를 확장한다.
- 기존 pass/texture/CB override는 유지한다.
- 신규 `material_bindings` override를 같은 entry에 추가한다.
- 저장은 model 단위다.
- 현재 map name은 `SceneContext::_mapName`을 쓴다.
- override가 있는 모델만 material json을 만든다.
- `map_object_kind` enum/string 계약을 저장한다.
- v2는 `material_bindings`, v1은 `instances/mappings` 호환으로 간다.
