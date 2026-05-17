#include "Material_View.h"

#pragma region INCLUDE
#include "MaterialCreateHelper.h"

#include "EditorInstance.h"
#include "Editor_Context.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "ContainerObject.h"
#include "PartObject.h"
#include "ModelCom.h"
#include "Asset_Manager.h"
#include "MaterialBuilder.h"
#include "Mesh.h"
#include "Material.h"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <Windows.h>
#include <ShObjIdl.h>
#include <ShlObj.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma endregion

// json 파싱과 텍스처 resolve는 MaterialCreateHelper에서 처리한다.
// 여기 파일은 UI와 에디터 적용 쪽만 들고있고 덤프 포맷 대응이나 텍스처 파일 찾기 같은 건 최대한 밖으로 뺐다.

#pragma region UTIL
namespace
{
    struct MaterialTemplateDesc
    {
        Engine::MaterialShaderTemplate id = Engine::MaterialShaderTemplate::OpaquePBR;
        const char* name = "";

        bool supportsBaseColor = true;
        bool supportsNormal = true;
        bool supportsORM = true;
        bool supportsEmissive = true;
        bool supportsOpacityMask = false;
    };

    constexpr MaterialTemplateDesc kTemplateTable[] =
    {
        { Engine::MaterialShaderTemplate::OpaquePBR, "OpaquePBR", true, true, true, true, false },
        { Engine::MaterialShaderTemplate::MaskedPBR, "MaskedPBR", true, true, true, true, true },
        { Engine::MaterialShaderTemplate::EmissivePBR, "EmissivePBR", true, true, true, true, false },
        { Engine::MaterialShaderTemplate::TransparentPBR, "TransparentPBR", true, true, true, true, false },
    };

    using TexturePreviewCache = std::unordered_map<wstring, ComPtr<ID3D11ShaderResourceView>>;
    using TextureSearchCache = std::unordered_map<string, string>;
    using TextureCandidateCache = std::unordered_map<wstring, vector<fs::path>>;

    TexturePreviewCache g_MaterialViewTexturePreviewCache{};
    TextureSearchCache g_MaterialViewTextureSearchCache{};
    TextureCandidateCache g_MaterialViewTextureCandidateCache{};

    string To_DisplayTexturePath(const wstring& resolvedPath)
    {
        // 여기서는 전체 절대 경로가 너무 길어서, Models 아래만 보이게 잘라서 보여준다
        string fullPath = String::ToString(resolvedPath);

        const size_t modelsPos = fullPath.find("Models\\");
        if (modelsPos != string::npos)
            return fullPath.substr(modelsPos);

        const size_t modelsPosAlt = fullPath.find("Models/");
        if (modelsPosAlt != string::npos)
            return fullPath.substr(modelsPosAlt);

        return fullPath;
    }

    bool Is_TextureExtension(const fs::path& path)
    {
        const string ext = String::ToString(path.extension().wstring());
        return ext == ".png" || ext == ".PNG" ||
            ext == ".jpg" || ext == ".JPG" ||
            ext == ".jpeg" || ext == ".JPEG" ||
            ext == ".tga" || ext == ".TGA" ||
            ext == ".dds" || ext == ".DDS" ||
            ext == ".bmp" || ext == ".BMP";
    }

    vector<fs::path> Collect_TextureCandidates(const wstring& resolvedPath)
    {
        vector<fs::path> candidates{};

        if (resolvedPath.empty())
            return candidates;

        const fs::path currentPath(resolvedPath);
        const fs::path directory = currentPath.parent_path();
        if (directory.empty() || !fs::exists(directory))
            return candidates;

        for (const auto& entry : fs::directory_iterator(directory))
        {
            if (!entry.is_regular_file())
                continue;

            if (!Is_TextureExtension(entry.path()))
                continue;

            candidates.push_back(entry.path().lexically_normal());
        }

        sort(candidates.begin(), candidates.end(), [](const fs::path& lhs, const fs::path& rhs)
        {
            return lhs.filename().wstring() < rhs.filename().wstring();
        });

        return candidates;
    }

    ID3D11ShaderResourceView* Get_TexturePreviewSRV(const fs::path& texturePath)
    {
        const wstring normalizedPath = texturePath.lexically_normal().wstring();

        const auto cached = g_MaterialViewTexturePreviewCache.find(normalizedPath);
        if (cached != g_MaterialViewTexturePreviewCache.end())
            return cached->second.Get();

        if (!fs::exists(texturePath) || !fs::is_regular_file(texturePath))
            return nullptr;

        ID3D11ShaderResourceView* srv = nullptr;
        const fs::path ext = texturePath.extension();

        HRESULT hr = S_OK;
        if (ext == ".dds" || ext == ".DDS")
            hr = DirectX::CreateDDSTextureFromFile(GAME->Get_Device().Get(), texturePath.c_str(), nullptr, &srv);
        else
            hr = DirectX::CreateWICTextureFromFile(GAME->Get_Device().Get(), texturePath.c_str(), nullptr, &srv);

        if (FAILED(hr) || srv == nullptr)
            return nullptr;

        g_MaterialViewTexturePreviewCache.emplace(normalizedPath, srv);
        return srv;
    }
}

#pragma endregion

NS_BEGIN(MainEditor)

Material_View::Material_View()
    : Editor_Window{ L"Material" }
{
}

HRESULT Material_View::Initialize()
{
    _textureIndexDirty = true;
    return Editor_Window::Initialize();
}

void Material_View::Update(float timeDelta)
{
    Editor_Window::Update(timeDelta);
}

void Material_View::Pre_Render()
{
    Editor_Window::Pre_Render();
}

void Material_View::Draw_ModeSelector()
{
    ImGui::SeparatorText("[ Material View Mode ]");

    const char* preview =
        (_mode == CreateMode::CreateWithJson) ? "Create With Json" :
        (_mode == CreateMode::EditWithBundle) ? "Edit With material.json" :
        (_mode == CreateMode::EditWithModelCom) ? "Edit With ModelCom" :
        (_mode == CreateMode::CreateMaterial) ? "Create Material" : "Unknown";

    if (ImGui::BeginCombo("##MaterialViewMode", preview))
    {
        struct ModeEntry { CreateMode mode; const char* label; };
        const ModeEntry entries[] = {
            { CreateMode::CreateWithJson, "Create With Json" },
            { CreateMode::EditWithBundle, "Edit With material.json" },
            { CreateMode::EditWithModelCom, "Edit With ModelCom" },
            { CreateMode::CreateMaterial, "Create Material" },
        };

        for (const ModeEntry& e : entries)
        {
            const bool selected = (_mode == e.mode);
            if (ImGui::Selectable(e.label, selected))
                _mode = e.mode;
            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }
}

void Material_View::Draw_Settings()
{
    // 텍스처 루트는 자주 바꾸는 값이라 여기에서 바로 편집하게 둔다
    ImGui::SeparatorText("Settings");

    static char texRootBuf[512]{};
    static char bundleDirBuf[512]{};
    static char bundleNameBuf[256]{};

    auto syncToBufOnce = [](char* buf, size_t bufSize, const wstring& value)
    {
        if (buf[0] != '\0')
            return;
        const string s = String::ToString(value);
        strncpy_s(buf, bufSize, s.c_str(), _TRUNCATE);
    };

    syncToBufOnce(texRootBuf, sizeof(texRootBuf), _textureSearchRoot);
    syncToBufOnce(bundleDirBuf, sizeof(bundleDirBuf), _bundleOutputDir);
    syncToBufOnce(bundleNameBuf, sizeof(bundleNameBuf), _bundleOutputName);

    ImGui::Checkbox("Live Apply", &_liveApplyEnabled);

    ImGui::TextDisabled("Texture Root");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(-100.f);
    if (ImGui::InputText("##TextureRoot", texRootBuf, sizeof(texRootBuf)))
    {
        _textureSearchRoot = String::ToWString(string(texRootBuf));
        _textureIndexDirty = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Browse##TextureRoot"))
    {
        wstring picked{};
        if (Try_PickFolderDialog(L"Select Texture Root", picked))
        {
            _textureSearchRoot = picked;
            _textureIndexDirty = true;

            const string s = String::ToString(_textureSearchRoot);
            strncpy_s(texRootBuf, sizeof(texRootBuf), s.c_str(), _TRUNCATE);
        }
    }

    ImGui::SeparatorText("Bundle Output");

    ImGui::TextDisabled("Save Folder");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(-100.f);
    if (ImGui::InputText("##BundleDir", bundleDirBuf, sizeof(bundleDirBuf)))
        _bundleOutputDir = String::ToWString(string(bundleDirBuf));

    ImGui::SameLine();
    if (ImGui::Button("Browse##BundleDir"))
    {
        wstring picked{};
        if (Try_PickFolderDialog(L"Select Bundle Output Folder", picked))
        {
            _bundleOutputDir = picked;
            const string s = String::ToString(_bundleOutputDir);
            strncpy_s(bundleDirBuf, sizeof(bundleDirBuf), s.c_str(), _TRUNCATE);
        }
    }

    ImGui::TextDisabled("Save Name");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputText("##BundleName", bundleNameBuf, sizeof(bundleNameBuf)))
        _bundleOutputName = String::ToWString(string(bundleNameBuf));

    auto buildBundleOutputPath = [&]() -> fs::path
    {
        fs::path dir(_bundleOutputDir);
        if (dir.empty())
            return {};

        const wstring nameW = _bundleOutputName;
        if (nameW.empty())
            return {};

        fs::path fileName(nameW);

        // 이름만 입력하면 자동으로 .material.json을 붙인다.
        // 확장자까지 입력했다면 그대로 사용한다.
        if (fileName.extension().empty())
        {
            const string base = String::ToString(fileName.wstring());
            if (base.ends_with(".material"))
                fileName.replace_extension(".json");
            else
                fileName = fs::path(base + ".material.json");
        }

        fs::path out = dir / fileName;
        return out.lexically_normal();
    };

    // 저장 경로가 상대경로면, 실제로는 프로세스 CWD 기준으로 해석된다
    // 파일이 어디로 저장됐는지 헷갈릴 때가 많아서, 여기서 절대 경로를 같이 보여준다
    {
        fs::path outPath = buildBundleOutputPath();
        _bundleOutputPath = outPath.wstring();

        if (!outPath.empty())
        {
            if (outPath.is_relative())
                outPath = fs::absolute(outPath);
            outPath = outPath.lexically_normal();
            ImGui::TextDisabled("Resolved");
            ImGui::SameLine(120.f);
            ImGui::TextWrapped("%s", String::ToString(outPath.wstring()).c_str());
        }

        if (!_lastBundleSaveError.empty() || _lastBundleSaveOk)
        {
            if (_lastBundleSaveOk)
                ImGui::TextColored(ImVec4(0.25f, 0.85f, 0.35f, 1.f), "Saved: %s", String::ToString(_lastBundleSaveAbsPath).c_str());
            else
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.f), "Save failed: %s", _lastBundleSaveError.c_str());
        }
    }
}

void Material_View::Draw_CreateWithJson()
{
    // Create With Json은 흐름
    // 1. json 파일 하나를 고른다
    // 2. 타입을 선택하고 mapping을 만든다
    // 3. mapping 기반으로 MI를 읽어서 최종 머테리얼 데이터를 만든다

    // 1. mapping source json 파일 고르기
    ImGui::SeparatorText("Input");

    if (ImGui::Button("Choose Json File"))
    {
        wstring picked{};
        fs::path defaultFolder(_originRoot);

        if (Try_PickJsonFileDialog(L"Select Json File", defaultFolder.wstring(), picked))
            _selectedJsonPath = picked;
    }

    ImGui::SameLine();
    {
        const fs::path p(_selectedJsonPath);
        const string label = _selectedJsonPath.empty() ? "<None>" : String::ToString(p.filename().wstring());
        ImGui::TextDisabled("%s", label.c_str());
    }

    ImGui::SeparatorText("Type");
    int kind = static_cast<int>(_selectedInputKind);
    ImGui::RadioButton("Skeletal", &kind, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Static Mesh", &kind, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Fx", &kind, 2);
    _selectedInputKind = static_cast<MappingInputKind>(kind);

    if (ImGui::Button("Start Mapping"))
    {
        // 여기서는 아직 MI를 읽지 않는다. 우선 어떤 파일이 어떤 머테리얼을 참조하는지 목록만 만든다
        if (!_selectedJsonPath.empty())
        {
            _mappingMaterialIndexFilter = 0;
            _liveApplyDirty = false;
            switch (_selectedInputKind)
            {
            case MappingInputKind::Skeletal:
                Load_SkFile(_selectedJsonPath);
                break;
            case MappingInputKind::StaticMesh:
                Load_SmFile(_selectedJsonPath);
                break;
            case MappingInputKind::Fx:
                Load_FxFile(_selectedJsonPath);
                break;
            default:
                break;
            }
        }
    }

    // 2. mapping source 기준으로 MaterialInstanceData 읽어 텍스처, 값 모으기
    if (ImGui::Button("Build Materials"))
        Build_MaterialInstances();

    ImGui::SameLine();
    if (ImGui::Button("Save Bundle"))
    {
        // Build를 자동으로 호출하면 방금 편집한 값이 덮여서 사라질 수 있다
        // 저장은 현재 화면에 보이는 상태 그대로를 파일로 남긴다
        (void)Save_BundleFile(_bundleOutputPath);
    }

    ImGui::SameLine();
    if (ImGui::Button("Apply To Selected ModelCom"))
    {
        // 적용도 현재 화면 상태 그대로 사용한다
        Apply_ToSelectedModel();
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear Bundle"))
    {
        _materialMappings.clear();
        _materialInstances.clear();
        _manualInstances.clear();
        _manualInstanceKinds.clear();
        _heuristics.clear();
        _activeMappingGroup = 0;
        _mappingMaterialIndexFilter = 0;
        _liveApplyDirty = false;
    }

    // 선택된 모델 기준으로 materialIndex를 골라서, 아래 편집 UI를 그 슬롯만 보이게 필터링한다.
    Shared<Engine::ModelCom> selectedModel = nullptr;
    int selectedMaterialIndexFilter = Draw_SelectedModelSection(selectedModel);
    if (selectedMaterialIndexFilter < 0)
        selectedMaterialIndexFilter = _mappingMaterialIndexFilter;
    Draw_MappingsEditorUI(selectedMaterialIndexFilter, selectedModel);
}

void Material_View::Draw_EditWithBundle()
{
    // 이미 저장된 .material.json(번들)을 불러와서 편집한다.
    ImGui::SeparatorText("Input");

    if (ImGui::Button("Choose material.json"))
    {
        wstring picked{};
        fs::path defaultFolder(_bundleOutputDir);
        if (Try_PickJsonFileDialog(L"Select material.json", defaultFolder.wstring(), picked))
            _selectedBundlePath = picked;
    }

    ImGui::SameLine();
    {
        const fs::path p(_selectedBundlePath);
        const string label = _selectedBundlePath.empty() ? "<None>" : String::ToString(p.filename().wstring());
        ImGui::TextDisabled("%s", label.c_str());
    }

    if (ImGui::Button("Load"))
    {
        if (!_selectedBundlePath.empty())
            (void)Load_BundleFile(_selectedBundlePath);
    }

    ImGui::SameLine();
    if (ImGui::Button("Save Bundle"))
        (void)Save_BundleFile(_bundleOutputPath);

    ImGui::SameLine();
    if (ImGui::Button("Apply To Selected ModelCom"))
        Apply_ToSelectedModel();

    ImGui::SameLine();
    if (ImGui::Button("Clear Bundle"))
    {
        _materialMappings.clear();
        _materialInstances.clear();
        _manualInstances.clear();
        _manualInstanceKinds.clear();
        _heuristics.clear();
        _activeMappingGroup = 0;
        _mappingMaterialIndexFilter = 0;
        _liveApplyDirty = false;
    }

    Shared<Engine::ModelCom> selectedModel = nullptr;
    int selectedMaterialIndexFilter = Draw_SelectedModelSection(selectedModel);
    if (selectedMaterialIndexFilter < 0)
        selectedMaterialIndexFilter = _mappingMaterialIndexFilter;
    Draw_MappingsEditorUI(selectedMaterialIndexFilter, selectedModel);
}

void Material_View::Draw_EditWithModelCom()
{
    // 선택된 ModelCom의 Runtime Material 상태(CB/텍스처)를 편집 상태로 가져온다.
    ImGui::SeparatorText("Input");

    if (ImGui::Button("Load From Selected ModelCom"))
        (void)Load_FromSelectedModelCom();

    ImGui::SameLine();
    if (ImGui::Button("Save Bundle"))
        (void)Save_BundleFile(_bundleOutputPath);

    ImGui::SameLine();
    if (ImGui::Button("Apply To Selected ModelCom"))
        Apply_ToSelectedModel();

    ImGui::SameLine();
    if (ImGui::Button("Clear Bundle"))
    {
        _materialMappings.clear();
        _materialInstances.clear();
        _manualInstances.clear();
        _manualInstanceKinds.clear();
        _heuristics.clear();
        _activeMappingGroup = 0;
        _mappingMaterialIndexFilter = 0;
        _liveApplyDirty = false;
    }

    Shared<Engine::ModelCom> selectedModel = nullptr;
    int selectedMaterialIndexFilter = Draw_SelectedModelSection(selectedModel);
    if (selectedMaterialIndexFilter < 0)
        selectedMaterialIndexFilter = _mappingMaterialIndexFilter;
    Draw_MappingsEditorUI(selectedMaterialIndexFilter, selectedModel);
}

int Material_View::Draw_SelectedModelSection(Shared<Engine::ModelCom>& outSelectedModel)
{
    ImGui::SeparatorText("Selected ModelCom");

    outSelectedModel = nullptr;
    const Editor_Context* context = EDITOR->Get_EditorContext();
    if (context != nullptr)
    {
        const Shared<GameObject> selectedObject = context->Get_Selection().selectedObject;
        if (selectedObject != nullptr)
            outSelectedModel = selectedObject->Get_Component<ModelCom>();
    }

    int selectedMaterialIndexFilter = -1;
    if (outSelectedModel == nullptr)
    {
        ImGui::TextDisabled("Select a GameObject that has ModelCom.");
        return -1;
    }

    if (ImGui::CollapsingHeader("Mesh -> Material Slot", &_showSelectedModelMeshMap, ImGuiTreeNodeFlags_DefaultOpen))
    {
        const auto& meshes = outSelectedModel->Get_Meshes();
        for (uint32 i = 0; i < static_cast<uint32>(meshes.size()); ++i)
        {
            const Shared<Mesh>& mesh = meshes[i];
            if (mesh == nullptr)
                continue;

            const uint32 matIndex = mesh->Get_MaterialIndex();
            const Shared<Material> mat = outSelectedModel->Get_Material(matIndex);
            const char* matName = (mat != nullptr) ? mat->Get_MaterialName().c_str() : "<null>";

            ImGui::BulletText("Mesh[%u] %s  ->  Material[%u] : %s",
                i,
                mesh->Get_Name().c_str(),
                matIndex,
                matName);
        }
    }

    const uint32 numMaterials = static_cast<uint32>(outSelectedModel->Get_NumMaterials());
    if (numMaterials == 0)
    {
        ImGui::TextDisabled("ModelCom has no materials.");
        return -1;
    }

    if (_selectedModelMaterialIndex < 0 || _selectedModelMaterialIndex >= static_cast<int>(numMaterials))
        _selectedModelMaterialIndex = 0;

    selectedMaterialIndexFilter = _selectedModelMaterialIndex;
    _mappingMaterialIndexFilter = _selectedModelMaterialIndex;

    const Shared<Material> currentMaterial = outSelectedModel->Get_Material(static_cast<uint32>(_selectedModelMaterialIndex));
    const string preview = "[" + to_string(_selectedModelMaterialIndex) + "] " +
        (currentMaterial ? currentMaterial->Get_MaterialName() : string("<null>"));

    if (ImGui::BeginCombo("Material Slot", preview.c_str()))
    {
        for (uint32 i = 0; i < numMaterials; ++i)
        {
            const Shared<Material> mat = outSelectedModel->Get_Material(i);
            const string label = "[" + to_string(i) + "] " + (mat ? mat->Get_MaterialName() : string("<null>"));
            const bool isSelected = (static_cast<int>(i) == _selectedModelMaterialIndex);
            if (ImGui::Selectable(label.c_str(), isSelected))
                _selectedModelMaterialIndex = static_cast<int>(i);
            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    return selectedMaterialIndexFilter;
}

void Material_View::Draw_MappingsEditorUI(int selectedMaterialIndexFilter, const Shared<Engine::ModelCom>& selectedModel)
{
    // SK_LUNE_UE5.json(Mapping Src) 파일이 참조하는 M, MI_*.json 파일을 읽어 실제 머테리얼로 파싱할 데이터들 보여주는 곳.
    ImGui::SeparatorText("Mappings");
    if (_materialMappings.empty())
    {
        ImGui::TextDisabled("No mappings. Start Mapping first.");
        return;
    }

    // mapping은 source + file 기준으로 묶이는 편이라, 여기서는 그 단위로 한 덩어리만 골라서 보여준다
    struct MappingGroup
    {
        MaterialSourceKind source = MaterialSourceKind::SK;
        string file{};
    };

    auto srcLabel = [](MaterialSourceKind s) -> const char*
    {
        switch (s)
        {
        case MaterialSourceKind::SK: return "SK";
        case MaterialSourceKind::SM: return "SM";
        case MaterialSourceKind::FX: return "FX";
        default: return "?";
        }
    };

    auto groupLabel = [&](const MappingGroup& g) -> string
    {
        return string(srcLabel(g.source)) + " : " + g.file;
    };

    vector<MappingGroup> groups{};
    groups.reserve(_materialMappings.size());
    for (const MaterialMapping& m : _materialMappings)
    {
        const auto it = find_if(groups.begin(), groups.end(), [&](const MappingGroup& g)
        {
            return g.source == m.source && g.file == m.sourceFile;
        });

        if (it == groups.end())
        {
            MappingGroup g{};
            g.source = m.source;
            g.file = m.sourceFile;
            groups.push_back(move(g));
        }
    }

    sort(groups.begin(), groups.end(), [](const MappingGroup& a, const MappingGroup& b)
    {
        if (a.source != b.source)
            return static_cast<uint32>(a.source) < static_cast<uint32>(b.source);
        return a.file < b.file;
    });

    if (groups.empty())
        return;

    if (_activeMappingGroup >= static_cast<int>(groups.size()))
        _activeMappingGroup = 0;

    ImGui::SeparatorText("Active");

    {
        const string preview = groupLabel(groups[_activeMappingGroup]);
        if (ImGui::BeginCombo("##ActiveMappingGroup", preview.c_str()))
        {
            for (int i = 0; i < static_cast<int>(groups.size()); ++i)
            {
                const bool isSelected = (i == _activeMappingGroup);
                const string label = groupLabel(groups[i]);
                if (ImGui::Selectable(label.c_str(), isSelected))
                    _activeMappingGroup = i;
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    const MappingGroup& activeGroup = groups[_activeMappingGroup];

    ImGui::TextDisabled("Src");
    ImGui::SameLine(120.f);
    ImGui::TextUnformatted(srcLabel(activeGroup.source));

    ImGui::TextDisabled("File");
    ImGui::SameLine(120.f);
    ImGui::TextUnformatted(activeGroup.file.c_str());

    vector<MaterialMapping*> activeMappings{};
    activeMappings.reserve(_materialMappings.size());
    for (MaterialMapping& m : _materialMappings)
    {
        if (m.source != activeGroup.source)
            continue;
        if (m.sourceFile != activeGroup.file)
            continue;
        activeMappings.push_back(&m);
    }

    sort(activeMappings.begin(), activeMappings.end(), [](const MaterialMapping* a, const MaterialMapping* b)
    {
        if (a->materialIndex != b->materialIndex)
            return a->materialIndex < b->materialIndex;
        if (a->rendererIndex != b->rendererIndex)
            return a->rendererIndex < b->rendererIndex;
        if (a->slotIndex != b->slotIndex)
            return a->slotIndex < b->slotIndex;
        return a->slotName < b->slotName;
    });

    int effectiveMaterialIndexFilter = selectedMaterialIndexFilter;
    if (effectiveMaterialIndexFilter < 0)
        effectiveMaterialIndexFilter = _mappingMaterialIndexFilter;
    else
        _mappingMaterialIndexFilter = effectiveMaterialIndexFilter;

    {
        vector<uint32> materialIndices{};
        materialIndices.reserve(activeMappings.size());
        for (const MaterialMapping* m : activeMappings)
        {
            if (m->source == MaterialSourceKind::FX)
                continue;
            materialIndices.push_back(m->materialIndex);
        }

        sort(materialIndices.begin(), materialIndices.end());
        materialIndices.erase(unique(materialIndices.begin(), materialIndices.end()), materialIndices.end());

        if (!materialIndices.empty())
        {
            const string preview = "MaterialIndex " + to_string(_mappingMaterialIndexFilter);
            if (ImGui::BeginCombo("Material Slot Filter", preview.c_str()))
            {
                for (uint32 idx : materialIndices)
                {
                    const bool isSelected = (static_cast<int>(idx) == _mappingMaterialIndexFilter);
                    const string label = to_string(idx);
                    if (ImGui::Selectable(label.c_str(), isSelected))
                    {
                        _mappingMaterialIndexFilter = static_cast<int>(idx);
                        effectiveMaterialIndexFilter = _mappingMaterialIndexFilter;
                        if (selectedModel != nullptr)
                            _selectedModelMaterialIndex = _mappingMaterialIndexFilter;
                    }
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
    }

    bool anyChanged = false;

    auto getOrMakeInstance = [&](MaterialMapping& mapping) -> MaterialInstanceData*
    {
        // MI는 Build 결과를 그대로 쓰고, M이나 Unknown은 수동 편집용 인스턴스를 만든다
        if (mapping.material.kind == MaterialRefKind::MI)
        {
            auto it = _materialInstances.find(mapping.material.assetName);
            if (it == _materialInstances.end())
                return nullptr;
            return &it->second;
        }

        if (mapping.material.assetName.empty())
            return nullptr;

        auto it = _manualInstances.find(mapping.material.assetName);
        if (it == _manualInstances.end())
        {
            MaterialInstanceData inst{};
            inst.assetName = mapping.material.assetName;
            inst.templateId = Engine::MaterialShaderTemplate::OpaquePBR;
            inst.renderState = Engine::MaterialRenderState{};
            inst.cb = Engine::MaterialCB{};

            auto [insertedIt, ok] = _manualInstances.emplace(mapping.material.assetName, move(inst));
            it = insertedIt;
        }

        _manualInstanceKinds[mapping.material.assetName] = mapping.material.kind;
        return &it->second;
    };

    auto drawTemplatePicker = [&](MaterialInstanceData& inst) -> bool
    {
        bool changed = false;
        int current = static_cast<int>(inst.templateId);
        const char* preview = "Unknown";
        for (const MaterialTemplateDesc& desc : kTemplateTable)
        {
            if (static_cast<int>(desc.id) == current)
            {
                preview = desc.name;
                break;
            }
        }

        if (ImGui::BeginCombo("Template", preview))
        {
            for (const MaterialTemplateDesc& desc : kTemplateTable)
            {
                const bool selected = (desc.id == inst.templateId);
                if (ImGui::Selectable(desc.name, selected))
                {
                    inst.templateId = desc.id;
                    changed = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
    };

    auto drawBlendModePicker = [&](Engine::MaterialRenderState& state) -> bool
    {
        bool changed = false;
        const char* preview = (state.blendMode == Engine::MaterialBlendMode::Opaque) ? "Opaque" :
            (state.blendMode == Engine::MaterialBlendMode::Masked) ? "Masked" :
            (state.blendMode == Engine::MaterialBlendMode::Translucent) ? "Translucent" : "Unknown";

        if (ImGui::BeginCombo("Blend", preview))
        {
            const char* labels[] = { "Opaque", "Masked", "Translucent" };
            for (int i = 0; i < 3; ++i)
            {
                const bool selected = (static_cast<int>(state.blendMode) == i);
                if (ImGui::Selectable(labels[i], selected))
                {
                    state.blendMode = static_cast<Engine::MaterialBlendMode>(i);
                    changed = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
    };

    auto drawSlotEditor = [&](const char* label, bool& overrideFlag, wstring& slotPath) -> bool
    {
        // 슬롯별 UI는 Model_Inspector에서 하던 방식과 거의 같다
        bool changed = false;
        const string idSuffix = string("##") + label;
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (!ImGui::TreeNode(label))
            return false;

        bool beforeEnabled = overrideFlag;
        ImGui::Checkbox("Enabled", &overrideFlag);
        changed = changed || (beforeEnabled != overrideFlag);

        ImGui::TextDisabled("Path");
        ImGui::SameLine(120.f);
        ImGui::BeginDisabled();
        const string displayPath = To_DisplayTexturePath(slotPath);
        ImGui::TextWrapped("%s", displayPath.empty() ? "-" : displayPath.c_str());
        ImGui::EndDisabled();

        if (!slotPath.empty())
        {
            ID3D11ShaderResourceView* previewSrv = Get_TexturePreviewSRV(fs::path(slotPath));
            if (previewSrv)
            {
                ImGui::TextDisabled("Preview");
                ImGui::SameLine(120.f);
                ImGui::Image(reinterpret_cast<ImTextureID>(previewSrv), ImVec2(64.f, 64.f));
            }
        }

        if (!slotPath.empty())
        {
            ImGui::TextDisabled("Texture");
            ImGui::SameLine(120.f);

            const string currentFileName = slotPath.empty() ? "-" : String::ToString(fs::path(slotPath).filename().wstring());
            const string texturePickerId = string("##TexturePicker") + idSuffix;
            if (ImGui::BeginCombo(texturePickerId.c_str(), currentFileName.c_str()))
            {
                const fs::path directory = fs::path(slotPath).parent_path();
                vector<fs::path> textureCandidates{};
                if (!directory.empty() && fs::exists(directory))
                {
                    const wstring dirKey = directory.lexically_normal().wstring();
                    const auto cached = g_MaterialViewTextureCandidateCache.find(dirKey);
                    if (cached != g_MaterialViewTextureCandidateCache.end())
                        textureCandidates = cached->second;
                    else
                    {
                        textureCandidates = Collect_TextureCandidates(slotPath);
                        g_MaterialViewTextureCandidateCache[dirKey] = textureCandidates;
                    }
                }

                const string pickerKey = string(label) + "_" + currentFileName;
                string& searchText = g_MaterialViewTextureSearchCache[pickerKey];
                char searchBuffer[256]{};
                strncpy_s(searchBuffer, searchText.c_str(), _TRUNCATE);

                ImGui::SetNextItemWidth(-FLT_MIN);
                const string textureSearchId = string("##TextureSearch") + idSuffix;
                if (ImGui::InputTextWithHint(textureSearchId.c_str(), "Search texture...", searchBuffer, sizeof(searchBuffer)))
                    searchText = searchBuffer;

                ImGui::Separator();

                for (const fs::path& candidatePath : textureCandidates)
                {
                    const string candidateName = String::ToString(candidatePath.filename().wstring());
                    const bool isSelected = (!slotPath.empty() && candidatePath.wstring() == fs::path(slotPath).lexically_normal().wstring());

                    if (!searchText.empty())
                    {
                        string candidateNameLower = candidateName;
                        string searchTextLower = searchText;

                        transform(candidateNameLower.begin(), candidateNameLower.end(), candidateNameLower.begin(), ::tolower);
                        transform(searchTextLower.begin(), searchTextLower.end(), searchTextLower.begin(), ::tolower);

                        if (candidateNameLower.find(searchTextLower) == string::npos)
                            continue;
                    }

                    ImGui::PushID(candidateName.c_str());

                    ID3D11ShaderResourceView* thumbSrv = Get_TexturePreviewSRV(candidatePath);
                    if (thumbSrv)
                        ImGui::Image(reinterpret_cast<ImTextureID>(thumbSrv), ImVec2(32.f, 32.f));
                    else
                        ImGui::Dummy(ImVec2(32.f, 32.f));

                    ImGui::SameLine();

                    if (ImGui::Selectable(candidateName.c_str(), isSelected, 0, ImVec2(0.f, 32.f)))
                    {
                        slotPath = candidatePath.wstring();
                        overrideFlag = true;
                        changed = true;
                    }

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();

                    ImGui::PopID();
                }

                ImGui::EndCombo();
            }
        }

        ImGui::TextDisabled("Browse");
        ImGui::SameLine(120.f);
        const string browseId = string("Browse") + idSuffix;
        if (ImGui::Button(browseId.c_str()))
        {
            wstring picked{};
            if (Try_PickTextureFileDialog(L"Select Texture", _textureSearchRoot, picked))
            {
                slotPath = picked;
                overrideFlag = true;
                changed = true;
            }
        }

        ImGui::SameLine();
        const string clearId = string("Clear") + idSuffix;
        if (ImGui::Button(clearId.c_str()))
        {
            slotPath.clear();
            overrideFlag = true;
            changed = true;
        }

        ImGui::TreePop();
        return changed;
    };

    for (MaterialMapping* mPtr : activeMappings)
    {
        MaterialMapping& m = *mPtr;

        // 선택된 ModelCom 기반으로 slot을 고른 경우, 해당 materialIndex만 보여준다.
        if (effectiveMaterialIndexFilter >= 0 &&
            m.source != MaterialSourceKind::FX &&
            static_cast<int>(m.materialIndex) != effectiveMaterialIndexFilter)
        {
            continue;
        }

        ImGui::PushID(mPtr);

        string header{};
        if (m.source == MaterialSourceKind::FX)
            header = "Renderer : " + m.emitter + " " + m.rendererType + "[" + to_string(m.rendererIndex) + "] slot" + to_string(m.slotIndex);
        else
            header = "Material[" + to_string(m.materialIndex) + "] : " + m.slotName;

        if (!ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PopID();
            continue;
        }

        if (m.source != MaterialSourceKind::FX)
        {
            ImGui::TextDisabled("Index");
            ImGui::SameLine(120.f);
            int idx = static_cast<int>(m.materialIndex);
            ImGui::SetNextItemWidth(120.f);
            if (ImGui::DragInt("##MaterialIndex", &idx, 0.25f, 0, 4096))
            {
                m.materialIndex = static_cast<uint32>((std::max)(0, idx));
                anyChanged = true;
            }

            ImGui::SameLine();
            if (ImGui::Button("Reset##MaterialIndex"))
            {
                m.materialIndex = m.originalMaterialIndex;
                anyChanged = true;
            }

            ImGui::TextDisabled("Slot");
            ImGui::SameLine(120.f);
            if (_mode == CreateMode::CreateMaterial)
            {
                char slotBuf[256]{};
                strncpy_s(slotBuf, m.slotName.c_str(), _TRUNCATE);

                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::InputText("##SlotName", slotBuf, sizeof(slotBuf)))
                {
                    const string newName = slotBuf;
                    if (m.slotName != newName)
                    {
                        m.slotName = newName;
                        anyChanged = true;
                    }
                }
            }
            else
            {
                ImGui::TextUnformatted(m.slotName.c_str());
            }
        }

        ImGui::SeparatorText("Ref");
        {
            const char* kindPreview = (m.material.kind == MaterialRefKind::MI) ? "MI" :
                (m.material.kind == MaterialRefKind::M) ? "M" : "Unknown";

            int kind = static_cast<int>(m.material.kind);
            if (ImGui::BeginCombo("Kind", kindPreview))
            {
                if (ImGui::Selectable("MI", m.material.kind == MaterialRefKind::MI))
                    kind = static_cast<int>(MaterialRefKind::MI);
                if (ImGui::Selectable("M", m.material.kind == MaterialRefKind::M))
                    kind = static_cast<int>(MaterialRefKind::M);
                if (ImGui::Selectable("Unknown", m.material.kind == MaterialRefKind::Unknown))
                    kind = static_cast<int>(MaterialRefKind::Unknown);
                ImGui::EndCombo();
            }

            bool refChanged = false;
            const MaterialRefKind newKind = static_cast<MaterialRefKind>(kind);
            if (newKind != m.material.kind)
            {
                m.material.kind = newKind;
                refChanged = true;
                anyChanged = true;
            }

            char assetBuf[256]{};
            strncpy_s(assetBuf, m.material.assetName.c_str(), _TRUNCATE);

            ImGui::TextDisabled("Asset");
            ImGui::SameLine(120.f);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputText("##AssetName", assetBuf, sizeof(assetBuf)))
            {
                const string oldName = m.material.assetName;
                const string newName = assetBuf;
                if (oldName != newName)
                {
                    auto itManual = _manualInstances.find(oldName);
                    if (itManual != _manualInstances.end())
                    {
                        MaterialInstanceData moved = move(itManual->second);
                        _manualInstances.erase(itManual);
                        moved.assetName = newName;
                        _manualInstances[newName] = move(moved);

                        auto itKind = _manualInstanceKinds.find(oldName);
                        if (itKind != _manualInstanceKinds.end())
                        {
                            _manualInstanceKinds[newName] = itKind->second;
                            _manualInstanceKinds.erase(itKind);
                        }
                    }

                    m.material.assetName = newName;
                    refChanged = true;
                    anyChanged = true;
                }
            }

            if (refChanged)
            {
                // 사용자가 직접 바꾸면 이 값들은 더 이상 신뢰할 수 없다
                m.material.objectName.clear();
                m.material.objectPath.clear();
            }
        }

        MaterialInstanceData* inst = getOrMakeInstance(m);
        if (inst == nullptr)
        {
            ImGui::TextDisabled("No instance data. Build Materials first or set ref kind and asset name.");
            ImGui::PopID();
            continue;
        }

        ImGui::SeparatorText("Textures");
        anyChanged = anyChanged || drawSlotEditor("BaseColor", inst->baseColorOverride, inst->baseColorPath);
        anyChanged = anyChanged || drawSlotEditor("Normal", inst->normalOverride, inst->normalPath);
        anyChanged = anyChanged || drawSlotEditor("ORM", inst->ormOverride, inst->ormPath);
        anyChanged = anyChanged || drawSlotEditor("Emissive", inst->emissiveOverride, inst->emissivePath);
        anyChanged = anyChanged || drawSlotEditor("OpacityMask", inst->opacityMaskOverride, inst->opacityMaskPath);
        anyChanged = anyChanged || drawSlotEditor("HairDepth", inst->hairDepthOverride, inst->hairDepthPath);
        anyChanged = anyChanged || drawSlotEditor("HairID", inst->hairIDOverride, inst->hairIDPath);
        anyChanged = anyChanged || drawSlotEditor("IrisHeight", inst->irisHeightOverride, inst->irisHeightPath);
        anyChanged = anyChanged || drawSlotEditor("IrisMask", inst->irisMaskOverride, inst->irisMaskPath);

        ImGui::SeparatorText("Parameters");
        anyChanged = anyChanged || drawTemplatePicker(*inst);

        {
            int before = inst->forcedPassIndex;
            ImGui::SetNextItemWidth(120.f);
            if (ImGui::DragInt("Force Pass (-1=Auto)", &inst->forcedPassIndex, 0.25f, -1, 4096))
                anyChanged = true;
            anyChanged = anyChanged || (before != inst->forcedPassIndex);
        }

        if (_mode == CreateMode::EditWithModelCom)
        {
            bool before = inst->renderStateOverrideEnabled;
            ImGui::Checkbox("Override RenderState", &inst->renderStateOverrideEnabled);
            anyChanged = anyChanged || (before != inst->renderStateOverrideEnabled);
        }

        const bool disableRenderState = (_mode == CreateMode::EditWithModelCom && !inst->renderStateOverrideEnabled);
        ImGui::BeginDisabled(disableRenderState);
        {
            bool before = inst->renderState.twoSided;
            ImGui::Checkbox("TwoSided", &inst->renderState.twoSided);
            anyChanged = anyChanged || (before != inst->renderState.twoSided);
        }

        anyChanged = anyChanged || drawBlendModePicker(inst->renderState);

        if (ImGui::DragFloat("Clip", &inst->renderState.opacityMaskClipValue, 0.001f, 0.f, 1.f))
        {
            inst->cb.opacityMaskClipValue = inst->renderState.opacityMaskClipValue;
            inst->cbMask.opacityMaskClipValue = true;
            anyChanged = true;
        }
        ImGui::EndDisabled();

        Engine::MaterialCB baseCb{};
        bool hasBaseCb = false;
        if (_mode == CreateMode::EditWithModelCom && selectedModel != nullptr)
        {
            const Engine::Asset_Manager* am = GAME->Get_AssetManager();
            const Engine::MaterialBuilder* builder = am ? am->Get_MaterialBuilder() : nullptr;
            if (builder != nullptr)
            {
                const auto& bindings = selectedModel->Get_MaterialBindings();
                if (m.materialIndex < bindings.size())
                {
                    const string& bundleKey = bindings[m.materialIndex].baseBundleKey;
                    hasBaseCb = builder->Try_GetBaseCBForMaterial(bundleKey, m.materialIndex, baseCb);
                }
            }
        }

        if (_mode == CreateMode::EditWithModelCom)
        {
            if (ImGui::Button("Clear Instance Overrides (slot)"))
            {
                inst->cbMask.Clear();
                inst->renderStateOverrideEnabled = false;
                inst->baseColorOverride = false;
                inst->normalOverride = false;
                inst->ormOverride = false;
                inst->emissiveOverride = false;
                inst->opacityMaskOverride = false;
                inst->hairDepthOverride = false;
                inst->hairIDOverride = false;
                inst->irisHeightOverride = false;
                inst->irisMaskOverride = false;
                anyChanged = true;
            }
        }

        auto drawOverrideCheckbox = [&](const char* label, bool& enabled, const char* basePreview) -> bool
        {
            bool before = enabled;
            ImGui::Checkbox(label, &enabled);
            if (basePreview != nullptr)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", basePreview);
            }
            return (before != enabled);
        };

        {
            const char* basePreview = nullptr;
            string preview{};
            if (hasBaseCb)
            {
                preview = "Base: " +
                    to_string(baseCb.baseColorTint.x) + ", " +
                    to_string(baseCb.baseColorTint.y) + ", " +
                    to_string(baseCb.baseColorTint.z) + ", " +
                    to_string(baseCb.baseColorTint.w);
                basePreview = preview.c_str();
            }
            const bool toggled = drawOverrideCheckbox("Override BaseTint", inst->cbMask.baseColorTint, basePreview);
            anyChanged = anyChanged || toggled;
            if (toggled && inst->cbMask.baseColorTint && hasBaseCb)
                inst->cb.baseColorTint = baseCb.baseColorTint;

            ImGui::BeginDisabled(!inst->cbMask.baseColorTint);
            float baseTint[4] = { inst->cb.baseColorTint.x, inst->cb.baseColorTint.y, inst->cb.baseColorTint.z, inst->cb.baseColorTint.w };
            if (ImGui::ColorEdit4("BaseTint", baseTint))
            {
                inst->cb.baseColorTint = Engine::Vec4(baseTint[0], baseTint[1], baseTint[2], baseTint[3]);
                anyChanged = true;
            }
            ImGui::EndDisabled();
        }

        {
            const char* basePreview = nullptr;
            string preview{};
            if (hasBaseCb)
            {
                preview = "Base: " +
                    to_string(baseCb.emissiveColor.x) + ", " +
                    to_string(baseCb.emissiveColor.y) + ", " +
                    to_string(baseCb.emissiveColor.z) + ", " +
                    to_string(baseCb.emissiveColor.w);
                basePreview = preview.c_str();
            }
            const bool toggled = drawOverrideCheckbox("Override EmissiveColor", inst->cbMask.emissiveColor, basePreview);
            anyChanged = anyChanged || toggled;
            if (toggled && inst->cbMask.emissiveColor && hasBaseCb)
                inst->cb.emissiveColor = baseCb.emissiveColor;

            ImGui::BeginDisabled(!inst->cbMask.emissiveColor);
            float emiColor[4] = { inst->cb.emissiveColor.x, inst->cb.emissiveColor.y, inst->cb.emissiveColor.z, inst->cb.emissiveColor.w };
            if (ImGui::ColorEdit4("EmissiveColor", emiColor))
            {
                inst->cb.emissiveColor = Engine::Vec4(emiColor[0], emiColor[1], emiColor[2], emiColor[3]);
                anyChanged = true;
            }
            ImGui::EndDisabled();
        }

        {
            string preview{};
            const char* basePreview = nullptr;
            if (hasBaseCb) { preview = "Base: " + to_string(baseCb.emissiveIntensity); basePreview = preview.c_str(); }
            const bool toggled = drawOverrideCheckbox("Override EmissiveIntensity", inst->cbMask.emissiveIntensity, basePreview);
            anyChanged = anyChanged || toggled;
            if (toggled && inst->cbMask.emissiveIntensity && hasBaseCb) inst->cb.emissiveIntensity = baseCb.emissiveIntensity;
            ImGui::BeginDisabled(!inst->cbMask.emissiveIntensity);
            if (ImGui::DragFloat("EmissiveIntensity", &inst->cb.emissiveIntensity, 0.01f, 0.f, 100.f))
                anyChanged = true;
            ImGui::EndDisabled();
        }

        {
            string preview{};
            const char* basePreview = nullptr;
            if (hasBaseCb) { preview = "Base: " + to_string(baseCb.emissiveMaskMul); basePreview = preview.c_str(); }
            const bool toggled = drawOverrideCheckbox("Override EmissiveMaskMul", inst->cbMask.emissiveMaskMul, basePreview);
            anyChanged = anyChanged || toggled;
            if (toggled && inst->cbMask.emissiveMaskMul && hasBaseCb) inst->cb.emissiveMaskMul = baseCb.emissiveMaskMul;
            ImGui::BeginDisabled(!inst->cbMask.emissiveMaskMul);
            if (ImGui::DragFloat("EmissiveMaskMul", &inst->cb.emissiveMaskMul, 0.01f, 0.f, 100.f))
                anyChanged = true;
            ImGui::EndDisabled();
        }

        {
            string preview{};
            const char* basePreview = nullptr;
            if (hasBaseCb) { preview = "Base: " + to_string(baseCb.normalStrength); basePreview = preview.c_str(); }
            const bool toggled = drawOverrideCheckbox("Override NormalStrength", inst->cbMask.normalStrength, basePreview);
            anyChanged = anyChanged || toggled;
            if (toggled && inst->cbMask.normalStrength && hasBaseCb) inst->cb.normalStrength = baseCb.normalStrength;
            ImGui::BeginDisabled(!inst->cbMask.normalStrength);
            if (ImGui::DragFloat("NormalStrength", &inst->cb.normalStrength, 0.01f, 0.f, 10.f))
                anyChanged = true;
            ImGui::EndDisabled();
        }

        {
            string preview{};
            const char* basePreview = nullptr;
            if (hasBaseCb) { preview = "Base: " + to_string(baseCb.opacityMaskClipValue); basePreview = preview.c_str(); }
            const bool toggled = drawOverrideCheckbox("Override OpacityClipCB", inst->cbMask.opacityMaskClipValue, basePreview);
            anyChanged = anyChanged || toggled;
            if (toggled && inst->cbMask.opacityMaskClipValue && hasBaseCb) inst->cb.opacityMaskClipValue = baseCb.opacityMaskClipValue;
            ImGui::BeginDisabled(!inst->cbMask.opacityMaskClipValue);
            if (ImGui::DragFloat("OpacityClipCB", &inst->cb.opacityMaskClipValue, 0.001f, 0.f, 1.f))
            {
                inst->renderState.opacityMaskClipValue = inst->cb.opacityMaskClipValue;
                anyChanged = true;
            }
            ImGui::EndDisabled();
        }

        {
            string preview{};
            const char* basePreview = nullptr;
            if (hasBaseCb) { preview = "Base: " + to_string(baseCb.roughnessScale); basePreview = preview.c_str(); }
            const bool toggled = drawOverrideCheckbox("Override RoughnessScale", inst->cbMask.roughnessScale, basePreview);
            anyChanged = anyChanged || toggled;
            if (toggled && inst->cbMask.roughnessScale && hasBaseCb) inst->cb.roughnessScale = baseCb.roughnessScale;
            ImGui::BeginDisabled(!inst->cbMask.roughnessScale);
            if (ImGui::DragFloat("RoughnessScale", &inst->cb.roughnessScale, 0.01f, 0.f, 10.f))
                anyChanged = true;
            ImGui::EndDisabled();
        }

        {
            string preview{};
            const char* basePreview = nullptr;
            if (hasBaseCb) { preview = "Base: " + to_string(baseCb.metalnessScale); basePreview = preview.c_str(); }
            const bool toggled = drawOverrideCheckbox("Override MetalnessScale", inst->cbMask.metalnessScale, basePreview);
            anyChanged = anyChanged || toggled;
            if (toggled && inst->cbMask.metalnessScale && hasBaseCb) inst->cb.metalnessScale = baseCb.metalnessScale;
            ImGui::BeginDisabled(!inst->cbMask.metalnessScale);
            if (ImGui::DragFloat("MetalnessScale", &inst->cb.metalnessScale, 0.01f, 0.f, 10.f))
                anyChanged = true;
            ImGui::EndDisabled();
        }

        {
            string preview{};
            const char* basePreview = nullptr;
            if (hasBaseCb) { preview = "Base: " + to_string(baseCb.aoScale); basePreview = preview.c_str(); }
            const bool toggled = drawOverrideCheckbox("Override AoScale", inst->cbMask.aoScale, basePreview);
            anyChanged = anyChanged || toggled;
            if (toggled && inst->cbMask.aoScale && hasBaseCb) inst->cb.aoScale = baseCb.aoScale;
            ImGui::BeginDisabled(!inst->cbMask.aoScale);
            if (ImGui::DragFloat("AoScale", &inst->cb.aoScale, 0.01f, 0.f, 10.f))
                anyChanged = true;
            ImGui::EndDisabled();
        }

        {
            string preview{};
            const char* basePreview = nullptr;
            if (hasBaseCb)
            {
                preview = "Base: " + to_string(baseCb.uvScale.x) + ", " + to_string(baseCb.uvScale.y);
                basePreview = preview.c_str();
            }
            const bool toggled = drawOverrideCheckbox("Override UV Scale", inst->cbMask.uvScale, basePreview);
            anyChanged = anyChanged || toggled;
            if (toggled && inst->cbMask.uvScale && hasBaseCb) inst->cb.uvScale = baseCb.uvScale;
            ImGui::BeginDisabled(!inst->cbMask.uvScale);
            float uvScale[2] = { inst->cb.uvScale.x, inst->cb.uvScale.y };
            if (ImGui::DragFloat2("UV Scale", uvScale))
            {
                inst->cb.uvScale = Engine::Vec2(uvScale[0], uvScale[1]);
                anyChanged = true;
            }
            ImGui::EndDisabled();
        }

        {
            string preview{};
            const char* basePreview = nullptr;
            if (hasBaseCb)
            {
                preview = "Base: " + to_string(baseCb.uvOffset.x) + ", " + to_string(baseCb.uvOffset.y);
                basePreview = preview.c_str();
            }
            const bool toggled = drawOverrideCheckbox("Override UV Offset", inst->cbMask.uvOffset, basePreview);
            anyChanged = anyChanged || toggled;
            if (toggled && inst->cbMask.uvOffset && hasBaseCb) inst->cb.uvOffset = baseCb.uvOffset;
            ImGui::BeginDisabled(!inst->cbMask.uvOffset);
            float uvOffset[2] = { inst->cb.uvOffset.x, inst->cb.uvOffset.y };
            if (ImGui::DragFloat2("UV Offset", uvOffset))
            {
                inst->cb.uvOffset = Engine::Vec2(uvOffset[0], uvOffset[1]);
                anyChanged = true;
            }
            ImGui::EndDisabled();
        }

        {
            string preview{};
            const char* basePreview = nullptr;
            if (hasBaseCb) { preview = "Base: " + to_string(baseCb.uvRotation); basePreview = preview.c_str(); }
            const bool toggled = drawOverrideCheckbox("Override UV Rotation", inst->cbMask.uvRotation, basePreview);
            anyChanged = anyChanged || toggled;
            if (toggled && inst->cbMask.uvRotation && hasBaseCb) inst->cb.uvRotation = baseCb.uvRotation;
            ImGui::BeginDisabled(!inst->cbMask.uvRotation);
            if (ImGui::DragFloat("UV Rotation", &inst->cb.uvRotation, 0.001f, -100.f, 100.f))
                anyChanged = true;
            ImGui::EndDisabled();
        }

        {
            string preview{};
            const char* basePreview = nullptr;
            if (hasBaseCb) { preview = "Base: " + to_string(baseCb.materialMask); basePreview = preview.c_str(); }
            const bool toggled = drawOverrideCheckbox("Override MaterialMask", inst->cbMask.materialMask, basePreview);
            anyChanged = anyChanged || toggled;
            if (toggled && inst->cbMask.materialMask && hasBaseCb) inst->cb.materialMask = baseCb.materialMask;
            ImGui::BeginDisabled(!inst->cbMask.materialMask);
            int mask = static_cast<int>(inst->cb.materialMask);
            if (ImGui::InputInt("MaterialMask", &mask))
            {
                inst->cb.materialMask = static_cast<uint32>(std::max(0, mask));
                anyChanged = true;
            }
            ImGui::EndDisabled();
        }

        ImGui::PopID();
    }

    if (anyChanged)
        _liveApplyDirty = true;

    if (_liveApplyEnabled && _liveApplyDirty && selectedModel != nullptr && effectiveMaterialIndexFilter >= 0)
    {
        Apply_ToSelectedModel(effectiveMaterialIndexFilter);
        _liveApplyDirty = false;
    }

    ImGui::SeparatorText("Heuristics");
    if (_heuristics.empty())
    {
        ImGui::TextDisabled("No notes.");
        return;
    }

    for (const HeuristicNote& n : _heuristics)
        ImGui::BulletText("%s: %s", n.kind.c_str(), n.detail.c_str());
}

void Material_View::Draw_CreateMaterial()
{
    ImGui::SeparatorText("Manual Mappings");

    auto clearAll = [&]() -> void
    {
        _materialMappings.clear();
        _materialInstances.clear();
        _manualInstances.clear();
        _manualInstanceKinds.clear();
        _heuristics.clear();
        _activeMappingGroup = 0;
        _mappingMaterialIndexFilter = 0;
        _liveApplyDirty = false;
    };

    auto computeNextManualIndex = [&]() -> uint32
    {
        uint32 maxIndex = 0;
        bool hasAny = false;
        for (const MaterialMapping& m : _materialMappings)
        {
            if (m.source == MaterialSourceKind::FX)
                continue;
            if (m.sourceFile != "Manual")
                continue;

            maxIndex = hasAny ? (std::max)(maxIndex, m.materialIndex) : m.materialIndex;
            hasAny = true;
        }
        return hasAny ? (maxIndex + 1) : 0;
    };

    auto addManualSlot = [&](uint32 newIndex) -> void
    {
        MaterialMapping m{};
        m.source = MaterialSourceKind::SK;
        m.sourceFile = "Manual";
        m.materialIndex = newIndex;
        m.originalMaterialIndex = newIndex;
        m.slotName = "Slot_" + to_string(newIndex);

        m.material.kind = MaterialRefKind::M;
        m.material.assetName = "Manual_" + to_string(newIndex);

        _materialMappings.push_back(move(m));
        _mappingMaterialIndexFilter = static_cast<int>(newIndex);
    };

    if (ImGui::Button("New Manual Mapping"))
    {
        clearAll();
        addManualSlot(0);
    }

    ImGui::SameLine();
    if (ImGui::Button("Add Slot"))
    {
        if (!_materialMappings.empty())
        {
            bool hasNonManual = false;
            for (const MaterialMapping& m : _materialMappings)
            {
                if (m.source == MaterialSourceKind::FX)
                    continue;
                if (m.sourceFile != "Manual")
                {
                    hasNonManual = true;
                    break;
                }
            }

            if (hasNonManual)
                clearAll();
        }
        else
        {
            clearAll();
        }

        const uint32 newIndex = computeNextManualIndex();
        addManualSlot(newIndex);
    }

    ImGui::SameLine();
    if (ImGui::Button("Remove Current Slot"))
    {
        const int targetIndex = _mappingMaterialIndexFilter;
        string removedAssetName{};

        for (auto it = _materialMappings.begin(); it != _materialMappings.end(); ++it)
        {
            if (it->source == MaterialSourceKind::FX)
                continue;
            if (it->sourceFile != "Manual")
                continue;
            if (static_cast<int>(it->materialIndex) != targetIndex)
                continue;

            removedAssetName = it->material.assetName;
            _materialMappings.erase(it);
            break;
        }

        if (!removedAssetName.empty())
        {
            bool stillUsed = false;
            for (const MaterialMapping& m : _materialMappings)
            {
                if (m.source == MaterialSourceKind::FX)
                    continue;
                if (m.material.assetName == removedAssetName)
                {
                    stillUsed = true;
                    break;
                }
            }

            if (!stillUsed)
            {
                _manualInstances.erase(removedAssetName);
                _manualInstanceKinds.erase(removedAssetName);
            }
        }

        // 최소 1개는 유지
        bool hasAnyManual = false;
        for (const MaterialMapping& m : _materialMappings)
        {
            if (m.source == MaterialSourceKind::FX)
                continue;
            if (m.sourceFile == "Manual")
            {
                hasAnyManual = true;
                break;
            }
        }

        if (!hasAnyManual)
        {
            clearAll();
            addManualSlot(0);
        }
        else
        {
            // 삭제 후 필터를 남아있는 index로 이동
            int nearest = -1;
            int bestDelta = INT_MAX;
            for (const MaterialMapping& m : _materialMappings)
            {
                if (m.source == MaterialSourceKind::FX)
                    continue;
                if (m.sourceFile != "Manual")
                    continue;

                const int idx = static_cast<int>(m.materialIndex);
                const int delta = abs(idx - targetIndex);
                if (delta < bestDelta)
                {
                    bestDelta = delta;
                    nearest = idx;
                }
            }
            _mappingMaterialIndexFilter = (nearest >= 0) ? nearest : 0;
        }

        _liveApplyDirty = false;
    }

    ImGui::SeparatorText("Actions");
    if (ImGui::Button("Save Bundle"))
        (void)Save_BundleFile(_bundleOutputPath);

    ImGui::SameLine();
    if (ImGui::Button("Apply To Selected ModelCom"))
        Apply_ToSelectedModel(_mappingMaterialIndexFilter);

    ImGui::SameLine();
    if (ImGui::Button("Clear Bundle"))
        clearAll();

    // 선택된 모델 기준으로 materialIndex를 골라서, 아래 편집 UI를 그 슬롯만 보이게 필터링한다.
    Shared<Engine::ModelCom> selectedModel = nullptr;
    int selectedMaterialIndexFilter = Draw_SelectedModelSection(selectedModel);
    if (selectedMaterialIndexFilter < 0)
        selectedMaterialIndexFilter = _mappingMaterialIndexFilter;
    Draw_MappingsEditorUI(selectedMaterialIndexFilter, selectedModel);
}

void Material_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

        Draw_ModeSelector();
        Draw_Settings();

        if (_mode == CreateMode::CreateWithJson)
            Draw_CreateWithJson();
        else if (_mode == CreateMode::EditWithBundle)
            Draw_EditWithBundle();
        else if (_mode == CreateMode::EditWithModelCom)
            Draw_EditWithModelCom();
        else
            Draw_CreateMaterial();
    }

    ImGui::End();
    Set_Open(isOpen);
}

bool Material_View::Load_SkFile(const wstring& skFilePath)
{
    // helper는 상태를 들고있지 않아서 필요할 때마다 잠깐 만들어서 쓴다. 아니면 매번 동기화해야 해서 차라리 이게 편함.
    MaterialCreateHelper helper(_miRoot, _textureSearchRoot, &_textureIndex, &_textureIndexDirty);
    return helper.Load_SkFile(skFilePath, _materialMappings, _heuristics);
}

bool Material_View::Load_SmFile(const wstring& smFilePath)
{
    // SM은 StaticMaterials 배열을 본다
    MaterialCreateHelper helper(_miRoot, _textureSearchRoot, &_textureIndex, &_textureIndexDirty);
    return helper.Load_SmFile(smFilePath, _materialMappings, _heuristics);
}

bool Material_View::Load_FxFile(const wstring& fxFilePath)
{
    // FX는 emitter renderer마다 material ref가 따로 있어서 mapping이 더 많이 나옴. 
    MaterialCreateHelper helper(_miRoot, _textureSearchRoot, &_textureIndex, &_textureIndexDirty);
    return helper.Load_FxFile(fxFilePath, _materialMappings, _heuristics);
}

bool Material_View::Build_MaterialInstances()
{
    // mapping에서 MI만 골라서 실제 텍스처 경로와 파라미터를 해석한다
    // 결과는 _materialInstances에 쌓이고, Apply나 Save는 그 데이터를 사용한다
    MaterialCreateHelper helper(_miRoot, _textureSearchRoot, &_textureIndex, &_textureIndexDirty);
    return helper.Build_MaterialInstances(_materialMappings, _materialInstances, _heuristics);
}


void Material_View::Apply_ToSelectedModel(int onlyMaterialIndex)
{
    // 선택된 오브젝트의 ModelCom의 materialIndex별로 텍스처 슬롯을 Override 한다.
    const Editor_Context* context = EDITOR->Get_EditorContext();
    if (context == nullptr)
        return;

    const Shared<GameObject> selectedObject = context->Get_Selection().selectedObject;
    if (selectedObject == nullptr)
        return;

    if (_materialMappings.empty())
        return;

    auto applyToModel = [&](const Shared<ModelCom>& model) -> void
    {
        // ModelCom은 메시마다 materialIndex가 있고, 그 인덱스로 Material을 가져온다
        // mapping의 materialIndex와 ModelCom의 materialIndex가 완전히 같다고 확신할 수 없어서
        // 지금은 에디터에서 순서를 바꾸거나 인덱스를 맞춰갈 수 있도록 한다.
        if (model == nullptr)
            return;

        for (const MaterialMapping& m : _materialMappings)
        {
            if (m.source == MaterialSourceKind::FX)
                continue;

            if (onlyMaterialIndex >= 0 && static_cast<int>(m.materialIndex) != onlyMaterialIndex)
                continue;

            const MaterialInstanceData* instPtr = nullptr;
            if (m.material.kind == MaterialRefKind::MI)
            {
                const auto it = _materialInstances.find(m.material.assetName);
                if (it != _materialInstances.end())
                    instPtr = &it->second;
            }
            else
            {
                const auto it = _manualInstances.find(m.material.assetName);
                if (it != _manualInstances.end())
                    instPtr = &it->second;
            }

            if (instPtr == nullptr)
                continue;

            const MaterialInstanceData& inst = *instPtr;

            // =================================================== NOTE ============================================================
            // Edit With ModelCom 모드에서는 "오브젝트별(instance)"만 저장/적용한다.
            // 즉, runtime Material을 직접 덮어쓰는 게 아니라 ModelCom의 instance override를 갱신하고,
            // base(bundle) + instance를 합성 적용한다.
            // =====================================================================================================================
            if (_mode == CreateMode::EditWithModelCom)
            {
                Engine::MaterialInstanceOverride ov{};
                ov.overrideRenderStateEnabled = inst.renderStateOverrideEnabled;
                if (ov.overrideRenderStateEnabled)
                    ov.renderState = inst.renderState;
                ov.cb = inst.cb;
                ov.cbMask.baseColorTint = inst.cbMask.baseColorTint;
                ov.cbMask.emissiveColor = inst.cbMask.emissiveColor;
                ov.cbMask.emissiveIntensity = inst.cbMask.emissiveIntensity;
                ov.cbMask.emissiveMaskMul = inst.cbMask.emissiveMaskMul;
                ov.cbMask.normalStrength = inst.cbMask.normalStrength;
                ov.cbMask.opacityMaskClipValue = inst.cbMask.opacityMaskClipValue;
                ov.cbMask.aoScale = inst.cbMask.aoScale;
                ov.cbMask.roughnessScale = inst.cbMask.roughnessScale;
                ov.cbMask.metalnessScale = inst.cbMask.metalnessScale;
                ov.cbMask.uvScale = inst.cbMask.uvScale;
                ov.cbMask.uvOffset = inst.cbMask.uvOffset;
                ov.cbMask.uvRotation = inst.cbMask.uvRotation;
                ov.cbMask.materialMask = inst.cbMask.materialMask;

                if (inst.baseColorOverride) { ov.baseColor.enabled = true; ov.baseColor.path = inst.baseColorPath; }
                if (inst.normalOverride) { ov.normal.enabled = true; ov.normal.path = inst.normalPath; }
                if (inst.ormOverride) { ov.orm.enabled = true; ov.orm.path = inst.ormPath; }
                if (inst.emissiveOverride) { ov.emissive.enabled = true; ov.emissive.path = inst.emissivePath; }
                if (inst.opacityMaskOverride) { ov.opacityMask.enabled = true; ov.opacityMask.path = inst.opacityMaskPath; }
                if (inst.hairDepthOverride) { ov.hairDepth.enabled = true; ov.hairDepth.path = inst.hairDepthPath; }
                if (inst.hairIDOverride) { ov.hairID.enabled = true; ov.hairID.path = inst.hairIDPath; }
                if (inst.irisHeightOverride) { ov.irisHeight.enabled = true; ov.irisHeight.path = inst.irisHeightPath; }
                if (inst.irisMaskOverride) { ov.irisMask.enabled = true; ov.irisMask.path = inst.irisMaskPath; }

                model->Set_InstanceOverride(m.materialIndex, ov);
                model->Apply_EffectiveMaterial(m.materialIndex);
                continue;
            }

            const Shared<Material> material = model->Get_Material(m.materialIndex);
            if (material == nullptr)
                continue;

            material->Set_RenderState(inst.renderState);
            material->Set_CB(inst.cb);

            // slotIndex는 0으로 고정한다. 멀티 텍스처 인덱스는 아직 고려하지 않음
            // Enabled가 켜진 슬롯만 덮어쓴다. 비어있는 경로는 지우기로 해석한다
            if (inst.baseColorOverride)
                material->Override_TextureSlot(MaterialTextureSlot::BaseColor, 0, inst.baseColorPath);
            if (inst.normalOverride)
                material->Override_TextureSlot(MaterialTextureSlot::Normal, 0, inst.normalPath);
            if (inst.ormOverride)
                material->Override_TextureSlot(MaterialTextureSlot::ORM, 0, inst.ormPath);
            if (inst.emissiveOverride)
                material->Override_TextureSlot(MaterialTextureSlot::Emissive, 0, inst.emissivePath);

            // 빈 문자열 전달 필수!!!!!!!
            if (inst.opacityMaskOverride)
                material->Override_TextureSlot(MaterialTextureSlot::OpacityMask, 0, inst.opacityMaskPath);
            else
                material->Override_TextureSlot(MaterialTextureSlot::OpacityMask, 0, L"");
            if (inst.hairDepthOverride)
                material->Override_TextureSlot(MaterialTextureSlot::HairDepth, 0, inst.hairDepthPath);
            if (inst.hairIDOverride)
                material->Override_TextureSlot(MaterialTextureSlot::HairID, 0, inst.hairIDPath);
            if (inst.irisHeightOverride)
                material->Override_TextureSlot(MaterialTextureSlot::IrisHeight, 0, inst.irisHeightPath);
            if (inst.irisMaskOverride)
                material->Override_TextureSlot(MaterialTextureSlot::IrisMask, 0, inst.irisMaskPath);

#ifdef _DEBUG
            {
                const Engine::MaterialRenderState& rs = material->Get_RenderState();
                const Engine::MaterialCB& cb = material->Get_CB();

                string msg = "[Material_View][Apply] mat=" + material->Get_MaterialName();
                msg += " mappingIndex=" + to_string(m.materialIndex);
                msg += " blendMode=" + to_string(static_cast<int>(rs.blendMode));
                msg += " alphaClip=" + to_string(rs.alphaClip ? 1 : 0);
                msg += " clip=" + to_string(cb.opacityMaskClipValue);
                msg += " materialMask=" + to_string(cb.materialMask);
                LOG_INFO(msg);
            }
#endif
         }
     };


    // =================================================== NOTE ============================================================
    // 현재,
    // - PartObject는 PartObject를 가지지 않으므로 ModelCom에 재귀 적용하는 로직은 없다.
    // - ContainerObject는 ModelCom을 가지지 않으므로 자기 자신을 제외한 PartObject에만 적용한다.
    // =====================================================================================================================

    // PartObject는 하이어라키 직접 등록되지 않는다.
    if (const Shared<ModelCom> model = selectedObject->Get_Component<ModelCom>(); model != nullptr)
    {
        applyToModel(model);
        return;
    }

    Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(selectedObject);
    if (container == nullptr)
        return;

    const auto& parts = container->Get_PartObjects();
    for (const auto& [partTag, partObject] : parts)
    {
        if (partObject == nullptr)
            continue;

        const Shared<ModelCom> partModel = partObject->Get_Component<ModelCom>();
        if (partModel == nullptr)
            continue;

        HeuristicNote note{};
        note.kind = "apply_target";
        note.detail = String::ToString(partTag);
        _heuristics.push_back(move(note));

        applyToModel(partModel);
    }
}

bool Material_View::Try_PickFolderDialog(const wstring& title, wstring& outFolderPath)
{
    outFolderPath.clear();

    const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool shouldUninit = SUCCEEDED(initHr);

    IFileOpenDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    if (FAILED(hr) || dialog == nullptr)
    {
        if (shouldUninit)
            CoUninitialize();
        return false;
    }

    dialog->SetTitle(title.c_str());

    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);

    hr = dialog->Show(nullptr);
    if (SUCCEEDED(hr))
    {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr)
        {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr)
            {
                outFolderPath = path;
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }

    dialog->Release();

    if (shouldUninit)
        CoUninitialize();

    return !outFolderPath.empty();
}

bool Material_View::Try_PickJsonFileDialog(const wstring& title, const wstring& defaultFolder, wstring& outFilePath)
{
    // json 파일 하나를 고르는 다이얼로그다
    outFilePath.clear();

    const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool shouldUninit = SUCCEEDED(initHr);

    IFileOpenDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    if (FAILED(hr) || dialog == nullptr)
    {
        if (shouldUninit)
            CoUninitialize();
        return false;
    }

    dialog->SetTitle(title.c_str());

    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST);

    auto resolveDefaultFolderForJsonPicker = [](const wstring& userPath) -> fs::path
    {
        try
        {
            fs::path raw(userPath);
            if (raw.empty())
                return {};

            fs::path candidate = raw;
            if (candidate.is_relative())
                candidate = fs::absolute(candidate);
            candidate = candidate.lexically_normal();

            if (fs::exists(candidate) && fs::is_directory(candidate))
                return candidate;

            fs::path cursor = fs::current_path();
            for (int i = 0; i < 10; ++i)
            {
                const fs::path probe = (cursor / "Client" / "Bin" / "Resources" / "Materials" / "Origins" / "MappingSrc").lexically_normal();
                if (fs::exists(probe) && fs::is_directory(probe))
                    return probe;

                if (!cursor.has_parent_path())
                    break;

                fs::path parent = cursor.parent_path();
                if (parent == cursor)
                    break;
                cursor = parent;
            }

            return {};
        }
        catch (...)
        {
            return {};
        }
    };

    const fs::path resolvedDefaultFolder = resolveDefaultFolderForJsonPicker(defaultFolder);
    if (!resolvedDefaultFolder.empty())
    {
        IShellItem* folderItem = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(resolvedDefaultFolder.wstring().c_str(), nullptr, IID_PPV_ARGS(&folderItem))) && folderItem != nullptr)
        {
            dialog->SetFolder(folderItem);
            folderItem->Release();
        }
    }

    const COMDLG_FILTERSPEC filters[] =
    {
        { L"Json", L"*.json" },
        { L"All", L"*.*" },
    };
    dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);

    hr = dialog->Show(nullptr);
    if (SUCCEEDED(hr))
    {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr)
        {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr)
            {
                outFilePath = path;
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }

    dialog->Release();

    if (shouldUninit)
        CoUninitialize();

    return !outFilePath.empty();
}

bool Material_View::Load_BundleFile(const wstring& bundlePath)
{
    // 저장된 .material.json(번들)을 읽어서
    // - _materialMappings / _materialInstances / _manualInstances
    // 를 편집 가능한 상태로 복원한다.
    fs::path p(bundlePath);
    if (p.empty())
        return false;

    if (p.is_relative())
        p = fs::absolute(p);
    p = p.lexically_normal();

    if (!fs::exists(p) || !fs::is_regular_file(p))
        return false;

    json root{};
    try
    {
        ifstream ifs(p);
        if (!ifs.is_open())
            return false;
        ifs >> root;
    }
    catch (...)
    {
        return false;
    }

    if (!root.is_object())
        return false;

    _materialMappings.clear();
    _materialInstances.clear();
    _manualInstances.clear();
    _manualInstanceKinds.clear();
    _heuristics.clear();
    _activeMappingGroup = 0;

    if (root.contains("heuristics") && root["heuristics"].is_array())
    {
        for (const json& jn : root["heuristics"])
        {
            if (!jn.is_object())
                continue;
            HeuristicNote note{};
            note.kind = jn.value("kind", string{});
            note.detail = jn.value("detail", string{});
            _heuristics.push_back(move(note));
        }
    }

    auto parseRefKind = [](const string& s) -> MaterialRefKind
    {
        if (s == "MI") return MaterialRefKind::MI;
        if (s == "M") return MaterialRefKind::M;
        return MaterialRefKind::Unknown;
    };

    const fs::path resourceRoot = p.has_parent_path() ? p.parent_path().parent_path() : fs::path{};

    auto relToAbsUnderResourceRoot = [&](const string& rel) -> wstring
    {
        if (rel.empty())
            return {};

        fs::path rp(rel);
        if (rp.is_absolute())
            return rp.lexically_normal().wstring();

        if (!resourceRoot.empty())
            return (resourceRoot / rp).lexically_normal().wstring();

        return rp.lexically_normal().wstring();
    };

    auto readTexturePathFromJson = [&](const json& textureValue) -> wstring
    {
        if (textureValue.is_string())
            return relToAbsUnderResourceRoot(textureValue.get<string>());

        if (!textureValue.is_object())
            return {};

        const string textureGuid = textureValue.value("texture_guid", string{});
        if (!textureGuid.empty())
        {
            const wstring resolvedByGuid = GAME->Resolve_AssetPath(textureGuid);
            if (!resolvedByGuid.empty())
                return resolvedByGuid;
        }

        const string fallbackPath = textureValue.value("texture_path", string{});
        return relToAbsUnderResourceRoot(fallbackPath);
    };

    if (root.contains("instances") && root["instances"].is_array())
    {
        for (const json& ji : root["instances"])
        {
            if (!ji.is_object())
                continue;

            MaterialInstanceData inst{};
            inst.assetName = ji.value("assetName", string{});
            if (inst.assetName.empty())
                continue;

            inst.templateId = static_cast<Engine::MaterialShaderTemplate>(ji.value("template", static_cast<uint32>(inst.templateId)));

            if (ji.contains("renderState") && ji["renderState"].is_object())
            {
                const json& rs = ji["renderState"];
                inst.renderState.twoSided = rs.value("twoSided", inst.renderState.twoSided);
                inst.renderState.blendMode = static_cast<Engine::MaterialBlendMode>(rs.value("blendMode", static_cast<uint32>(inst.renderState.blendMode)));
                inst.renderState.opacityMaskClipValue = rs.value("opacityMaskClipValue", inst.renderState.opacityMaskClipValue);
            }

            inst.forcedPassIndex = ji.value("forcePassIndex", inst.forcedPassIndex);

            if (ji.contains("overrides") && ji["overrides"].is_object())
            {
                const json& jov = ji["overrides"];
                inst.baseColorOverride = jov.value("baseColor", inst.baseColorOverride);
                inst.normalOverride = jov.value("normal", inst.normalOverride);
                inst.ormOverride = jov.value("orm", inst.ormOverride);
                inst.emissiveOverride = jov.value("emissive", inst.emissiveOverride);
                inst.opacityMaskOverride = jov.value("opacityMask", inst.opacityMaskOverride);
                inst.hairDepthOverride = jov.value("hairDepth", inst.hairDepthOverride);
                inst.hairIDOverride = jov.value("hairID", inst.hairIDOverride);
                inst.irisHeightOverride = jov.value("irisHeight", inst.irisHeightOverride);
                inst.irisMaskOverride = jov.value("irisMask", inst.irisMaskOverride);
            }

            if (ji.contains("textures") && ji["textures"].is_object())
            {
                const json& jt = ji["textures"];
                if (jt.contains("baseColor")) inst.baseColorPath = readTexturePathFromJson(jt["baseColor"]);
                if (jt.contains("normal")) inst.normalPath = readTexturePathFromJson(jt["normal"]);
                if (jt.contains("orm")) inst.ormPath = readTexturePathFromJson(jt["orm"]);
                if (jt.contains("emissive")) inst.emissivePath = readTexturePathFromJson(jt["emissive"]);
                if (jt.contains("opacityMask")) inst.opacityMaskPath = readTexturePathFromJson(jt["opacityMask"]);
                if (jt.contains("hairDepth")) inst.hairDepthPath = readTexturePathFromJson(jt["hairDepth"]);
                if (jt.contains("hairID")) inst.hairIDPath = readTexturePathFromJson(jt["hairID"]);
                if (jt.contains("irisHeight")) inst.irisHeightPath = readTexturePathFromJson(jt["irisHeight"]);
                if (jt.contains("irisMask")) inst.irisMaskPath = readTexturePathFromJson(jt["irisMask"]);
            }

            if (ji.contains("cb") && ji["cb"].is_object())
            {
                const json& jcb = ji["cb"];
                if (jcb.contains("baseColorTint") && jcb["baseColorTint"].is_array() && jcb["baseColorTint"].size() >= 4)
                    inst.cb.baseColorTint = Engine::Vec4(jcb["baseColorTint"][0], jcb["baseColorTint"][1], jcb["baseColorTint"][2], jcb["baseColorTint"][3]);
                if (jcb.contains("emissiveColor") && jcb["emissiveColor"].is_array() && jcb["emissiveColor"].size() >= 4)
                    inst.cb.emissiveColor = Engine::Vec4(jcb["emissiveColor"][0], jcb["emissiveColor"][1], jcb["emissiveColor"][2], jcb["emissiveColor"][3]);

                inst.cb.emissiveIntensity = jcb.value("emissiveIntensity", inst.cb.emissiveIntensity);
                inst.cb.emissiveMaskMul = jcb.value("emissiveMaskMul", inst.cb.emissiveMaskMul);
                inst.cb.normalStrength = jcb.value("normalStrength", inst.cb.normalStrength);
                inst.cb.opacityMaskClipValue = jcb.value("opacityMaskClipValue", inst.cb.opacityMaskClipValue);
                inst.cb.roughnessScale = jcb.value("roughnessScale", inst.cb.roughnessScale);
                inst.cb.metalnessScale = jcb.value("metalnessScale", inst.cb.metalnessScale);
                inst.cb.aoScale = jcb.value("aoScale", inst.cb.aoScale);

                if (jcb.contains("uvScale") && jcb["uvScale"].is_array() && jcb["uvScale"].size() >= 2)
                    inst.cb.uvScale = Engine::Vec2(jcb["uvScale"][0], jcb["uvScale"][1]);
                if (jcb.contains("uvOffset") && jcb["uvOffset"].is_array() && jcb["uvOffset"].size() >= 2)
                    inst.cb.uvOffset = Engine::Vec2(jcb["uvOffset"][0], jcb["uvOffset"][1]);
            }

            const MaterialRefKind kind = parseRefKind(ji.value("refKind", string{}));
            if (kind == MaterialRefKind::MI)
            {
                _materialInstances.emplace(inst.assetName, move(inst));
            }
            else
            {
                _manualInstanceKinds[inst.assetName] = kind;
                _manualInstances.emplace(inst.assetName, move(inst));
            }
        }
    }

    if (root.contains("mappings") && root["mappings"].is_array())
    {
        for (const json& jm : root["mappings"])
        {
            if (!jm.is_object())
                continue;

            MaterialMapping m{};
            m.source = MaterialSourceKind::SK;
            m.sourceFile = p.filename().string();
            m.materialIndex = jm.value("materialIndex", 0u);
            m.originalMaterialIndex = m.materialIndex;
            m.slotName = jm.value("slotName", string{});

            if (jm.contains("ref") && jm["ref"].is_object())
            {
                const json& jref = jm["ref"];
                m.material.assetName = jref.value("assetName", string{});
                const string kindStr = jref.value("kind", string{});
                m.material.kind = (kindStr == "MI") ? MaterialRefKind::MI : (kindStr == "M") ? MaterialRefKind::M : MaterialRefKind::Unknown;
                m.material.objectName = jref.value("objectName", string{});
                m.material.objectPath = jref.value("objectPath", string{});
            }

            _materialMappings.push_back(move(m));
        }
    }

    _selectedBundlePath = p.wstring();
    _bundleOutputPath = _selectedBundlePath;

    // 번들 파일 안 경로들은 Resources 루트 기준 상대 경로라서, texture root는 저장 위치 기준으로 세팅해두는 게 편하다.
    if (p.has_parent_path())
    {
        _textureSearchRoot = resourceRoot.wstring() + L"\\Models";
        _textureIndexDirty = true;
    }

    return true;
}

bool Material_View::Load_FromSelectedModelCom()
{
    // 선택된 ModelCom의 Runtime Material 상태를 읽어서 편집 상태로 만든다.
    // 목표:
    // - 런타임에서 조정된 CB 값(색/강도/UV 등)을 그대로 가져오기
    // - SRV가 있는 슬롯은 resolvedPath를 경로로 노출하기
    const Editor_Context* context = EDITOR->Get_EditorContext();
    if (context == nullptr)
        return false;

    const Shared<GameObject> selectedObject = context->Get_Selection().selectedObject;
    if (selectedObject == nullptr)
        return false;

    const Shared<ModelCom> model = selectedObject->Get_Component<ModelCom>();
    if (model == nullptr)
        return false;

    _materialMappings.clear();
    _materialInstances.clear();
    _manualInstances.clear();
    _manualInstanceKinds.clear();
    _heuristics.clear();
    _activeMappingGroup = 0;

    const uint32 numMaterials = static_cast<uint32>(model->Get_NumMaterials());
    const auto& bindings = model->Get_MaterialBindings();
    for (uint32 materialIndex = 0; materialIndex < numMaterials; ++materialIndex)
    {
        const Shared<Material> mat = model->Get_Material(materialIndex);
        if (mat == nullptr)
            continue;

        const string assetName = mat->Get_MaterialName().empty()
            ? ("Material_" + to_string(materialIndex))
            : mat->Get_MaterialName();

        MaterialMapping mapping{};
        mapping.source = MaterialSourceKind::SK;
        mapping.sourceFile = "SelectedModelCom";
        mapping.materialIndex = materialIndex;
        mapping.originalMaterialIndex = materialIndex;
        mapping.slotName = "Runtime";
        mapping.material.assetName = assetName;
        mapping.material.kind = MaterialRefKind::Unknown;
        _materialMappings.push_back(mapping);

        MaterialInstanceData inst{};
        inst.assetName = assetName;
        inst.templateId = mat->Get_TemplateId();
        inst.renderState = mat->Get_RenderState();
        inst.cb = mat->Get_CB();

        // override 체크는 "런타임 SRV 유무"가 아니라 ModelCom instance override를 기준으로 한다.
        // (베이스 텍스처가 존재하기만 해도 전부 override로 켜지는 문제 방지)
        const Engine::MaterialBinding* binding = (materialIndex < bindings.size()) ? &bindings[materialIndex] : nullptr;
        const Engine::MaterialInstanceOverride* instOv = (binding != nullptr) ? &binding->instance : nullptr;

        if (instOv != nullptr)
        {
            inst.renderStateOverrideEnabled = instOv->overrideRenderStateEnabled;

            // cb mask 복사
#define EDITOR_COPY_CB_MASK(_key, _member, _kind) inst.cbMask._member = instOv->cbMask._member;
            MATERIAL_CB_OVERRIDE_FIELDS(EDITOR_COPY_CB_MASK)
#undef EDITOR_COPY_CB_MASK
        }
        else
        {
            inst.renderStateOverrideEnabled = false;
            inst.cbMask.Clear();
        }

        auto slotToPath = [&](MaterialTextureSlot slot, wstring& outPath, bool& outOverride, const Engine::TextureOverride* ov) -> void
        {
            if (ov != nullptr && ov->enabled)
            {
                outPath = ov->path;
                outOverride = true;
                return;
            }

            const Material::TextureSlot* s = mat->Find_TextureSlot(slot, 0);
            if (s != nullptr && s->shaderResourceView != nullptr)
            {
                // 표시용: base/resolved path는 보여주되 override는 켜지지 않는다.
                outPath = s->resolvedPath;
                outOverride = false;
            }
            else
            {
                outPath.clear();
                outOverride = false;
            }
        };

        const Engine::TextureOverride* ovBaseColor = (instOv != nullptr) ? &instOv->baseColor : nullptr;
        const Engine::TextureOverride* ovNormal = (instOv != nullptr) ? &instOv->normal : nullptr;
        const Engine::TextureOverride* ovOrm = (instOv != nullptr) ? &instOv->orm : nullptr;
        const Engine::TextureOverride* ovEmissive = (instOv != nullptr) ? &instOv->emissive : nullptr;
        const Engine::TextureOverride* ovOpacityMask = (instOv != nullptr) ? &instOv->opacityMask : nullptr;
        const Engine::TextureOverride* ovHairDepth = (instOv != nullptr) ? &instOv->hairDepth : nullptr;
        const Engine::TextureOverride* ovHairID = (instOv != nullptr) ? &instOv->hairID : nullptr;
        const Engine::TextureOverride* ovIrisHeight = (instOv != nullptr) ? &instOv->irisHeight : nullptr;
        const Engine::TextureOverride* ovIrisMask = (instOv != nullptr) ? &instOv->irisMask : nullptr;

        slotToPath(MaterialTextureSlot::BaseColor, inst.baseColorPath, inst.baseColorOverride, ovBaseColor);
        slotToPath(MaterialTextureSlot::Normal, inst.normalPath, inst.normalOverride, ovNormal);
        slotToPath(MaterialTextureSlot::ORM, inst.ormPath, inst.ormOverride, ovOrm);
        slotToPath(MaterialTextureSlot::Emissive, inst.emissivePath, inst.emissiveOverride, ovEmissive);
        slotToPath(MaterialTextureSlot::OpacityMask, inst.opacityMaskPath, inst.opacityMaskOverride, ovOpacityMask);
        slotToPath(MaterialTextureSlot::HairDepth, inst.hairDepthPath, inst.hairDepthOverride, ovHairDepth);
        slotToPath(MaterialTextureSlot::HairID, inst.hairIDPath, inst.hairIDOverride, ovHairID);
        slotToPath(MaterialTextureSlot::IrisHeight, inst.irisHeightPath, inst.irisHeightOverride, ovIrisHeight);
        slotToPath(MaterialTextureSlot::IrisMask, inst.irisMaskPath, inst.irisMaskOverride, ovIrisMask);

        _manualInstances.emplace(assetName, move(inst));
        _manualInstanceKinds[assetName] = MaterialRefKind::Unknown;
    }

    return !_materialMappings.empty();
}

bool Material_View::Try_PickTextureFileDialog(const wstring& title, const wstring& defaultFolder, wstring& outFilePath)
{
    // 텍스처 파일을 직접 고르는 다이얼로그다
    // 기본 폴더는 Texture Root에서 시작하는 게 자연스럽다
    outFilePath.clear();

    const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool shouldUninit = SUCCEEDED(initHr);

    IFileOpenDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    if (FAILED(hr) || dialog == nullptr)
    {
        if (shouldUninit)
            CoUninitialize();
        return false;
    }

    dialog->SetTitle(title.c_str());

    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST);

    const fs::path resolvedDefaultFolder = MaterialCreateHelper::Try_ResolveDirectoryPath(defaultFolder);
    if (!resolvedDefaultFolder.empty())
    {
        IShellItem* folderItem = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(resolvedDefaultFolder.wstring().c_str(), nullptr, IID_PPV_ARGS(&folderItem))) && folderItem != nullptr)
        {
            dialog->SetFolder(folderItem);
            folderItem->Release();
        }
    }

    const COMDLG_FILTERSPEC filters[] =
    {
        { L"Textures", L"*.png;*.jpg;*.jpeg;*.tga;*.dds;*.bmp" },
        { L"All", L"*.*" },
    };
    dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);

    hr = dialog->Show(nullptr);
    if (SUCCEEDED(hr))
    {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr)
        {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr)
            {
                outFilePath = path;
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }

    dialog->Release();

    if (shouldUninit)
        CoUninitialize();

    return !outFilePath.empty();
}

bool Material_View::Save_BundleFile(const wstring& outputPath) const
{
    try
    {
        // 저장이 실패했을 때 화면에 바로 표시하려고 상태값을 멤버로 들고있음. 호출할 때마다 초기화해둬야 이전 실패 메시지가 계속 남지 않는다
        _lastBundleSaveOk = false;
        _lastBundleSaveAbsPath.clear();
        _lastBundleSaveError.clear();

        fs::path outPath(outputPath);
        if (outPath.empty())
        {
            _lastBundleSaveError = "outputPath is empty";
            return false;
        }

        // 이름만 들어온 경우를 대비해서, 확장자가 없으면 .material.json을 붙인다.
        // 확장자가 있으면 그대로 사용한다.
        if (outPath.extension().empty())
        {
            const string base = String::ToString(outPath.wstring());
            if (base.ends_with(".material"))
                outPath.replace_extension(".json");
            else
                outPath = fs::path(base + ".material.json");
        }

        // 상대 경로로 받았더라도 실제 파일 쓰기는 절대 경로로 정리해서 진행한다
        if (outPath.is_relative())
            outPath = fs::absolute(outPath);
        outPath = outPath.lexically_normal();

        std::error_code ec{};
        if (outPath.has_parent_path() && (!fs::exists(outPath.parent_path(), ec) || ec))
        {
            _lastBundleSaveError = "output folder does not exist: " + String::ToString(outPath.parent_path().wstring());
            return false;
        }

        // 번들 파일이 들어갈 Resources 폴더를 기준으로 상대 경로를 저장한다
        const fs::path resourceRoot = outPath.has_parent_path() ? outPath.parent_path().parent_path() : fs::path{};

        auto toRelativeUnderResourceRoot = [&](const wstring& p) -> string
        {
            // p가 비어있으면 비어있는 문자열로 저장한다
            // 절대 경로가 들어오면 resourceRoot 기준으로 상대 경로로 바꿔서 저장한다
            // 상대 경로로 못 만들면 빈 문자열을 돌려서, 저장물에 잘못된 경로가 섞이지 않게 한다
            if (p.empty())
                return {};

            fs::path abs(p);
            if (abs.is_relative())
                abs = fs::absolute(abs);
            abs = abs.lexically_normal();

            if (!resourceRoot.empty())
            {
                fs::path rel = abs.lexically_relative(resourceRoot);
                if (!rel.empty() && !rel.is_absolute())
                    return rel.generic_string();
            }

            return {};
        };

        auto writeTextureRef = [&](const wstring& p) -> json
        {
            if (p.empty())
                return "";

            const string textureGuid = GAME->Ensure_AssetGUID(p, "Texture");
            if (!textureGuid.empty())
            {
                json ref{};
                ref["texture_guid"] = textureGuid;

                const string relFallback = toRelativeUnderResourceRoot(p);
                if (!relFallback.empty())
                    ref["texture_path"] = relFallback;

                return ref;
            }

            const string relFallback = toRelativeUnderResourceRoot(p);
            if (relFallback.empty())
                return "";

            json ref{};
            ref["texture_path"] = relFallback;
            return ref;
        };

        json out{};
        // textureRoot는 에디터가 어디서 텍스처를 찾아왔는지 정도만 남긴다
        out["textureRoot"] = toRelativeUnderResourceRoot(_textureSearchRoot);
        out["miRoot"] = string{};

        // 템플릿 테이블도 같이 저장
        out["templates"] = json::array();
        for (const MaterialTemplateDesc& t : kTemplateTable)
        {
            json jt{};
            jt["id"] = static_cast<uint32>(t.id);
            jt["name"] = t.name;
            jt["supportsBaseColor"] = t.supportsBaseColor;
            jt["supportsNormal"] = t.supportsNormal;
            jt["supportsORM"] = t.supportsORM;
            jt["supportsEmissive"] = t.supportsEmissive;
            jt["supportsOpacityMask"] = t.supportsOpacityMask;
            out["templates"].push_back(move(jt));
        }

        out["mappings"] = json::array();
        for (const MaterialMapping& m : _materialMappings)
        {
            // mappings에는 원본 파일이 어떤 ref를 가리켰는지 그대로 남긴다
            // 여기까지는 텍스처 resolve 성공 여부와 무관하게 기록한다
            json jm{};
            jm["sourceFile"] = m.sourceFile;

            switch (m.source)
            {
            case MaterialSourceKind::SK: jm["source"] = "SK"; break;
            case MaterialSourceKind::SM: jm["source"] = "SM"; break;
            case MaterialSourceKind::FX: jm["source"] = "FX"; break;
            default: jm["source"] = "Unknown"; break;
            }

            if (m.source == MaterialSourceKind::FX)
            {
                jm["emitter"] = m.emitter;
                jm["rendererType"] = m.rendererType;
                jm["rendererIndex"] = m.rendererIndex;
                jm["slotIndex"] = m.slotIndex;
            }
            else
            {
                jm["materialIndex"] = m.materialIndex;
                jm["slotName"] = m.slotName;
            }

            json jref{};
            jref["objectName"] = m.material.objectName;
            jref["objectPath"] = m.material.objectPath;
            jref["assetName"] = m.material.assetName;

            switch (m.material.kind)
            {
            case MaterialRefKind::MI: jref["kind"] = "MI"; break;
            case MaterialRefKind::M: jref["kind"] = "M"; break;
            default: jref["kind"] = "Unknown"; break;
            }

            jm["ref"] = move(jref);
            out["mappings"].push_back(move(jm));
        }

        out["instances"] = json::array();

        auto kindToStr = [](MaterialRefKind kind) -> const char*
        {
            switch (kind)
            {
            case MaterialRefKind::MI: return "MI";
            case MaterialRefKind::M: return "M";
            default: return "Unknown";
            }
        };

        auto writeInstance = [&](const MaterialInstanceData& inst, MaterialRefKind kind) -> void
        {
            // instances는 최종 resolve 결과를 모은 것이다
            // 텍스처는 리소스 루트 기준 상대 경로만 저장한다
            json ji{};
            ji["assetName"] = inst.assetName;
            ji["template"] = static_cast<uint32>(inst.templateId);
            ji["refKind"] = kindToStr(kind);
            ji["forcePassIndex"] = inst.forcedPassIndex;

            json jstate{};
            jstate["twoSided"] = inst.renderState.twoSided;
            jstate["blendMode"] = static_cast<uint32>(inst.renderState.blendMode);
            jstate["opacityMaskClipValue"] = inst.renderState.opacityMaskClipValue;
            ji["renderState"] = move(jstate);

            json jtex{};
            jtex["baseColor"] = writeTextureRef(inst.baseColorPath);
            jtex["normal"] = writeTextureRef(inst.normalPath);
            jtex["orm"] = writeTextureRef(inst.ormPath);
            jtex["emissive"] = writeTextureRef(inst.emissivePath);
            jtex["opacityMask"] = writeTextureRef(inst.opacityMaskPath);
            jtex["hairDepth"] = writeTextureRef(inst.hairDepthPath);
            jtex["hairID"] = writeTextureRef(inst.hairIDPath);
            jtex["irisHeight"] = writeTextureRef(inst.irisHeightPath);
            jtex["irisMask"] = writeTextureRef(inst.irisMaskPath);
            ji["textures"] = move(jtex);

            json jov{};
            jov["baseColor"] = inst.baseColorOverride;
            jov["normal"] = inst.normalOverride;
            jov["orm"] = inst.ormOverride;
            jov["emissive"] = inst.emissiveOverride;
            jov["opacityMask"] = inst.opacityMaskOverride;
            jov["hairDepth"] = inst.hairDepthOverride;
            jov["hairID"] = inst.hairIDOverride;
            jov["irisHeight"] = inst.irisHeightOverride;
            jov["irisMask"] = inst.irisMaskOverride;
            ji["overrides"] = move(jov);

            json jcb{};
            jcb["baseColorTint"] = { inst.cb.baseColorTint.x, inst.cb.baseColorTint.y, inst.cb.baseColorTint.z, inst.cb.baseColorTint.w };
            jcb["emissiveColor"] = { inst.cb.emissiveColor.x, inst.cb.emissiveColor.y, inst.cb.emissiveColor.z, inst.cb.emissiveColor.w };
            jcb["emissiveIntensity"] = inst.cb.emissiveIntensity;
            jcb["emissiveMaskMul"] = inst.cb.emissiveMaskMul;
            jcb["normalStrength"] = inst.cb.normalStrength;
            jcb["opacityMaskClipValue"] = inst.cb.opacityMaskClipValue;
            jcb["roughnessScale"] = inst.cb.roughnessScale;
            jcb["metalnessScale"] = inst.cb.metalnessScale;
            jcb["aoScale"] = inst.cb.aoScale;
            jcb["uvScale"] = { inst.cb.uvScale.x, inst.cb.uvScale.y };
            jcb["uvOffset"] = { inst.cb.uvOffset.x, inst.cb.uvOffset.y };
            ji["cb"] = move(jcb);

            out["instances"].push_back(move(ji));
        };

        for (const auto& kv : _materialInstances)
            writeInstance(kv.second, MaterialRefKind::MI);

        for (const auto& kv : _manualInstances)
        {
            const auto itKind = _manualInstanceKinds.find(kv.first);
            const MaterialRefKind kind = (itKind != _manualInstanceKinds.end()) ? itKind->second : MaterialRefKind::Unknown;
            writeInstance(kv.second, kind);
        }

        out["heuristics"] = json::array();
        for (const HeuristicNote& n : _heuristics)
        {
            // 나중에 다시 봤을 때 왜 이런 결과가 나왔는지 추적하기 위한 메모다
            json jn{};
            jn["kind"] = n.kind;
            jn["detail"] = n.detail;
            out["heuristics"].push_back(move(jn));
        }

        // 여기서부터 실제 파일 쓰기다
        // dump에서 error handler는 replace로 두어서, 문자열 깨짐이 있어도 저장 자체는 계속 가게 해둔다
        ofstream file(outPath);
        if (!file.is_open())
        {
            _lastBundleSaveError = "failed to open file for write: " + String::ToString(outPath.wstring());
            return false;
        }

        file << out.dump(4, ' ', false, json::error_handler_t::replace);
        file.close();

        ec.clear();
        if (!fs::exists(outPath, ec) || ec)
        {
            _lastBundleSaveError = ec ? ("write finished but exists() failed: " + ec.message()) : "write finished but file not found";
            return false;
        }

        _lastBundleSaveOk = true;
        _lastBundleSaveAbsPath = outPath.wstring();
        return true;
    }
    catch (...)
    {
        _lastBundleSaveError = "exception";
        return false;
    }
}

Shared<Material_View> Material_View::Create()
{
    return make_shared<Material_View>();
}

void Material_View::Free()
{
    Editor_Window::Free();
}

void Material_View::Clear_TexturePreviewCache()
{
    g_MaterialViewTexturePreviewCache.clear();
    g_MaterialViewTextureSearchCache.clear();
    g_MaterialViewTextureCandidateCache.clear();
}

NS_END
