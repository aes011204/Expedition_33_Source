#include "Model_Inspector.h"

#include "GameInstance.h"
#include "ModelCom.h"
#include "Material.h"
#include <filesystem>
#include "EditorInstance.h"
#include "Animation_View.h"
#include "Editor_Window.h"

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

    const uint32 numMaterials = model->Get_NumMaterials();
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

        const vector<vector<Material::TextureSlot>>& allSlots = material->Get_TextureSlots();

        for (uint32 typeIndex = 0; typeIndex < allSlots.size(); ++typeIndex)
        {
            const vector<Material::TextureSlot>& slots = allSlots[typeIndex];
            if (slots.empty())
                continue;

            ImGui::PushID(static_cast<int>(typeIndex));

            string slotHeader = string(Get_TextureTypeLabel(typeIndex)) + " [" + to_string(typeIndex) + "] (" + to_string(slots.size()) + ")";
            if (!ImGui::TreeNode(slotHeader.c_str()))
            {
                ImGui::PopID();
                continue;
            }

            for (size_t slotIndex = 0; slotIndex < slots.size(); ++slotIndex)
            {
                const Material::TextureSlot& slot = slots[slotIndex];

                ImGui::PushID(static_cast<int>(slotIndex));

                ImGui::TextDisabled("TextureIndex");
                ImGui::SameLine(120.f);
                ImGui::Text("%u", slot.textureIndex);

                ImGui::TextDisabled("Loaded");
                ImGui::SameLine(120.f);
                ImGui::TextUnformatted(slot.shaderResourceView ? "Yes" : "No");

                ImGui::TextDisabled("Path");
                ImGui::SameLine(120.f);
                ImGui::BeginDisabled();
                const string displayPath = To_DisplayTexturePath(slot.resolvedPath);
                ImGui::TextWrapped("%s", displayPath.c_str());
                ImGui::EndDisabled();

                if (slot.shaderResourceView)
                {
                    ImGui::TextDisabled("Preview");
                    ImGui::SameLine(120.f);
                    ImGui::Image(reinterpret_cast<ImTextureID>(slot.shaderResourceView.Get()), ImVec2(64.f, 64.f));
                }

                const vector<fs::path> textureCandidates = Collect_TextureCandidates(slot.resolvedPath);
                if (!textureCandidates.empty())
                {
                    ImGui::TextDisabled("Texture");
                    ImGui::SameLine(120.f);

                    if (slot.shaderResourceView)
                    {
                        ImGui::Image(reinterpret_cast<ImTextureID>(slot.shaderResourceView.Get()), ImVec2(32.f, 32.f));
                        ImGui::SameLine();
                    }

                    const string currentFileName = String::ToString(fs::path(slot.resolvedPath).filename().wstring());
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
                            const bool isSelected = candidatePath.wstring() == fs::path(slot.resolvedPath).lexically_normal().wstring();

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
                                material->Override_TextureSlot(static_cast<MaterialTextureSlot>(typeIndex), static_cast<uint32>(slotIndex), candidatePath.wstring());

                            if (isSelected)
                                ImGui::SetItemDefaultFocus();

                            ImGui::PopID();
                        }

                        ImGui::EndCombo();
                    }
                }

                ImGui::Separator();
                ImGui::PopID();
            }

            ImGui::TreePop();
            ImGui::PopID();
        }

        ImGui::PopID();
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

vector<fs::path> ModelCom_Inspector::Collect_TextureCandidates(const wstring& resolvedPath)
{
    namespace fs = fs;

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
    if (state.selectedAnimIndex >= 0)
    {
        if (const char* animName = model->Get_AnimationName(static_cast<uint32>(state.selectedAnimIndex)))
            selectedAnimName = animName;
    }

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("Animation Clip", selectedAnimName))
    {
        for (uint32 i = 0; i < numAnimations; ++i)
        {
            const char* animName = model->Get_AnimationName(i);
            if (!animName)
                continue;

            const bool isSelected = state.selectedAnimIndex == static_cast<int32>(i);
            if (ImGui::Selectable(animName, isSelected))
                state.selectedAnimIndex = static_cast<int32>(i);

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
                animationView->Open_FromModelComponent(model, animationName ? animationName : "", ImGui::GetWindowDockID());
            }
        }
    }

    if (ImGui::BeginChild("ModelAnimList", ImVec2(0.f, 180.f), true, ImGuiWindowFlags_HorizontalScrollbar))
    {
        for (uint32 i = 0; i < numAnimations; ++i)
        {
            const char* animName = model->Get_AnimationName(i);
            if (!animName)
                continue;

            const bool isSelected = state.selectedAnimIndex == static_cast<int32>(i);
            if (ImGui::Selectable(animName, isSelected))
                state.selectedAnimIndex = static_cast<int32>(i);

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                state.selectedAnimIndex = static_cast<int32>(i);

                if (state.blendDuration > 0.f)
                    model->Set_Animation(i, state.blendDuration, state.loop);
                else
                    model->Set_Animation(i, state.loop);
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
