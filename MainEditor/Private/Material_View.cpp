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
#include "Material.h"

#include <algorithm>
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

    TexturePreviewCache g_MaterialViewTexturePreviewCache{};
    TextureSearchCache g_MaterialViewTextureSearchCache{};

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
    if (ImGui::Button("Create With Json"))
        _mode = CreateMode::CreateWithJson;

    ImGui::SameLine();
    if (ImGui::Button("Create Material"))
        _mode = CreateMode::CreateMaterial;
}

void Material_View::Draw_Settings()
{
    // 텍스처 루트는 자주 바꾸는 값이라 여기에서 바로 편집하게 둔다
    ImGui::SeparatorText("Settings");

    static char texRootBuf[512]{};

    auto syncToBufOnce = [](char* buf, size_t bufSize, const wstring& value)
    {
        if (buf[0] != '\0')
            return;
        const string s = String::ToString(value);
        strncpy_s(buf, bufSize, s.c_str(), _TRUNCATE);
    };

    syncToBufOnce(texRootBuf, sizeof(texRootBuf), _textureSearchRoot);

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

    // 저장 경로가 상대경로면, 실제로는 프로세스 CWD 기준으로 해석된다
    // 파일이 어디로 저장됐는지 헷갈릴 때가 많아서, 여기서 절대 경로를 같이 보여준다
    {
        fs::path outPath(_bundleOutputPath);
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
    }

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

    auto drawTemplatePicker = [&](MaterialInstanceData& inst) -> void
    {
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
                    inst.templateId = desc.id;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    };

    auto drawBlendModePicker = [&](Engine::MaterialRenderState& state) -> void
    {
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
                    state.blendMode = static_cast<Engine::MaterialBlendMode>(i);
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    };

    auto drawSlotEditor = [&](const char* label, bool& overrideFlag, wstring& slotPath) -> void
    {
        // 슬롯별 UI는 Model_Inspector에서 하던 방식과 거의 같다
        if (!ImGui::TreeNode(label))
            return;

        ImGui::Checkbox("Enabled", &overrideFlag);

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

        const vector<fs::path> textureCandidates = Collect_TextureCandidates(slotPath);
        if (!textureCandidates.empty())
        {
            ImGui::TextDisabled("Texture");
            ImGui::SameLine(120.f);

            const string currentFileName = slotPath.empty() ? "-" : String::ToString(fs::path(slotPath).filename().wstring());
            if (ImGui::BeginCombo("##TexturePicker", currentFileName.c_str()))
            {
                const string pickerKey = string(label) + "_" + currentFileName;
                string& searchText = g_MaterialViewTextureSearchCache[pickerKey];
                char searchBuffer[256]{};
                strncpy_s(searchBuffer, searchText.c_str(), _TRUNCATE);

                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::InputTextWithHint("##TextureSearch", "Search texture...", searchBuffer, sizeof(searchBuffer)))
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
        if (ImGui::Button("Browse##Slot"))
        {
            wstring picked{};
            if (Try_PickTextureFileDialog(L"Select Texture", _textureSearchRoot, picked))
            {
                slotPath = picked;
                overrideFlag = true;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear##Slot"))
        {
            slotPath.clear();
            overrideFlag = true;
        }

        ImGui::TreePop();
    };

    for (MaterialMapping* mPtr : activeMappings)
    {
        MaterialMapping& m = *mPtr;
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
                m.materialIndex = static_cast<uint32>((std::max)(0, idx));

            ImGui::SameLine();
            if (ImGui::Button("Reset##MaterialIndex"))
                m.materialIndex = m.originalMaterialIndex;

            ImGui::TextDisabled("Slot");
            ImGui::SameLine(120.f);
            ImGui::TextUnformatted(m.slotName.c_str());
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
        drawSlotEditor("BaseColor", inst->baseColorOverride, inst->baseColorPath);
        drawSlotEditor("Normal", inst->normalOverride, inst->normalPath);
        drawSlotEditor("ORM", inst->ormOverride, inst->ormPath);
        drawSlotEditor("Emissive", inst->emissiveOverride, inst->emissivePath);
        drawSlotEditor("OpacityMask", inst->opacityMaskOverride, inst->opacityMaskPath);

        ImGui::SeparatorText("Parameters");
        drawTemplatePicker(*inst);

        ImGui::Checkbox("TwoSided", &inst->renderState.twoSided);
        drawBlendModePicker(inst->renderState);
        ImGui::DragFloat("Clip", &inst->renderState.opacityMaskClipValue, 0.001f, 0.f, 1.f);

        float baseTint[4] = { inst->cb.baseColorTint.x, inst->cb.baseColorTint.y, inst->cb.baseColorTint.z, inst->cb.baseColorTint.w };
        if (ImGui::InputFloat4("BaseTint", baseTint))
            inst->cb.baseColorTint = Engine::Vec4(baseTint[0], baseTint[1], baseTint[2], baseTint[3]);

        float emiColor[4] = { inst->cb.emissiveColor.x, inst->cb.emissiveColor.y, inst->cb.emissiveColor.z, inst->cb.emissiveColor.w };
        if (ImGui::InputFloat4("EmissiveColor", emiColor))
            inst->cb.emissiveColor = Engine::Vec4(emiColor[0], emiColor[1], emiColor[2], emiColor[3]);

        ImGui::DragFloat("EmissiveIntensity", &inst->cb.emissiveIntensity, 0.01f, 0.f, 100.f);
        ImGui::DragFloat("EmissiveMaskMul", &inst->cb.emissiveMaskMul, 0.01f, 0.f, 100.f);
        ImGui::DragFloat("NormalStrength", &inst->cb.normalStrength, 0.01f, 0.f, 10.f);
        ImGui::DragFloat("OpacityClipCB", &inst->cb.opacityMaskClipValue, 0.001f, 0.f, 1.f);
        ImGui::DragFloat("RoughnessScale", &inst->cb.roughnessScale, 0.01f, 0.f, 10.f);
        ImGui::DragFloat("MetalnessScale", &inst->cb.metalnessScale, 0.01f, 0.f, 10.f);
        ImGui::DragFloat("AoScale", &inst->cb.aoScale, 0.01f, 0.f, 10.f);

        ImGui::PopID();
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
    ImGui::TextDisabled("Create Material is not available yet.");
    ImGui::TextDisabled("Use Create With Json for now.");
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


void Material_View::Apply_ToSelectedModel()
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

        // 상대 경로로 받았더라도 실제 파일 쓰기는 절대 경로로 정리해서 진행한다
        if (outPath.is_relative())
            outPath = fs::absolute(outPath);
        outPath = outPath.lexically_normal();

        std::error_code ec{};
        if (outPath.has_parent_path() && !fs::exists(outPath.parent_path(), ec))
        {
            // 폴더가 없으면 먼저 만들어준다
            // 여기서 실패하면 파일을 열 수가 없어서 바로 종료한다
            ec.clear();
            fs::create_directories(outPath.parent_path(), ec);
            if (ec)
            {
                _lastBundleSaveError = "create_directories failed: " + ec.message();
                return false;
            }
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

            json jstate{};
            jstate["twoSided"] = inst.renderState.twoSided;
            jstate["blendMode"] = static_cast<uint32>(inst.renderState.blendMode);
            jstate["opacityMaskClipValue"] = inst.renderState.opacityMaskClipValue;
            ji["renderState"] = move(jstate);

            json jtex{};
            jtex["baseColor"] = toRelativeUnderResourceRoot(inst.baseColorPath);
            jtex["normal"] = toRelativeUnderResourceRoot(inst.normalPath);
            jtex["orm"] = toRelativeUnderResourceRoot(inst.ormPath);
            jtex["emissive"] = toRelativeUnderResourceRoot(inst.emissivePath);
            jtex["opacityMask"] = toRelativeUnderResourceRoot(inst.opacityMaskPath);
            ji["textures"] = move(jtex);

            json jov{};
            jov["baseColor"] = inst.baseColorOverride;
            jov["normal"] = inst.normalOverride;
            jov["orm"] = inst.ormOverride;
            jov["emissive"] = inst.emissiveOverride;
            jov["opacityMask"] = inst.opacityMaskOverride;
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

NS_END
