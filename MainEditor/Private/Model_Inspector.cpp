#include "Model_Inspector.h"

#include "GameInstance.h"
#include "Animation_NameUtils.h"
#include "ModelCom.h"
#include "Material.h"
#include <filesystem>
#include "EditorInstance.h"
#include "Animation_View.h"
#include "Editor_Context.h"
#include "Editor_Window.h"
#include "Prefab_View.h"

using TexturePreviewCache   = umap<wstring, ComPtr<ID3D11ShaderResourceView>>;
using TextureSearchCache    = umap<string, string>;

static TexturePreviewCache  g_TexturePreviewCache {};
static TextureSearchCache   g_TextureSearchCache  {};

void ModelCom_Inspector::Draw_Inspector(const Shared<Component>& component)
{
    Shared<ModelCom> model = dynamic_pointer_cast<ModelCom>(component);
    if (model == nullptr)
        return;

    if (!Draw_Header("Model"))
        return;

    Draw_ModelPicker(model);
    ImGui::Separator();

    const wstring& sourceModelPath = model->Get_SourceModelPath();
    const string modelGuid = sourceModelPath.empty() ? string{} : GAME->Ensure_AssetGUID(sourceModelPath, "Model");

    ImGui::TextDisabled("Source Model");
    ImGui::SameLine(120.f);
    ImGui::BeginDisabled();
    ImGui::TextWrapped("%s", String::ToString(sourceModelPath).c_str());
    ImGui::EndDisabled();

    ImGui::TextDisabled("Model GUID");
    ImGui::SameLine(120.f);
    ImGui::TextWrapped("%s", modelGuid.empty() ? "(none)" : modelGuid.c_str());

    ImGui::SeparatorText("Shader Types");
    
    const auto& context = EDITOR->Selected_Object();

    if (context)
    {
        auto shader = context->Get_Component<ShaderCom>();
        if (shader)
        {
            ShaderType shaderType = shader->Get_Type();
            const auto shaderTypeName = magic_enum::enum_name(shaderType);

            ImGui::TextDisabled("ShaderType");
            ImGui::SameLine(120.f);

            if (!shaderTypeName.empty())
                ImGui::Text("%s", string(shaderTypeName).c_str());
            else
                ImGui::Text("Unknown(%d)", static_cast<int32>(shaderType));
        }
    }

    Draw_AnimationSection(model);

    ImGui::TextDisabled("Meshes");
    ImGui::SameLine(120.f);
    ImGui::Text("%u", static_cast<uint32>(model->Get_NumMeshes()));

    ImGui::TextDisabled("Materials");
    ImGui::SameLine(120.f);
    ImGui::Text("%u", model->Get_NumMaterials());

    ImGui::TextDisabled("Texture Types");
    ImGui::SameLine(120.f);
    ImGui::Text("%u", model->Get_MaxTextureType());

    ImGui::SeparatorText("Mesh Slots");

    const uint32 numMeshes = static_cast<uint32>(model->Get_NumMeshes());
    for (uint32 meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
    {
        const uint32 materialIndex = model->Get_MeshMaterialIndex(meshIndex);
        Shared<Material> material = model->Get_Material(materialIndex);

        string label = "Mesh[" + to_string(meshIndex) + "]";
        if (material)
            label += " -> Material[" + to_string(materialIndex) + "] : " + material->Get_MaterialName();
        else
            label += " -> Material[Invalid]";

        ImGui::BulletText("%s", label.c_str());
    }

    ImGui::SeparatorText("Materials");

    // 각 머티리얼 내부 텍스처 슬롯 표시
    const uint32 numMaterials = static_cast<uint32>(model->Get_NumMaterials());
    for (uint32 materialIndex = 0; materialIndex < numMaterials; ++materialIndex)
    {
        Shared<Material> material = model->Get_Material(materialIndex);
        if (material == nullptr)
            continue;

        ImGui::PushID(static_cast<int>(materialIndex));

        string header = "Material[" + to_string(materialIndex) + "] : " + material->Get_MaterialName();
        if (!ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PopID();
            continue;
        }

       const uint32 currentPass = material->Get_PassIndex();

        ImGui::TextDisabled("Current Pass");
        ImGui::SameLine(120.f);

        if (currentPass >= 0)
            ImGui::Text("%d", currentPass);
        else
            ImGui::TextDisabled("(none)");

        const vector<vector<Material::TextureSlot>>& allSlots = material->Get_TextureSlots();

        auto drawTextureSlot = [&](uint32 typeIndex, uint32 slotIndex, const Material::TextureSlot* slot)
        {
            const wstring resolvedPath = slot ? slot->resolvedPath : wstring{};
            ID3D11ShaderResourceView* slotSrv = slot ? slot->shaderResourceView.Get() : nullptr;

            ImGui::PushID(static_cast<int>(slotIndex));

            ImGui::TextDisabled("TextureIndex");
            ImGui::SameLine(120.f);
            ImGui::Text("%u", slot ? slot->textureIndex : slotIndex);

            ImGui::TextDisabled("Loaded");
            ImGui::SameLine(120.f);
            ImGui::TextUnformatted(slotSrv ? "Yes" : "No");

            ImGui::TextDisabled("Path");
            ImGui::SameLine(120.f);
            ImGui::BeginDisabled();
            const string displayPath = resolvedPath.empty() ? "(empty)" : To_DisplayTexturePath(resolvedPath);
            ImGui::TextWrapped("%s", displayPath.c_str());
            ImGui::EndDisabled();

            if (slotSrv)
            {
                ImGui::TextDisabled("Preview");
                ImGui::SameLine(120.f);
                ImGui::Image(reinterpret_cast<ImTextureID>(slotSrv), ImVec2(64.f, 64.f));
            }

            const vector<fs::path> textureCandidates = Collect_TextureCandidates(resolvedPath, sourceModelPath);
            if (!textureCandidates.empty())
            {
                ImGui::TextDisabled("Texture");
                ImGui::SameLine(120.f);

                if (slotSrv)
                {
                    ImGui::Image(reinterpret_cast<ImTextureID>(slotSrv), ImVec2(32.f, 32.f));
                    ImGui::SameLine();
                }

                const string currentFileName = resolvedPath.empty()
                    ? string{ "<Select Texture>" }
                    : String::ToString(fs::path(resolvedPath).filename().wstring());
                const string pickerKey = material->Get_MaterialName() + "_" + to_string(typeIndex) + "_" + to_string(slotIndex);
                if (ImGui::BeginCombo("##TexturePicker", currentFileName.c_str()))
                {
                    string& searchText = g_TextureSearchCache[pickerKey];
                    char searchBuffer[256]{};
                    strncpy_s(searchBuffer, searchText.c_str(), _TRUNCATE);

                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::InputTextWithHint("##TextureSearch", "Search texture...", searchBuffer, sizeof(searchBuffer)))
                        searchText = searchBuffer;

                    ImGui::Separator();

                    for (const fs::path& candidatePath : textureCandidates)
                    {
                        const string candidateName = String::ToString(candidatePath.filename().wstring());
                        const bool isSelected = !resolvedPath.empty() &&
                            candidatePath.wstring() == fs::path(resolvedPath).lexically_normal().wstring();

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

                        ID3D11ShaderResourceView* previewSrv = Get_TexturePreviewSRV(candidatePath);
                        if (previewSrv)
                            ImGui::Image(reinterpret_cast<ImTextureID>(previewSrv), ImVec2(32.f, 32.f));
                        else
                            ImGui::Dummy(ImVec2(32.f, 32.f));

                        ImGui::SameLine();

                        if (ImGui::Selectable(candidateName.c_str(), isSelected, 0, ImVec2(0.f, 32.f)))
                            material->Override_TextureSlot(static_cast<MaterialTextureSlot>(typeIndex), slotIndex, candidatePath.wstring());

                        if (isSelected)
                            ImGui::SetItemDefaultFocus();

                        ImGui::PopID();
                    }

                    ImGui::EndCombo();
                }
            }

            ImGui::Separator();
            ImGui::PopID();
        };

        for (uint32 typeIndex = 0; typeIndex < ETOI(MaterialTextureSlot::END); ++typeIndex)
        {
            const vector<Material::TextureSlot>* slots = typeIndex < allSlots.size() ? &allSlots[typeIndex] : nullptr;
            const size_t slotCount = slots ? slots->size() : 0;

            ImGui::PushID(static_cast<int>(typeIndex));

            string slotHeader = string(Get_TextureTypeLabel(typeIndex)) + " [" + to_string(typeIndex) + "] (" + to_string(slotCount) + ")";
            if (!ImGui::TreeNode(slotHeader.c_str()))
            {
                ImGui::PopID();
                continue;
            }

            if (slotCount == 0)
                drawTextureSlot(typeIndex, 0, nullptr);
            else
                for (uint32 slotIndex = 0; slotIndex < slotCount; ++slotIndex)
                    drawTextureSlot(typeIndex, slotIndex, &(*slots)[slotIndex]);

            ImGui::TreePop();
            ImGui::PopID();
        }

        ImGui::PopID();
    }
}

string ModelCom_Inspector::Build_ModelPickerLabel(const AssetMeta& assetMeta)
{
    const fs::path fullPath(assetMeta.fullPath);
    string label = String::ToString(fullPath.stem().wstring());

    if (!assetMeta.modelType.empty())
        label += " [" + assetMeta.modelType + "]";

    if (!assetMeta.relativePath.empty())
        label += " {" + String::ToString(assetMeta.relativePath) + "}";

    if (!assetMeta.guid.empty())
    {
        const string shortGuid = assetMeta.guid.size() > 8 ? assetMeta.guid.substr(0, 8) : assetMeta.guid;
        label += " <" + shortGuid + ">";
    }

    return label;
}

bool ModelCom_Inspector::Is_ModelAssetOfType(const AssetMeta& assetMeta, Engine::ModelType expectedModelType)
{
    if (assetMeta.type != "Model" && assetMeta.type != "model")
        return false;

    const string extension = String::ToLowerCopy(String::ToString(fs::path(assetMeta.fullPath).extension().wstring()));
    if (extension != ".model")
        return false;

    if (assetMeta.modelType.empty())
        return true;

    if (expectedModelType == Engine::ModelType::Anim)
        return assetMeta.modelType == "Anim" || assetMeta.modelType == "ANIM" || assetMeta.modelType == "SkeletalMesh";

    if (expectedModelType == Engine::ModelType::NonAnim)
        return assetMeta.modelType == "NonAnim" || assetMeta.modelType == "NONANIM" || assetMeta.modelType == "StaticMesh";

    return true;
}

void ModelCom_Inspector::Draw_ModelPicker(const Shared<Engine::ModelCom>& model)
{
    if (model == nullptr)
        return;

    static char searchBuffer[128] = "";

    const wstring& sourceModelPath = model->Get_SourceModelPath();
    const string currentGuid = sourceModelPath.empty() ? string{} : GAME->Ensure_AssetGUID(sourceModelPath, "Model");
    const string displayName = sourceModelPath.empty()
        ? "(None)"
        : String::ToString(fs::path(sourceModelPath).stem().wstring());

    const auto toModelTypeString = [](Engine::ModelType modelType)
    {
        return modelType == Engine::ModelType::Anim ? string("Anim") : string("NonAnim");
    };

    ImGui::TextDisabled("Mesh");
    ImGui::SameLine(120.f);

    if (ImGui::Button(displayName.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0.f)))
        ImGui::OpenPopup("##ModelComModelPicker");

    if (ImGui::BeginPopup("##ModelComModelPicker"))
    {
        ImGui::TextUnformatted("Model Select");
        ImGui::Separator();

        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##ModelSearch", "Search model...", searchBuffer, IM_ARRAYSIZE(searchBuffer));

        vector<AssetMeta> modelAssets = GAME->Get_AssetsByType("Model");
        if (modelAssets.empty())
            modelAssets = GAME->Get_AssetsByType("model");

        const string searchText = String::ToLowerCopy(string(searchBuffer));
        bool hasSelectableModel = false;

        for (const AssetMeta& assetMeta : modelAssets)
        {
            if (!Is_ModelAssetOfType(assetMeta, model->Get_ModelType()))
                continue;

            const string label = Build_ModelPickerLabel(assetMeta);
            if (!searchText.empty())
            {
                const string lowerLabel = String::ToLowerCopy(label);
                const string lowerPath = String::ToLowerCopy(String::ToString(assetMeta.fullPath));
                if (lowerLabel.find(searchText) == string::npos &&
                    lowerPath.find(searchText) == string::npos)
                {
                    continue;
                }
            }

            hasSelectableModel = true;
            const bool isSelected = assetMeta.guid == currentGuid;

            if (isSelected)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.85f, 0.3f, 1.f));

            if (ImGui::Selectable(label.c_str(), isSelected))
            {
                json swapData = model->To_Json();
                swapData["model_guid"] = assetMeta.guid;
                swapData["model_type"] = assetMeta.modelType.empty()
                    ? toModelTypeString(model->Get_ModelType())
                    : assetMeta.modelType;

                model->From_Json(swapData);
                ImGui::CloseCurrentPopup();
            }

            if (isSelected)
                ImGui::PopStyleColor();
        }

        if (!hasSelectableModel)
            ImGui::TextDisabled("(No matching .model assets)");

        ImGui::EndPopup();
    }
}

string ModelCom_Inspector::To_DisplayTexturePath(const wstring& resolvedPath)
{
    string fullPath = String::ToString(resolvedPath);

    const size_t modelsPos = fullPath.find("Models\\");
    if (modelsPos != string::npos)
        return fullPath.substr(modelsPos);

    const size_t modelsPosAlt = fullPath.find("Models/");
    if (modelsPosAlt != string::npos)
        return fullPath.substr(modelsPosAlt);

    return fullPath;
}

bool ModelCom_Inspector::Is_TextureExtension(const fs::path& path)
{
    const string ext = String::ToString(path.extension().wstring());
    return ext == ".png" || ext == ".PNG" ||
           ext == ".jpg" || ext == ".JPG" ||
           ext == ".jpeg" || ext == ".JPEG" ||
           ext == ".tga" || ext == ".TGA" ||
           ext == ".dds" || ext == ".DDS" ||
           ext == ".bmp" || ext == ".BMP";
}

vector<fs::path> ModelCom_Inspector::Collect_TextureCandidates(const wstring& resolvedPath, const wstring& sourceModelPath)
{
    namespace fs = fs;

    vector<fs::path> candidates{};

    fs::path directory{};
    if (!resolvedPath.empty())
        directory = fs::path(resolvedPath).parent_path();
    else if (!sourceModelPath.empty())
    {
        const fs::path modelDirectory = fs::path(sourceModelPath).parent_path();
        const fs::path textureDirectory = modelDirectory / L"Texture";
        directory = fs::exists(textureDirectory) ? textureDirectory : modelDirectory;
    }

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

ID3D11ShaderResourceView* ModelCom_Inspector::Get_TexturePreviewSRV(const fs::path& texturePath)
{
    namespace fs = fs;

    const wstring normalizedPath = texturePath.lexically_normal().wstring();

    const auto cached = g_TexturePreviewCache.find(normalizedPath);
    if (cached != g_TexturePreviewCache.end())
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

    g_TexturePreviewCache.emplace(normalizedPath, srv);
    return srv;
}

ModelCom_Inspector::AnimPreviewState& ModelCom_Inspector::Get_AnimPreviewState(const Engine::ModelCom* model)
{
    static umap<const ModelCom*, AnimPreviewState> s_AnimPreviewStates{};
    return s_AnimPreviewStates[model];
}

void ModelCom_Inspector::Draw_AnimationSection(const Shared<Engine::ModelCom>& model)
{
    if (model == nullptr)
        return;

    ImGui::SeparatorText("Animation");

    if (!model->Is_Skeletal())
    {
        ImGui::TextDisabled("Static model does not have animation.");
        return;
    }

    const uint32 numAnimations = static_cast<uint32>(model->Get_NumAnimations());
    const vector<AnimationNameUtils::AssetAnimationInfo> assetInfos =
        AnimationNameUtils::Collect_AssetAnimationInfos(model->Get_SourceModelPath());
    vector<string> playbackNames{};
    playbackNames.reserve(numAnimations);

    for (uint32 i = 0; i < numAnimations; ++i)
    {
        const char* animName = model->Get_AnimationName(i);
        if (!animName)
            continue;

        playbackNames.push_back(animName);
    }

    struct AnimationListItem
    {
        int32 playbackIndex = -1;
        string displayName{};
    };

    vector<AnimationListItem> animationItems{};
    if (!assetInfos.empty())
    {
        vector<bool> usedPlayback(playbackNames.size(), false);
        animationItems.reserve(assetInfos.size() + playbackNames.size());

        for (const auto& assetInfo : assetInfos)
        {
            AnimationListItem item{};
            item.displayName = assetInfo.displayName;

            const int32 matchedPlaybackListIndex =
                AnimationNameUtils::Find_BestPlaybackMatchIndex(assetInfo, playbackNames, usedPlayback);
            if (matchedPlaybackListIndex >= 0)
            {
                item.playbackIndex = matchedPlaybackListIndex;
                usedPlayback[matchedPlaybackListIndex] = true;
            }

            animationItems.push_back(move(item));
        }

        for (int32 i = 0; i < static_cast<int32>(playbackNames.size()); ++i)
        {
            if (i < static_cast<int32>(usedPlayback.size()) && usedPlayback[i])
                continue;

            AnimationListItem item{};
            item.playbackIndex = i;
            item.displayName = AnimationNameUtils::Make_DisplayNameFallback(playbackNames[i]);
            animationItems.push_back(move(item));
        }

    }
    else
    {
        animationItems.reserve(playbackNames.size());
        for (int32 i = 0; i < static_cast<int32>(playbackNames.size()); ++i)
        {
            AnimationListItem item{};
            item.playbackIndex = i;
            item.displayName = AnimationNameUtils::Make_DisplayNameFallback(playbackNames[i]);
            animationItems.push_back(move(item));
        }
    }

    AnimPreviewState& state = Get_AnimPreviewState(model.get());

    if (state.selectedAnimIndex >= static_cast<int32>(numAnimations))
        state.selectedAnimIndex = numAnimations > 0 ? 0 : -1;

    ImGui::TextDisabled("Animation Count");
    ImGui::SameLine(120.f);
    ImGui::Text("%u", numAnimations);

    ImGui::Checkbox("Loop", &state.loop);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.f);
    ImGui::DragFloat("Blend", &state.blendDuration, 0.01f, 0.f, 5.f, "%.2f");

    if (numAnimations == 0)
    {
        ImGui::TextDisabled("No animations found.");
        return;
    }

    const char* selectedAnimName = "None";
    string selectedAnimLabel{};
    if (state.selectedAnimIndex >= 0)
    {
        for (const auto& item : animationItems)
        {
            if (item.playbackIndex != state.selectedAnimIndex)
                continue;

            selectedAnimLabel = item.displayName;
            selectedAnimName = selectedAnimLabel.c_str();
            break;
        }
    }

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("Animation Clip", selectedAnimName))
    {
        for (const auto& item : animationItems)
        {
            if (item.playbackIndex < 0 || item.playbackIndex >= static_cast<int32>(numAnimations))
                continue;

            const char* animName = model->Get_AnimationName(static_cast<uint32>(item.playbackIndex));
            if (!animName)
                continue;

            const bool isSelected = state.selectedAnimIndex == item.playbackIndex;
            const string label = item.displayName + "##" + animName;
            if (ImGui::Selectable(label.c_str(), isSelected))
                state.selectedAnimIndex = item.playbackIndex;

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    if (state.selectedAnimIndex >= 0)
    {
        if (ImGui::Button("Play Selected", ImVec2(140.f, 0.f)))
        {
            if (state.blendDuration > 0.f)
                model->Set_Animation(static_cast<uint32>(state.selectedAnimIndex), state.blendDuration, state.loop);
            else
                model->Set_Animation(static_cast<uint32>(state.selectedAnimIndex), state.loop);
        }

        ImGui::SameLine();

        if (ImGui::Button("Restart", ImVec2(100.f, 0.f)))
            model->Set_Animation(static_cast<uint32>(state.selectedAnimIndex), state.loop);

        ImGui::SameLine();

        if (ImGui::Button("Open Animation View", ImVec2(180.f, 0.f)))
        {
            Shared<Editor_Window> viewBase = EDITOR->Get_Window(L"Animation");
            Shared<Animation_View> animationView = dynamic_pointer_cast<Animation_View>(viewBase);

            if (animationView)
            {
                const char* animationName = model->Get_AnimationName(static_cast<uint32>(state.selectedAnimIndex));
                uint32 preferredDockId = ImGui::GetWindowDockID();

                Shared<Editor_Window> prefabBase = EDITOR->Get_Window(L"Prefab");
                Shared<Prefab_View> prefabView = dynamic_pointer_cast<Prefab_View>(prefabBase);
                if (prefabView != nullptr && prefabView->Is_Open() && prefabView->Get_DockId() != 0)
                    preferredDockId = prefabView->Get_DockId();

                animationView->Open_FromModelComponent(model, animationName ? animationName : "", preferredDockId);
            }
        }
    }

    if (ImGui::BeginChild("ModelAnimList", ImVec2(0.f, 180.f), true, ImGuiWindowFlags_HorizontalScrollbar))
    {
        for (const auto& item : animationItems)
        {
            if (item.playbackIndex < 0 || item.playbackIndex >= static_cast<int32>(numAnimations))
                continue;

            const char* animName = model->Get_AnimationName(static_cast<uint32>(item.playbackIndex));
            if (!animName)
                continue;

            const bool isSelected = state.selectedAnimIndex == item.playbackIndex;
            const string label = item.displayName + "##List" + animName;
            if (ImGui::Selectable(label.c_str(), isSelected))
                state.selectedAnimIndex = item.playbackIndex;

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                state.selectedAnimIndex = item.playbackIndex;

                if (state.blendDuration > 0.f)
                    model->Set_Animation(static_cast<uint32>(item.playbackIndex), state.blendDuration, state.loop);
                else
                    model->Set_Animation(static_cast<uint32>(item.playbackIndex), state.loop);
            }
        }
    }
    ImGui::EndChild();
}

const char* ModelCom_Inspector::Get_TextureTypeLabel(uint32 typeIndex)
{
    switch (static_cast<MaterialTextureSlot>(typeIndex))
    {
    case MaterialTextureSlot::BaseColor: return "BaseColor";
    case MaterialTextureSlot::Normal: return "Normal";
    case MaterialTextureSlot::ORM: return "ORM";
    case MaterialTextureSlot::Emissive: return "Emissive";
    case MaterialTextureSlot::OpacityMask: return "OpacityMask";
    default: return "Unknown";
    }
}

Shared<ModelCom_Inspector> ModelCom_Inspector::Create()
{
    return make_shared<ModelCom_Inspector>();
}
