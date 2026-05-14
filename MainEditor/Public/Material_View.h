#pragma once

#include "Editor_Window.h"
#include "Material_Types.h"
#include "Material_BundleTypes.h"
#include "Editor_Defines.h"

NS_BEGIN(Engine)
class ModelCom;
NS_END

NS_BEGIN(MainEditor)

class Material_View final : public Editor_Window
{
public:
    Material_View();
    ~Material_View() override = default;

public:
    HRESULT Initialize() override;
    void    Update(float timeDelta) override;
    void    Pre_Render() override;
    void    Render() override;

private:
    /*
        입력
        - SK_*, SM_*, FX_* .json에서 MaterialInterface를 모아 .material.json으로 뽑는다.

        적용
        - 선택된 오브젝트(ModelCom)에 텍스처만 덮어쓰거나 결과를 저장한다.
    */

    enum class CreateMode : uint8
    {
        CreateWithJson, // json 기반 자동 생성
        EditWithBundle, // .material.json 번들 편집
        EditWithModelCom, // 선택된 ModelCom 기반 편집
        CreateMaterial, // (legacy) 수동 생성
    };

private:
    // 선택한  .json을 읽어서 material mapping을 만든다.
    bool Load_SkFile(const wstring& skFilePath);
    bool Load_SmFile(const wstring& smFilePath);
    bool Load_FxFile(const wstring& fxFilePath);

    // mapping에 들어있는 MI_*만 대상으로 읽고 최종 데이터를 만든다.
    bool Build_MaterialInstances();

    // 선택된 GameObject의 ModelCom에 텍스처를 Override_TextureSlot로 적용한다.
    void Apply_ToSelectedModel(int onlyMaterialIndex = -1);

    // .material.json 번들을 읽어 현재 편집 상태로 로드한다.
    bool Load_BundleFile(const wstring& bundlePath);
    // 선택된 ModelCom에서 현재 머테리얼 상태를 편집 상태로 가져온다.
    bool Load_FromSelectedModelCom();

    // 파일 대화창
    static bool Try_PickFolderDialog(const wstring& title, wstring& outFolderPath);
    static bool Try_PickJsonFileDialog(const wstring& title, const wstring& defaultFolder, wstring& outFilePath);
    static bool Try_PickTextureFileDialog(const wstring& title, const wstring& defaultFolder, wstring& outFilePath);

    bool Save_BundleFile(const wstring& outputPath) const;

    void Draw_ModeSelector();
    void Draw_Settings();
    void Draw_CreateWithJson();
    void Draw_EditWithBundle();
    void Draw_EditWithModelCom();
    void Draw_CreateMaterial();

    // 선택된 오브젝트의 ModelCom을 기준으로,
    // - Mesh -> MaterialIndex 관계를 보여주고
    // - 편집 대상 materialIndex를 하나 고른다.
    // return: 필터링에 사용할 materialIndex, 선택 실패면 -1
    int Draw_SelectedModelSection(Shared<Engine::ModelCom>& outSelectedModel);
    void Draw_MappingsEditorUI(int selectedMaterialIndexFilter, const Shared<Engine::ModelCom>& selectedModel);

private:
    CreateMode _mode = CreateMode::CreateWithJson;

    // 경로
    wstring _originRoot = L"../../../Client/Bin/Resources/Materials/Origins/MappingSrc";
    wstring _miRoot = L"../../../Client/Bin/Resources/Materials/Origins/InstanceData";
    wstring _textureSearchRoot = L"../../../Client/Bin/Resources/Models";
    wstring _bundleOutputDir = L"../../../Client/Bin/Resources/Materials/";
    wstring _bundleOutputName = L"MaterialBundle";
    wstring _bundleOutputPath = L"../../../Client/Bin/Resources/Materials/MaterialBundle.material.json";

    wstring _selectedJsonPath{};
    wstring _selectedBundlePath{};

    MappingInputKind _selectedInputKind = MappingInputKind::Skeletal;

    // Bundle save feedback
    mutable bool _lastBundleSaveOk = false;
    mutable wstring _lastBundleSaveAbsPath{};
    mutable string _lastBundleSaveError{};

    // Mapping Source(SK SM FX) 에서 읽어온 원본 매핑 리스트다
    vector<MaterialMapping> _materialMappings{};
    // 매핑 리스트를 읽으며 MI assetName 기준으로 해석된 최종 데이터다
    unordered_map<string, MaterialInstanceData> _materialInstances{};
    // M이나 Unknown도 수동으로 슬롯을 지정할 수 있어서, 그 결과는 따로 들고있는다
    unordered_map<string, MaterialInstanceData> _manualInstances{};
    unordered_map<string, MaterialRefKind> _manualInstanceKinds{};
    // 덤프 포맷이나 키 우선순위 때문에 임의로 고른 부분을 기록해둔다
    vector<HeuristicNote> _heuristics{};

    // Cached texture index : 인덱싱 비용이 커서 필요 시점에만 재생성 시에는 먼저 발견된 1개만 저장한다.
    unordered_map<string, wstring> _textureIndex{};
    bool _textureIndexDirty = true;

    // 현재 UI에서 보고 있는 mapping 묶음이다. source + sourceFile 기준으로 좁혀서 보여준다
    int _activeMappingGroup = 0;

    // Selected ModelCom UI
    bool _showSelectedModelMeshMap = true;
    int  _selectedModelMaterialIndex = 0;

    // Mappings UI (Selected ModelCom이 없어도 1개 슬롯만 보이게)
    int  _mappingMaterialIndexFilter = 0;

    // Live apply
    bool _liveApplyEnabled = true;
    bool _liveApplyDirty = false;

public:
    static Shared<Material_View> Create();
    void Free() override;
};

NS_END
