#include "Model_Inspector.h"

#include "GameInstance.h"
#include "ModelCom.h"
#include "Material.h"
#include <filesystem>
#include <unordered_map>

namespace
{
    namespace fs = std::filesystem;
    using TexturePreviewCache = std::unordered_map<wstring, ComPtr<ID3D11ShaderResourceView>>;
    using TextureSearchCache = std::unordered_map<string, string>;

    TexturePreviewCache g_TexturePreviewCache{};
    TextureSearchCache g_TextureSearchCache{};

    string To_DisplayTexturePath(const wstring& resolvedPath)
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
}

void ModelCom_Inspector::Draw_Inspector(const Shared<Component>& component)
{
    Shared<ModelCom> model = dynamic_pointer_cast<ModelCom>(component);
    if (model == nullptr)
        return;

    if (!Draw_Header("Model"))
        return;

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

    // Top / Shoulder / Coat 같은 파츠별 mesh -> material 매핑 표시
    const uint32 numMeshes = static_cast<uint32>(model->Get_NumMeshes());
    for (uint32 meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
    {
        const uint32 materialIndex = model->Get_MeshMaterialIndex(meshIndex);
        Shared<Material> material = model->Get_Material(materialIndex);

        string label = "Mesh[" + to_string(meshIndex) + "]";
        if (material)
            label += " -> Material[" + to_string(materialIndex) + "] : " + material->Get_Name();
        else
            label += " -> Material[Invalid]";

        ImGui::BulletText("%s", label.c_str());
    }

    ImGui::SeparatorText("Materials");

    // 각 머티리얼 내부 텍스처 슬롯 표시
    const uint32 numMaterials = model->Get_NumMaterials();
    for (uint32 materialIndex = 0; materialIndex < numMaterials; ++materialIndex)
    {
        Shared<Material> material = model->Get_Material(materialIndex);
        if (material == nullptr)
            continue;

        ImGui::PushID(static_cast<int>(materialIndex));

        string header = "Material[" + to_string(materialIndex) + "] : " + material->Get_Name();
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
                    ImGui::Image(
                        reinterpret_cast<ImTextureID>(slot.shaderResourceView.Get()),
                        ImVec2(64.f, 64.f));
                }

                const vector<fs::path> textureCandidates = Collect_TextureCandidates(slot.resolvedPath);
                if (!textureCandidates.empty())
                {
                    ImGui::TextDisabled("Texture");
                    ImGui::SameLine(120.f);

                    if (slot.shaderResourceView)
                    {
                        ImGui::Image(
                            reinterpret_cast<ImTextureID>(slot.shaderResourceView.Get()),
                            ImVec2(32.f, 32.f));
                        ImGui::SameLine();
                    }

                    const string currentFileName = String::ToString(fs::path(slot.resolvedPath).filename().wstring());
                    const string pickerKey = material->Get_Name() + "_" + to_string(typeIndex) + "_" + to_string(slotIndex);
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
