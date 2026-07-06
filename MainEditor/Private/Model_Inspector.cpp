#include "Model_Inspector.h"

#include "GameInstance.h"
#include "Animation_NameUtils.h"
#include "Animation.h"
#include "ModelCom.h"
#include "Material.h"
#include <filesystem>
#include "EditorInstance.h"
#include "Editor_Context.h"
#include <fstream>
#include "GameObject.h"

#include "ShaderCom.h"

using TexturePreviewCache   = umap<wstring, ComPtr<ID3D11ShaderResourceView>>;
using TextureSearchCache    = umap<string, string>;
using TextureFolderCache    = umap<wstring, vector<fs::path>>;
using TextureMipCache       = umap<wstring, int32>;

static TexturePreviewCache  g_TexturePreviewCache {};
static TextureSearchCache   g_TextureSearchCache  {};
static TextureFolderCache   g_TextureFolderCache  {};
static TextureMipCache      g_TextureMipCache     {};

namespace
{
    string Build_ClippedButtonLabel(const string& visibleText, float buttonWidth, const char* hiddenIdSuffix)
    {
        const float horizontalPadding = ImGui::GetStyle().FramePadding.x * 2.f;
        const float availableTextWidth = max(0.f, buttonWidth - horizontalPadding);
        const float fullTextWidth = ImGui::CalcTextSize(visibleText.c_str()).x;

        if (fullTextWidth <= availableTextWidth)
            return visibleText + hiddenIdSuffix;

        static constexpr char kEllipsis[] = "...";
        const float ellipsisWidth = ImGui::CalcTextSize(kEllipsis).x;

        string clippedText{};
        clippedText.reserve(visibleText.size());

        for (char ch : visibleText)
        {
            const string candidate = clippedText + ch;
            if (ImGui::CalcTextSize(candidate.c_str()).x + ellipsisWidth > availableTextWidth)
                break;

            clippedText.push_back(ch);
        }

        if (clippedText.empty())
            clippedText = kEllipsis;
        else
            clippedText += kEllipsis;

        return clippedText + hiddenIdSuffix;
    }

    bool Matches_AnimationFilter(const string& displayName, const string& playbackName, const string& filterText)
    {
        if (filterText.empty())
            return true;

        const string loweredFilter = String::ToLowerCopy(filterText);
        const string loweredDisplayName = String::ToLowerCopy(displayName);
        const string loweredPlaybackName = String::ToLowerCopy(playbackName);

        return loweredDisplayName.find(loweredFilter) != string::npos ||
            loweredPlaybackName.find(loweredFilter) != string::npos;
    }
}

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

        ImGui::SameLine();
        if (ImGui::Button("Refresh Textures"))
            Clear_TexturePreviewCache();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Re-scan texture candidates and thumbnails on the next draw.");

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

            if (!resolvedPath.empty())
            {
                ImGui::TextDisabled("Mip Count");
                ImGui::SameLine(120.f);

                const int32 mipCount = Get_TextureMipCount(fs::path(resolvedPath), slotSrv);
                if (mipCount > 0)
                {
                    const bool singleMip = mipCount == 1;
                    if (singleMip)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.75f, 0.2f, 1.f));
                        ImGui::Text("%d (single mip)", mipCount);
                        ImGui::PopStyleColor();
                    }
                    else
                    {
                        ImGui::Text("%d", mipCount);
                    }
                }
                else
                {
                    ImGui::TextDisabled("(unknown)");
                }
            }

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

    const float buttonWidth = ImGui::GetContentRegionAvail().x;
    const string buttonLabel = Build_ClippedButtonLabel(displayName, buttonWidth, "##ModelComModelPickerButton");

    if (ImGui::Button(buttonLabel.c_str(), ImVec2(buttonWidth, 0.f)))
        ImGui::OpenPopup("##ModelComModelPicker");

    if (buttonLabel != displayName + "##ModelComModelPickerButton" && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", displayName.c_str());

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
        vector<uint32> filteredIndices{};
        filteredIndices.reserve(modelAssets.size());

        for (uint32 assetIndex = 0; assetIndex < static_cast<uint32>(modelAssets.size()); ++assetIndex)
        {
            const AssetMeta& assetMeta = modelAssets[assetIndex];
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

            filteredIndices.push_back(assetIndex);
        }

        if (filteredIndices.empty())
        {
            ImGui::TextDisabled("(No matching .model assets)");
        }
        else
        {
            const float rowHeight = ImGui::GetTextLineHeightWithSpacing();
            const float popupHeight = rowHeight * 10.f + ImGui::GetStyle().FramePadding.y * 2.f;

            if (ImGui::BeginChild("##ModelComModelPickerList", ImVec2(0.f, popupHeight), true))
            {
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(filteredIndices.size()));

                while (clipper.Step())
                {
                    for (int displayIndex = clipper.DisplayStart; displayIndex < clipper.DisplayEnd; ++displayIndex)
                    {
                        const AssetMeta& assetMeta = modelAssets[filteredIndices[displayIndex]];
                        const string label = Build_ModelPickerLabel(assetMeta);
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
                }

                clipper.End();
            }

            ImGui::EndChild();
        }

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

    const wstring cacheKey = directory.lexically_normal().wstring();
    const auto cached = g_TextureFolderCache.find(cacheKey);
    if (cached != g_TextureFolderCache.end())
        return cached->second;

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

    g_TextureFolderCache.emplace(cacheKey, candidates);
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

namespace
{
    constexpr uint32 kDdsMagic = 0x20534444; // "DDS "
    constexpr uint32 kDdsHeaderSize = 124;

    struct DdsHeaderRaw
    {
        uint32 size = 0;
        uint32 flags = 0;
        uint32 height = 0;
        uint32 width = 0;
        uint32 pitchOrLinearSize = 0;
        uint32 depth = 0;
        uint32 mipMapCount = 0;
        uint32 reserved1[11]{};
        uint32 ddspf[8]{};
        uint32 caps = 0;
        uint32 caps2 = 0;
        uint32 caps3 = 0;
        uint32 caps4 = 0;
        uint32 reserved2 = 0;
    };
}

int32 ModelCom_Inspector::Get_TextureMipCount(const fs::path& texturePath, ID3D11ShaderResourceView* shaderResourceView)
{
    namespace fs = fs;

    const wstring normalizedPath = texturePath.lexically_normal().wstring();
    const auto cached = g_TextureMipCache.find(normalizedPath);
    if (cached != g_TextureMipCache.end())
        return cached->second;

    int32 mipCount = 0;

    if (shaderResourceView != nullptr)
    {
        ComPtr<ID3D11Resource> resource;
        shaderResourceView->GetResource(resource.GetAddressOf());
        if (resource != nullptr)
        {
            ComPtr<ID3D11Texture2D> texture2D;
            if (SUCCEEDED(resource.As(&texture2D)) && texture2D != nullptr)
            {
                D3D11_TEXTURE2D_DESC desc{};
                texture2D->GetDesc(&desc);
                mipCount = static_cast<int32>(desc.MipLevels);
            }
        }
    }

    if (mipCount <= 0 &&
        fs::exists(texturePath) &&
        String::ToLowerCopy(String::ToString(texturePath.extension().wstring())) == ".dds")
    {
        ifstream stream(texturePath, ios::binary);
        if (stream.is_open())
        {
            uint32 magic = 0;
            DdsHeaderRaw header{};
            stream.read(reinterpret_cast<char*>(&magic), sizeof(uint32));
            stream.read(reinterpret_cast<char*>(&header), sizeof(DdsHeaderRaw));

            if (stream.good() &&
                magic == kDdsMagic &&
                header.size == kDdsHeaderSize)
            {
                mipCount = static_cast<int32>(header.mipMapCount == 0 ? 1 : header.mipMapCount);
            }
        }
    }

    g_TextureMipCache.emplace(normalizedPath, mipCount);
    return mipCount;
}

void ModelCom_Inspector::Clear_TexturePreviewCache()
{
    g_TexturePreviewCache.clear();
    g_TextureSearchCache.clear();
    g_TextureFolderCache.clear();
    g_TextureMipCache.clear();
}

ModelCom_Inspector::AnimPreviewState& ModelCom_Inspector::Get_AnimPreviewState(const Engine::ModelCom* model)
{
    static umap<const ModelCom*, AnimPreviewState> s_AnimPreviewStates{};
    return s_AnimPreviewStates[model];
}

ModelCom_Inspector::AnimListCache& ModelCom_Inspector::Get_AnimListCache(const Engine::ModelCom* model)
{
    static umap<const ModelCom*, AnimListCache> s_AnimListCaches{};
    return s_AnimListCaches[model];
}

void ModelCom_Inspector::Refresh_AnimListCache(const Shared<Engine::ModelCom>& model, AnimListCache& cache, uint32 numAnimations)
{
    cache.items.clear();

    if (model == nullptr)
        return;

    cache.sourceModelPath = model->Get_SourceModelPath();
    cache.numAnimations = numAnimations;

    vector<string> playbackNames{};
    playbackNames.reserve(numAnimations);

    for (uint32 i = 0; i < numAnimations; ++i)
    {
        const char* animName = model->Get_AnimationName(i);
        if (!animName)
            continue;

        playbackNames.push_back(animName);
    }

    const vector<AnimationNameUtils::AssetAnimationInfo> assetInfos =
        AnimationNameUtils::Collect_AssetAnimationInfos(cache.sourceModelPath);

    if (!assetInfos.empty())
    {
        vector<bool> usedPlayback(playbackNames.size(), false);
        cache.items.reserve(assetInfos.size() + playbackNames.size());

        for (const auto& assetInfo : assetInfos)
        {
            AnimationListItem item{};
            item.displayName = assetInfo.displayName;

            const int32 matchedPlaybackListIndex =
                AnimationNameUtils::Find_BestPlaybackMatchIndex(assetInfo, playbackNames, usedPlayback);
            if (matchedPlaybackListIndex >= 0)
            {
                item.playbackIndex = matchedPlaybackListIndex;
                item.playbackName = playbackNames[matchedPlaybackListIndex];
                usedPlayback[matchedPlaybackListIndex] = true;
            }

            cache.items.push_back(move(item));
        }

        for (int32 i = 0; i < static_cast<int32>(playbackNames.size()); ++i)
        {
            if (i < static_cast<int32>(usedPlayback.size()) && usedPlayback[i])
                continue;

            AnimationListItem item{};
            item.playbackIndex = i;
            item.displayName = AnimationNameUtils::Make_DisplayNameFallback(playbackNames[i]);
            item.playbackName = playbackNames[i];
            cache.items.push_back(move(item));
        }

        return;
    }

    cache.items.reserve(playbackNames.size());
    for (int32 i = 0; i < static_cast<int32>(playbackNames.size()); ++i)
    {
        AnimationListItem item{};
        item.playbackIndex = i;
        item.displayName = AnimationNameUtils::Make_DisplayNameFallback(playbackNames[i]);
        item.playbackName = playbackNames[i];
        cache.items.push_back(move(item));
    }
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
    AnimListCache& animListCache = Get_AnimListCache(model.get());
    if (animListCache.sourceModelPath != model->Get_SourceModelPath() || animListCache.numAnimations != numAnimations)
        Refresh_AnimListCache(model, animListCache, numAnimations);

    const vector<AnimationListItem>& animationItems = animListCache.items;

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

    char filterBuffer[256]{};
    strncpy_s(filterBuffer, state.filterText.c_str(), _TRUNCATE);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##ModelAnimFilter", "Search animation...", filterBuffer, IM_ARRAYSIZE(filterBuffer)))
        state.filterText = filterBuffer;

    if (numAnimations == 0)
    {
        ImGui::TextDisabled("No animations found.");
        return;
    }

    vector<int32> filteredAnimationIndices{};
    filteredAnimationIndices.reserve(animationItems.size());
    for (int32 itemIndex = 0; itemIndex < static_cast<int32>(animationItems.size()); ++itemIndex)
    {
        const AnimationListItem& item = animationItems[itemIndex];
        if (item.playbackIndex < 0 || item.playbackIndex >= static_cast<int32>(numAnimations))
            continue;
        if (!Matches_AnimationFilter(item.displayName, item.playbackName, state.filterText))
            continue;

        filteredAnimationIndices.push_back(itemIndex);
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
        for (int32 filteredIndex : filteredAnimationIndices)
        {
            const AnimationListItem& item = animationItems[filteredIndex];

            const bool isSelected = state.selectedAnimIndex == item.playbackIndex;
            const string label = item.displayName + "##" + item.playbackName;
            if (ImGui::Selectable(label.c_str(), isSelected))
                state.selectedAnimIndex = item.playbackIndex;

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    if (state.selectedAnimIndex >= 0)
    {
        const Animation* selectedAnimation = model->Get_Animation(static_cast<uint32>(state.selectedAnimIndex));
        const float selectedFrameMax = selectedAnimation != nullptr ? selectedAnimation->Get_Duration() : 0.f;
        state.previewFrame = ::clamp(state.previewFrame, 0.f, selectedFrameMax);

        ImGui::SetNextItemWidth(120.f);
        ImGui::DragFloat("Frame", &state.previewFrame, 1.f, 0.f, selectedFrameMax, "%.0f");
        if (ImGui::IsItemDeactivatedAfterEdit())
            state.previewFrame = ::clamp(state.previewFrame, 0.f, selectedFrameMax);

        ImGui::SameLine();
        ImGui::TextDisabled("/ %.0f", selectedFrameMax);

        if (ImGui::Button("Play Selected", ImVec2(140.f, 0.f)))
        {
            if (state.blendDuration > 0.f)
                model->Set_Animation(static_cast<uint32>(state.selectedAnimIndex), state.blendDuration, state.loop, state.previewFrame);
            else
                model->Set_Animation(static_cast<uint32>(state.selectedAnimIndex), state.loop, state.previewFrame);

            model->Set_AnimationPaused(false); // Stop으로 멈춘 뒤 다시 눌렀을 때 즉시 재생되게 한다.
        }

        ImGui::SameLine();

        if (ImGui::Button("Restart", ImVec2(100.f, 0.f)))
        {
            state.previewFrame = 0.f;
            model->Set_Animation(static_cast<uint32>(state.selectedAnimIndex), state.loop);
            model->Set_AnimationPaused(false); // 처음 프레임부터 다시 재생한다.
        }

        ImGui::SameLine();

        if (ImGui::Button("Set Frame", ImVec2(100.f, 0.f)))
        {
            model->Set_Animation(static_cast<uint32>(state.selectedAnimIndex), state.loop, state.previewFrame);
            model->Set_AnimationPaused(true); // 지정한 프레임 포즈를 유지해서 긴 애니메이션을 바로 확인한다.
            model->Refresh_CurrentAnimationPose();
        }

        ImGui::SameLine();

        if (ImGui::Button("Stop", ImVec2(100.f, 0.f)))
        {
            model->Set_AnimationPaused(true); // 현재 포즈를 그대로 멈춰서 무기 위치를 맞출 수 있게 한다.
            model->Refresh_CurrentAnimationPose();
        }
    }

    if (ImGui::BeginChild("ModelAnimList", ImVec2(0.f, 180.f), true, ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(filteredAnimationIndices.size()));

        while (clipper.Step())
        {
            for (int displayIndex = clipper.DisplayStart; displayIndex < clipper.DisplayEnd; ++displayIndex)
            {
                const AnimationListItem& item = animationItems[filteredAnimationIndices[displayIndex]];

                const bool isSelected = state.selectedAnimIndex == item.playbackIndex;
                const string label = item.displayName + "##List" + item.playbackName;
                if (ImGui::Selectable(label.c_str(), isSelected))
                    state.selectedAnimIndex = item.playbackIndex;

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    state.selectedAnimIndex = item.playbackIndex;

                    if (state.blendDuration > 0.f)
                        model->Set_Animation(static_cast<uint32>(item.playbackIndex), state.blendDuration, state.loop, state.previewFrame);
                    else
                        model->Set_Animation(static_cast<uint32>(item.playbackIndex), state.loop, state.previewFrame);
                }
            }
        }

        clipper.End();
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
    case MaterialTextureSlot::HairDepth: return "HairDepth";
    case MaterialTextureSlot::HairID: return "HairID";
    default: return "Unknown";
    }
}

Shared<ModelCom_Inspector> ModelCom_Inspector::Create()
{
    return make_shared<ModelCom_Inspector>();
}
