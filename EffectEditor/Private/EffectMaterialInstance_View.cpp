#include "EffectMaterialInstance_View.h"

#include <fstream>
#include <shobjidl.h>

#include "EffectEditorInstance.h"
#include "EffectMaterialPreviewRenderer.h"
#include "Emitter_View.h"
#include "GameInstance.h"
#include "Helper_DetailProperty.h"
#include "Helper_String.h"
#include "Texture.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    constexpr const char* kEffectTexturePayloadType = "CONTENT_BROWSER_EFFECT_TEXTURE";
    constexpr const wchar_t* kTextureTablePath = L"../../../Client/Bin/Resources/Data/json/DT_Texture.json";
    constexpr const wchar_t* kTextureDialogDefaultFolder = L"../../../Client/Bin/Resources/Effects/Textures";

    string textureAssignStatus{};

    string ToLowerCopy(string value)
    {
        ranges::transform(value, value.begin(), ::tolower);
        return value;
    }

    wstring ToLowerCopy(wstring value)
    {
        ranges::transform(
            value,
            value.begin(),
            [](wchar_t character)
            {
                return static_cast<wchar_t>(::towlower(character));
            }
        );
        return value;
    }

    bool Is_SupportedTextureFile(const fs::path& filePath)
    {
        const string extension = ToLowerCopy(filePath.extension().string());
        return extension == ".png" || extension == ".dds";
    }

    fs::path Normalize_Path(const fs::path& filePath)
    {
        try
        {
            if (fs::exists(filePath))
                return fs::weakly_canonical(filePath);
        }
        catch (...)
        {
        }

        try
        {
            return fs::absolute(filePath).lexically_normal();
        }
        catch (...)
        {
        }

        return filePath.lexically_normal();
    }

    fs::path Resolve_EditorPath(const wchar_t* path)
    {
        return Normalize_Path(fs::path(path));
    }

    fs::path Resolve_TextureTablePath(const string& tablePathText)
    {
        string normalizedText = tablePathText;
        ranges::replace(normalizedText, '\\', '/');

        const string marker = "Bin/Resources/";
        const size_t markerPos = normalizedText.find(marker);
        if (markerPos != string::npos)
        {
            const string resourceRelative = normalizedText.substr(markerPos + marker.size());
            fs::path resourceRoot = GAME ? fs::path(GAME->Get_AssetRoot()) : fs::path{};
            if (resourceRoot.empty())
                resourceRoot = fs::path(L"../../../Client/Bin/Resources");
            return Normalize_Path(resourceRoot / String::ToWString(resourceRelative));
        }

        return Normalize_Path(String::ToWString(tablePathText));
    }

    bool Try_ReadTextureIdFromTable(const fs::path& filePath, string& outTextureId, string& outError)
    {
        outTextureId.clear();
        outError.clear();

        if (!Is_SupportedTextureFile(filePath))
        {
            outError = "Unsupported texture suffix. Use .png or .dds.";
            return false;
        }

        if (!fs::exists(filePath))
        {
            outError = "Texture file not found.";
            return false;
        }

        const fs::path textureTablePath = Resolve_EditorPath(kTextureTablePath);
        if (textureTablePath.empty())
        {
            outError = "Failed to resolve DT_Texture.json path.";
            return false;
        }

        ifstream file{ textureTablePath };
        if (!file.is_open())
        {
            outError = "Failed to open DT_Texture.json.";
            return false;
        }

        json root{};
        try
        {
            file >> root;
        }
        catch (...)
        {
            outError = "Failed to parse DT_Texture.json.";
            return false;
        }

        const json* textureArray = nullptr;
        if (root.contains("DT_Texture"))
            textureArray = &root["DT_Texture"];
        else if (root.contains("Texture"))
            textureArray = &root["Texture"];

        if (textureArray == nullptr || !textureArray->is_array())
        {
            outError = "DT_Texture.json has no texture array.";
            return false;
        }

        const wstring selectedPath = ToLowerCopy(Normalize_Path(filePath).wstring());
        for (const json& item : *textureArray)
        {
            const string id = item.value("Id", item.value("id", string{}));
            const string path = item.value("Path", item.value("path", string{}));
            if (id.empty() || path.empty() || path.find('%') != string::npos)
                continue;

            const wstring tablePath = ToLowerCopy(Resolve_TextureTablePath(path).wstring());
            if (tablePath != selectedPath)
                continue;

            outTextureId = id;
            return true;
        }

        outError = "Texture file exists but has no matching DT_Texture.json row.";
        return false;
    }

    bool Try_AssignTextureFromFile(const fs::path& filePath, string& value)
    {
        string textureId{};
        string error{};
        if (!Try_ReadTextureIdFromTable(filePath, textureId, error))
        {
            textureAssignStatus = error;
            return false;
        }

        value = textureId;
        textureAssignStatus = "Texture assigned: " + textureId;
        return true;
    }

    bool Try_PickTextureFileDialog(wstring& outFilePath)
    {
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

        dialog->SetTitle(L"Select Effect Texture");

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST);

        const fs::path defaultFolder = Resolve_EditorPath(kTextureDialogDefaultFolder);
        if (!defaultFolder.empty())
        {
            IShellItem* folderItem = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(defaultFolder.wstring().c_str(), nullptr, IID_PPV_ARGS(&folderItem))) && folderItem != nullptr)
            {
                dialog->SetFolder(folderItem);
                folderItem->Release();
            }
        }

        const COMDLG_FILTERSPEC filters[] =
        {
            { L"Effect Textures", L"*.png;*.dds" },
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

    Shared<Texture> Resolve_TextureThumbnail(const string& textureId)
    {
        if (textureId.empty() || GAME == nullptr)
            return nullptr;

        static map<string, Shared<Texture>> textureCache{};
        const auto iter = textureCache.find(textureId);
        if (iter != textureCache.end())
            return iter->second;

        Shared<Texture> texture = dynamic_pointer_cast<Texture>(
            GAME->Clone_Prototype(Prototype::Comopnent, ETOI(LevelType::GamePlay), String::ToWString(textureId))
        );
        textureCache[textureId] = texture;
        return texture;
    }

    ID3D11ShaderResourceView* Get_TextureThumbnailSRV(const string& textureId)
    {
        const Shared<Texture> texture = Resolve_TextureThumbnail(textureId);
        if (texture == nullptr || texture->Get_SRVs().empty())
            return nullptr;

        return texture->Get_SRVs().front().Get();
    }

    void Draw_TextureThumbnail(const string& textureId, const ImVec2& size)
    {
        ID3D11ShaderResourceView* srv = Get_TextureThumbnailSRV(textureId);
        if (srv != nullptr)
        {
            const ImTextureID texture = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(srv));
            ImGui::Image(texture, size);
            return;
        }

        const ImVec2 minPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##TextureThumbnailFallback", size);
        const ImVec2 maxPos{ minPos.x + size.x, minPos.y + size.y };
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(minPos, maxPos, IM_COL32(18, 20, 24, 255), 4.f);
        drawList->AddRect(minPos, maxPos, IM_COL32(85, 96, 116, 255), 4.f);
        drawList->AddText(
            ImVec2(minPos.x + 7.f, minPos.y + size.y * 0.5f - 7.f),
            IM_COL32(150, 160, 176, 255),
            textureId.empty() ? "NONE" : "TEX"
        );
    }

    bool Draw_TextureSlotBody(string& value)
    {
        constexpr float browseButtonWidth{ 28.f };
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float slotWidth = max(96.f, ImGui::GetContentRegionAvail().x - browseButtonWidth - spacing);
        const ImVec2 slotSize{ slotWidth, 44.f };

        const ImGuiPayload* activePayload = ImGui::GetDragDropPayload();
        const bool isTextureDragging =
            activePayload != nullptr &&
            activePayload->IsDataType(kEffectTexturePayloadType);

        ImGui::InvisibleButton("##TextureDropSlot", slotSize);
        bool changed = false;
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                kEffectTexturePayloadType,
                ImGuiDragDropFlags_AcceptBeforeDelivery
            ))
            {
                if (payload->IsDelivery())
                    changed = Try_AssignTextureFromFile(static_cast<const wchar_t*>(payload->Data), value);
            }
            ImGui::EndDragDropTarget();
        }

        const ImVec2 slotMin = ImGui::GetItemRectMin();
        const ImVec2 slotMax = ImGui::GetItemRectMax();
        const bool isHovered = ImGui::IsItemHovered();
        const ImU32 fillColor = isHovered || isTextureDragging
                                ? IM_COL32(34, 42, 58, 255)
                                : IM_COL32(22, 25, 32, 255);
        const ImU32 borderColor = isHovered || isTextureDragging
                                  ? IM_COL32(116, 156, 235, 255)
                                  : IM_COL32(78, 88, 108, 255);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(slotMin, slotMax, fillColor, 4.f);
        drawList->AddRect(slotMin, slotMax, borderColor, 4.f, 0, isTextureDragging ? 2.f : 1.f);

        const string displayText = value.empty() ? "(none)" : value;
        drawList->PushClipRect(ImVec2(slotMin.x + 8.f, slotMin.y), ImVec2(slotMax.x - 8.f, slotMax.y), true);
        drawList->AddText(
            ImVec2(slotMin.x + 10.f, slotMin.y + slotSize.y * 0.5f - ImGui::GetTextLineHeight() * 0.5f),
            IM_COL32(220, 226, 238, 255),
            displayText.c_str()
        );
        drawList->PopClipRect();

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("Content Browser의 .png / .dds를 드롭하면 현재 instance texture id로 지정합니다.");

        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2{ 0.5f, 0.5f });
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 0.f, 0.f });
        if (ImGui::Button(ICON_FA_FOLDER_OPEN "##PickTexture", ImVec2{ browseButtonWidth, slotSize.y }))
        {
            wstring pickedFilePath{};
            if (Try_PickTextureFileDialog(pickedFilePath))
                changed = Try_AssignTextureFromFile(pickedFilePath, value);
        }
        ImGui::PopStyleVar(2);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("텍스처 파일 선택");

        return changed;
    }

    bool Draw_TextureProperty(const char* label, string& value, const string& defaultValue)
    {
        ImGui::PushID(label);
        DetailProperty::Draw_PropertyLabel(label);

        constexpr ImVec2 thumbnailSize{ 44.f, 44.f };
        Draw_TextureThumbnail(value, thumbnailSize);
        ImGui::SameLine();
        bool changed = Draw_TextureSlotBody(value);

        const bool resetClicked = DetailProperty::Draw_ResetButton(value != defaultValue);
        if (resetClicked)
        {
            value = defaultValue;
            textureAssignStatus = value.empty() ? "Texture reset: (none)" : "Texture reset: " + value;
            changed = true;
        }

        ImGui::PopID();
        return changed;
    }

    bool Equals_IgnoreCase(const string& lhs, const char* rhs)
    {
        return ToLowerCopy(lhs) == ToLowerCopy(rhs);
    }

    bool Draw_StringComboProperty(const char* label, string& value, const string& defaultValue, const char* const* options, size_t optionCount)
    {
        ImGui::PushID(label);
        DetailProperty::Draw_PropertyLabel(label);

        const char* preview = value.empty() ? "(none)" : value.c_str();
        for (size_t i = 0; i < optionCount; ++i)
        {
            if (Equals_IgnoreCase(value, options[i]))
            {
                preview = options[i];
                break;
            }
        }

        bool changed = false;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", preview))
        {
            for (size_t i = 0; i < optionCount; ++i)
            {
                const bool selected = Equals_IgnoreCase(value, options[i]);
                if (ImGui::Selectable(options[i], selected))
                {
                    value = options[i];
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        const bool resetClicked = DetailProperty::Draw_ResetButton(value != defaultValue);
        if (resetClicked)
        {
            value = defaultValue;
            changed = true;
        }

        ImGui::PopID();
        return changed;
    }
}

EffectMaterialInstance_View::EffectMaterialInstance_View()
    : Editor_Window{ L"Effect Material Instance", ICON_FA_PALETTE }
    , _previewRenderer{ make_unique<EffectMaterialPreviewRenderer>() }
{
}

EffectMaterialInstance_View::~EffectMaterialInstance_View() = default;

void EffectMaterialInstance_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string& windowName = Get_ImGuiWindowName();

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        Shared<Emitter_View> emitterView{};
        AuthoringEmitter* emitter{ nullptr };
        RequiredModuleData* requiredData{ nullptr };
        if (!Resolve_Target(emitterView, emitter, requiredData))
        {
            Commit_PendingAuthoringEdit(emitterView);
            ImGui::TextDisabled("Material instance target is not selected.");
        }
        else
        {
            ImGui::TextUnformatted("Embedded Effect Material Instance (Material Copy)");
            ImGui::TextDisabled("Source material GUID is not stored in this instance.");
            ImGui::Separator();

            bool changed = false;
            const float contentWidth = ImGui::GetContentRegionAvail().x;
            const bool useSideBySideLayout = contentWidth >= 760.f;
            const EffectAuthoringSelection selection{
                EffectAuthoringSelectionKind::Module,
                _targetEmitterId,
                _targetModuleId
            };
            const Emitter_View::AuthoringSnapshot beforeSnapshot = emitterView->Capture_AuthoringSnapshot();

            if (useSideBySideLayout)
            {
                const float spacing = ImGui::GetStyle().ItemSpacing.x;
                const float previewPaneWidth = clamp(contentWidth * 0.42f, 360.f, 560.f);

                ImGui::BeginChild("EffectMaterialInstancePreviewPane", ImVec2(previewPaneWidth, 0.f), false);
                {
                    if (_previewRenderer != nullptr)
                    {
                        _previewRenderer->Draw_Viewport(
                            "EffectMaterialInstancePreview",
                            requiredData->material,
                            "현재 인스턴스 프리뷰"
                        );
                    }
                }
                ImGui::EndChild();

                ImGui::SameLine(0.f, spacing);

                ImGui::BeginChild("EffectMaterialInstancePropertyPane", ImVec2(0.f, 0.f), false);
                {
                    changed |= Draw_InstanceProperties(requiredData->material);
                }
                ImGui::EndChild();
            }
            else
            {
                if (_previewRenderer != nullptr)
                {
                    _previewRenderer->Draw_Viewport(
                        "EffectMaterialInstancePreview",
                        requiredData->material,
                        "현재 인스턴스 프리뷰"
                    );
                    ImGui::Separator();
                }

                changed |= Draw_InstanceProperties(requiredData->material);
            }

            if (changed)
            {
                Apply_MaterialChanged(emitterView, *emitter, *requiredData);
                Begin_PendingAuthoringEdit(emitterView, selection, beforeSnapshot, "Edit Material Instance");
            }

            Commit_PendingAuthoringEditIfIdle(emitterView);
        }
    }

    Set_Open(isOpen);
    ImGui::End();
}

void EffectMaterialInstance_View::Pre_Render()
{
    if (!Is_Open() || _previewRenderer == nullptr)
        return;

    Shared<Emitter_View> emitterView{};
    AuthoringEmitter* emitter{ nullptr };
    RequiredModuleData* requiredData{ nullptr };
    if (!Resolve_Target(emitterView, emitter, requiredData))
        return;

    _previewRenderer->Render_Material(requiredData->material);
}

void EffectMaterialInstance_View::Open_Instance(uint32 emitterId, uint32 moduleId)
{
    _targetEmitterId = emitterId;
    _targetModuleId = moduleId;
    Set_Open(true);
}

bool EffectMaterialInstance_View::Resolve_Target(
    Shared<Emitter_View>& outEmitterView,
    AuthoringEmitter*& outEmitter,
    RequiredModuleData*& outRequiredData) const
{
    outEmitterView.reset();
    outEmitter = nullptr;
    outRequiredData = nullptr;

    if (EDITOR == nullptr)
        return false;

    const Shared<Editor_Window> emitterWindow = EDITOR->Get_Window(L"Emitter");
    outEmitterView = dynamic_pointer_cast<Emitter_View>(emitterWindow);
    if (outEmitterView == nullptr)
        return false;

    outEmitter = outEmitterView->Find_Emitter(_targetEmitterId);
    if (outEmitter == nullptr)
        return false;

    AuthoringModule* module = outEmitterView->Find_Module(_targetEmitterId, _targetModuleId);
    if (module == nullptr || module->type != AuthoringModuleType::Required)
        return false;

    outRequiredData = get_if<RequiredModuleData>(&module->data);
    return outRequiredData != nullptr;
}

bool EffectMaterialInstance_View::Draw_InstanceProperties(EffectMaterialInstanceData& material)
{
    const EffectMaterialInstanceData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("Texture", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (DetailProperty::Begin_PropertyTable("EffectMaterialInstanceTexture"))
        {
            changed |= Internal::Draw_TextureProperty("Main Texture", material.mainTextureId, defaultData.mainTextureId);
            changed |= Internal::Draw_TextureProperty("Noise Texture", material.noiseTextureId, defaultData.noiseTextureId);
            DetailProperty::End_PropertyTable();
        }

        if (!Internal::textureAssignStatus.empty())
            ImGui::TextDisabled("%s", Internal::textureAssignStatus.c_str());
    }

    if (ImGui::CollapsingHeader("Color / Scalar", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (DetailProperty::Begin_PropertyTable("EffectMaterialInstanceColorScalar"))
        {
            changed |= DetailProperty::Draw_ColorProperty("Tint", material.tint, defaultData.tint);
            changed |= DetailProperty::Draw_FloatProperty("Intensity", material.intensity, defaultData.intensity);
            changed |= DetailProperty::Draw_FloatProperty("Opacity Power", material.opacityPower, defaultData.opacityPower);
            changed |= DetailProperty::Draw_FloatProperty("Noise Strength", material.noiseStrength, defaultData.noiseStrength);
            DetailProperty::End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("Options", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (DetailProperty::Begin_PropertyTable("EffectMaterialInstanceOptions"))
        {
            constexpr const char* blendModeOptions[]{ "AlphaBlend", "Additive" };
            constexpr const char* opacitySourceOptions[]{ "Alpha", "Red", "Luminance" };

            changed |= Internal::Draw_StringComboProperty(
                "Blend Mode",
                material.blendMode,
                defaultData.blendMode,
                blendModeOptions,
                sizeof(blendModeOptions) / sizeof(blendModeOptions[0])
            );
            changed |= Internal::Draw_StringComboProperty(
                "Opacity Source",
                material.opacitySource,
                defaultData.opacitySource,
                opacitySourceOptions,
                sizeof(opacitySourceOptions) / sizeof(opacitySourceOptions[0])
            );
            changed |= DetailProperty::Draw_UintProperty("SubUV Rows", material.subUVRows, defaultData.subUVRows);
            changed |= DetailProperty::Draw_UintProperty("SubUV Cols", material.subUVCols, defaultData.subUVCols);
            DetailProperty::End_PropertyTable();
        }
    }

    return changed;
}

void EffectMaterialInstance_View::Apply_MaterialChanged(
    const Shared<Emitter_View>& emitterView,
    AuthoringEmitter& emitter,
    const RequiredModuleData& requiredData)
{
    emitter.previewDirty = true;
    emitter.rendererType = requiredData.rendererType;
    emitter.textureId = requiredData.material.mainTextureId;
    emitter.resourceSummary = requiredData.material.mainTextureId;

    if (emitterView)
        emitterView->MarkDirty();

    MarkDirty();
}

void EffectMaterialInstance_View::Begin_PendingAuthoringEdit(
    const Shared<Emitter_View>& emitterView,
    const EffectAuthoringSelection& selection,
    const Emitter_View::AuthoringSnapshot& beforeSnapshot,
    const string& description)
{
    if (emitterView == nullptr)
        return;

    if (!_hasPendingAuthoringEdit)
    {
        _hasPendingAuthoringEdit = true;
        _pendingAuthoringSelection = selection;
        _pendingAuthoringSnapshot = beforeSnapshot;
        _pendingAuthoringDescription = description;
    }
}

void EffectMaterialInstance_View::Commit_PendingAuthoringEditIfIdle(const Shared<Emitter_View>& emitterView)
{
    if (!_hasPendingAuthoringEdit || ImGui::IsAnyItemActive())
        return;

    Commit_PendingAuthoringEdit(emitterView);
}

void EffectMaterialInstance_View::Commit_PendingAuthoringEdit(const Shared<Emitter_View>& emitterView)
{
    if (!_hasPendingAuthoringEdit || emitterView == nullptr)
        return;

    Emitter_View::AuthoringSnapshot afterSnapshot = emitterView->Capture_AuthoringSnapshot();
    afterSnapshot.selectedEmitterIndex = _pendingAuthoringSnapshot.selectedEmitterIndex;
    afterSnapshot.selectedTypeData = _pendingAuthoringSnapshot.selectedTypeData;
    afterSnapshot.selectedModuleIndex = _pendingAuthoringSnapshot.selectedModuleIndex;
    emitterView->Execute_AuthoringSnapshotCommand(
        _pendingAuthoringSnapshot,
        afterSnapshot,
        _pendingAuthoringDescription
    );

    _hasPendingAuthoringEdit = false;
    _pendingAuthoringSelection = {};
    _pendingAuthoringSnapshot = {};
    _pendingAuthoringDescription.clear();
}

Shared<EffectMaterialInstance_View> EffectMaterialInstance_View::Create()
{
    return make_shared<EffectMaterialInstance_View>();
}

NS_END
