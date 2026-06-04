#include "EffectMaterialInstance_View.h"

#include "DetailPropertyContext.h"
#include "EffectEditorInstance.h"
#include "EffectMaterialPreviewRenderer.h"
#include "EffectMaterialScalarModulationPreview.h"
#include "Emitter_View.h"
#include "GameInstance.h"
#include "Helper_EffectAuthoring.h"
#include "Helper_String.h"
#include "Notification_Manager.h"
#include "Texture.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    constexpr auto kEffectTexturePayloadType = "CONTENT_BROWSER_EFFECT_TEXTURE";
    constexpr auto kTextureDialogDefaultFolder = L"../../../Client/Bin/Resources/Effects/Textures";

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
                return static_cast<wchar_t>(towlower(character));
            }
        );
        return value;
    }

    bool Is_SupportedTextureFile(const fs::path& filePath)
    {
        const string extension = ToLowerCopy(filePath.extension().string());
        return extension == ".png" || extension == ".dds";
    }

    bool Is_PngTextureFile(const fs::path& filePath)
    {
        return ToLowerCopy(filePath.extension().string()) == ".png";
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

    string Make_ResourceRelativePath(const fs::path& filePath)
    {
        if (GAME != nullptr)
        {
            const fs::path assetRoot = Normalize_Path(GAME->Get_AssetRoot());
            if (!assetRoot.empty())
            {
                const fs::path normalizedPath = Normalize_Path(filePath);
                const fs::path relativePath = normalizedPath.lexically_relative(assetRoot);
                if (!relativePath.empty() && !relativePath.native().starts_with(L".."))
                {
                    string result = String::ToString(relativePath.wstring());
                    ranges::replace(result, '\\', '/');
                    return result;
                }
            }
        }

        string result = String::ToString(Normalize_Path(filePath).wstring());
        ranges::replace(result, '\\', '/');
        return result;
    }

    wstring Resolve_InstanceTexturePathByGuid(const string& textureGuid)
    {
        if (GAME == nullptr || textureGuid.empty())
            return {};

        const AssetMeta* assetMeta = GAME->Find_AssetByGUID(textureGuid);
        if (assetMeta == nullptr || assetMeta->type != "Texture")
            return {};

        const wstring resolvedPath = GAME->Resolve_AssetPath(textureGuid);
        return !resolvedPath.empty() && fs::exists(resolvedPath) ? resolvedPath : wstring{};
    }

    wstring Resolve_InstanceTexturePathByFallbackPath(const string& texturePath)
    {
        if (!texturePath.empty())
        {
            fs::path candidatePath = String::ToWString(texturePath);
            if (candidatePath.is_relative() && GAME != nullptr)
                candidatePath = fs::path(GAME->Get_AssetRoot()) / candidatePath;

            candidatePath = Normalize_Path(candidatePath);
            if (fs::exists(candidatePath))
                return candidatePath.wstring();
        }

        return {};
    }

    wstring Resolve_InstanceTexturePath(const string& textureGuid, const string& texturePath)
    {
        wstring resolvedPath = Resolve_InstanceTexturePathByGuid(textureGuid);
        if (!resolvedPath.empty())
            return resolvedPath;

        return Resolve_InstanceTexturePathByFallbackPath(texturePath);
    }

    fs::path Resolve_PreferredTexturePath(const fs::path& filePath, bool& outSwitchedToDds)
    {
        outSwitchedToDds = false;

        fs::path targetPath = Normalize_Path(filePath);
        if (!Is_PngTextureFile(targetPath))
            return targetPath;

        fs::path ddsPath = targetPath;
        ddsPath.replace_extension(".dds");

        if (!fs::exists(ddsPath))
            return targetPath;

        outSwitchedToDds = true;
        return Normalize_Path(ddsPath);
    }

    void Notify_TextureAssignment(NotifyType type, const string& message)
    {
        if (EDITOR == nullptr || EDITOR->Get_Notification() == nullptr)
            return;

        EDITOR->Get_Notification()->Add_Notification_With_Type(type, "{}", message);
    }

    bool Try_AssignTextureFromFile(const fs::path& filePath, string& textureGuid, string& texturePath)
    {
        if (!Is_SupportedTextureFile(filePath))
        {
            textureAssignStatus = "Unsupported texture suffix. Use .png or .dds.";
            return false;
        }

        if (!fs::exists(filePath))
        {
            textureAssignStatus = "Texture file not found.";
            return false;
        }

        bool switchedToDds = false;
        const fs::path targetPath = Resolve_PreferredTexturePath(filePath, switchedToDds);

        texturePath = Make_ResourceRelativePath(targetPath);
        textureGuid = GAME != nullptr ? GAME->Ensure_AssetGUID(targetPath.wstring(), "Texture") : string{};

        if (switchedToDds)
        {
            const string sourcePath = Make_ResourceRelativePath(filePath);
            textureAssignStatus = "Texture switched PNG -> DDS: " + sourcePath + " -> " + texturePath;
            Notify_TextureAssignment(NotifyType::Info, textureAssignStatus);
        }
        else
            textureAssignStatus = "Texture assigned: " + texturePath;

        if (textureGuid.empty())
            Notify_TextureAssignment(NotifyType::Warning, textureAssignStatus + " (GUID cache unavailable)");

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

    Shared<Texture> Resolve_TextureThumbnail(
        map<string, Shared<Texture>>& textureCache,
        const string& textureGuid,
        const string& texturePath)
    {
        if (EDITOR == nullptr)
            return nullptr;

        const wstring resolvedPath = Resolve_InstanceTexturePath(textureGuid, texturePath);
        if (resolvedPath.empty())
            return nullptr;

        const string cacheKey = String::ToString(resolvedPath);
        const auto iter = textureCache.find(cacheKey);
        if (iter != textureCache.end())
            return iter->second;

        Shared<Texture> texture = Texture::Create(EDITOR->Get_Device(), EDITOR->Get_Context(), resolvedPath.c_str(), 1);
        textureCache[cacheKey] = texture;
        return texture;
    }

    ShaderResourceView* Get_TextureThumbnailSRV(
        map<string, Shared<Texture>>& textureCache,
        const string& textureGuid,
        const string& texturePath)
    {
        const Shared<Texture> texture = Resolve_TextureThumbnail(textureCache, textureGuid, texturePath);
        if (texture == nullptr || texture->Get_SRVs().empty())
            return nullptr;

        return texture->Get_SRVs().front().Get();
    }

    void Draw_TextureThumbnail(
        map<string, Shared<Texture>>& textureCache,
        const string& textureGuid,
        const string& texturePath,
        const ImVec2& size)
    {
        ShaderResourceView* srv = Get_TextureThumbnailSRV(textureCache, textureGuid, texturePath);
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
            texturePath.empty() && textureGuid.empty() ? "NONE" : "TEX"
        );
    }

    string Format_TextureReference(const string& textureGuid, const string& texturePath)
    {
        if (!texturePath.empty())
            return texturePath;

        if (!textureGuid.empty())
            return textureGuid;

        return "(none)";
    }

    string Make_TextureReferenceTooltip(const string& textureGuid, const string& texturePath)
    {
        string state = "Empty";
        if (!textureGuid.empty())
        {
            const bool guidResolved = !Resolve_InstanceTexturePathByGuid(textureGuid).empty();
            if (guidResolved && !texturePath.empty() && Resolve_InstanceTexturePathByFallbackPath(texturePath).empty())
                state = "GUID reference (path fallback missing)";
            else
                state = guidResolved ? "GUID reference" : "GUID missing";
        }
        else if (!texturePath.empty())
        {
            state = Resolve_InstanceTexturePathByFallbackPath(texturePath).empty()
                    ? "Path fallback missing"
                    : "Path fallback";
        }

        return
            "State: " + state +
            "\n" +
            "GUID: " + (textureGuid.empty() ? string{ "(none)" } : textureGuid) +
            "\nPath: " + (texturePath.empty() ? string{ "(none)" } : texturePath);
    }

    bool Is_WarningTextureReference(const string& textureGuid, const string& texturePath)
    {
        if (!textureGuid.empty())
            return Resolve_InstanceTexturePathByGuid(textureGuid).empty();

        if (!texturePath.empty())
            return Resolve_InstanceTexturePathByFallbackPath(texturePath).empty();

        return false;
    }

    bool Draw_TextureSlotBody(string& textureGuid, string& texturePath)
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
                    changed = Try_AssignTextureFromFile(static_cast<const wchar_t*>(payload->Data), textureGuid, texturePath);
            }
            ImGui::EndDragDropTarget();
        }

        const ImVec2 slotMin = ImGui::GetItemRectMin();
        const ImVec2 slotMax = ImGui::GetItemRectMax();
        const bool isHovered = ImGui::IsItemHovered();
        const bool isWarningReference = Is_WarningTextureReference(textureGuid, texturePath);
        const ImU32 fillColor = isWarningReference
                                ? (isHovered || isTextureDragging ? IM_COL32(58, 32, 34, 255) : IM_COL32(34, 22, 24, 255))
                                : isHovered || isTextureDragging ? IM_COL32(34, 42, 58, 255) : IM_COL32(22, 25, 32, 255);
        const ImU32 borderColor = isWarningReference
                                  ? IM_COL32(214, 86, 96, 255)
                                  : isHovered || isTextureDragging ? IM_COL32(116, 156, 235, 255) : IM_COL32(78, 88, 108, 255);
        const ImU32 textColor = isWarningReference
                                ? IM_COL32(255, 190, 196, 255)
                                : IM_COL32(220, 226, 238, 255);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(slotMin, slotMax, fillColor, 4.f);
        drawList->AddRect(slotMin, slotMax, borderColor, 4.f, 0, isTextureDragging ? 2.f : 1.f);

        const string displayText = Format_TextureReference(textureGuid, texturePath);
        drawList->PushClipRect(ImVec2(slotMin.x + 8.f, slotMin.y), ImVec2(slotMax.x - 8.f, slotMax.y), true);
        drawList->AddText(
            ImVec2(slotMin.x + 10.f, slotMin.y + slotSize.y * 0.5f - ImGui::GetTextLineHeight() * 0.5f),
            textColor,
            displayText.c_str()
        );
        drawList->PopClipRect();

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            const string tooltip =
                "Content Browser의 .png / .dds를 드롭하면 현재 instance texture reference로 지정합니다.\n\n" +
                Make_TextureReferenceTooltip(textureGuid, texturePath);
            ImGui::SetTooltip("%s", tooltip.c_str());
        }

        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2{ 0.5f, 0.5f });
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 0.f, 0.f });
        if (ImGui::Button(ICON_FA_FOLDER_OPEN "##PickTexture", ImVec2{ browseButtonWidth, slotSize.y }))
        {
            wstring pickedFilePath{};
            if (Try_PickTextureFileDialog(pickedFilePath))
                changed = Try_AssignTextureFromFile(pickedFilePath, textureGuid, texturePath);
        }
        ImGui::PopStyleVar(2);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("텍스처 파일 선택");

        return changed;
    }

    bool Draw_TextureProperty(
        map<string, Shared<Texture>>& textureCache,
        DetailPropertyContext& detailContext,
        const char* label,
        string& textureGuid,
        string& texturePath,
        const string& defaultGuid,
        const string& defaultPath)
    {
        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);

        constexpr ImVec2 thumbnailSize{ 44.f, 44.f };
        Draw_TextureThumbnail(textureCache, textureGuid, texturePath, thumbnailSize);
        ImGui::SameLine();
        bool changed = Draw_TextureSlotBody(textureGuid, texturePath);

        const bool resetClicked = detailContext.Draw_ResetButton(textureGuid != defaultGuid || texturePath != defaultPath);
        if (resetClicked)
        {
            textureGuid = defaultGuid;
            texturePath = defaultPath;
            textureAssignStatus = texturePath.empty() ? "Texture reset: (none)" : "Texture reset: " + texturePath;
            changed = true;
        }

        ImGui::PopID();
        return changed;
    }

    bool Equals_IgnoreCase(const string& lhs, const char* rhs)
    {
        return ToLowerCopy(lhs) == ToLowerCopy(rhs);
    }

    bool Draw_StringComboProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        string& value,
        const string& defaultValue,
        const char* const* options,
        size_t optionCount)
    {
        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);

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

        const bool resetClicked = detailContext.Draw_ResetButton(value != defaultValue);
        if (resetClicked)
        {
            value = defaultValue;
            changed = true;
        }

        ImGui::PopID();
        return changed;
    }

    static const char* Get_BlendModeLabel(EffectMaterialBlendMode value)
    {
        switch (value)
        {
        case EffectMaterialBlendMode::AlphaBlend:
            return "AlphaBlend";
        case EffectMaterialBlendMode::Additive:
            return "Additive";
        case EffectMaterialBlendMode::Masked:
            return "Masked";
        case EffectMaterialBlendMode::Modulate:
            return "Modulate";
        default:
            return "AlphaBlend";
        }
    }

    bool Draw_BlendModeProperty(
        DetailPropertyContext& detailContext,
        EffectMaterialBlendMode& value,
        EffectMaterialBlendMode defaultValue)
    {
        static constexpr EffectMaterialBlendMode kModes[]{
            EffectMaterialBlendMode::AlphaBlend,
            EffectMaterialBlendMode::Additive,
            EffectMaterialBlendMode::Masked,
            EffectMaterialBlendMode::Modulate
        };

        ImGui::PushID("Blend Mode");
        detailContext.Draw_PropertyLabel("Blend Mode");
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = false;

        if (ImGui::BeginCombo("##Value", Get_BlendModeLabel(value)))
        {
            for (const EffectMaterialBlendMode mode : kModes)
            {
                const bool selected = value == mode;
                const bool selectable =
                    mode == EffectMaterialBlendMode::AlphaBlend ||
                    mode == EffectMaterialBlendMode::Additive ||
                    mode == EffectMaterialBlendMode::Masked;

                if (!selectable)
                    ImGui::BeginDisabled();

                if (ImGui::Selectable(Get_BlendModeLabel(mode), selected) && selectable)
                {
                    value = mode;
                    changed = true;
                }

                if (!selectable)
                    ImGui::EndDisabled();

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        const bool resetClicked = detailContext.Draw_ResetButton(value != defaultValue);
        if (resetClicked)
        {
            value = defaultValue;
            changed = true;
        }

        ImGui::PopID();
        return changed;
    }

    const char* Get_AdditiveColorSourceLabel(EffectMaterialAdditiveColorSource value)
    {
        switch (value)
        {
        case EffectMaterialAdditiveColorSource::MainRGB:
            return "Main RGB";
        case EffectMaterialAdditiveColorSource::TintedRGB:
            return "Tinted RGB";
        case EffectMaterialAdditiveColorSource::EmissiveColor:
            return "Emissive Color";
        case EffectMaterialAdditiveColorSource::ConstantColor:
            return "Constant Color";
        default:
            return "Main RGB";
        }
    }

    const char* Get_AdditiveAmountSourceLabel(EffectMaterialAdditiveAmountSource value)
    {
        switch (value)
        {
        case EffectMaterialAdditiveAmountSource::Alpha:
            return "Alpha";
        case EffectMaterialAdditiveAmountSource::Red:
            return "Red";
        case EffectMaterialAdditiveAmountSource::Luminance:
            return "Luminance";
        case EffectMaterialAdditiveAmountSource::Mask:
            return "Mask";
        case EffectMaterialAdditiveAmountSource::One:
            return "One";
        default:
            return "Alpha";
        }
    }

    const char* Get_AdditiveCoveragePolicyLabel(EffectMaterialAdditiveCoveragePolicy value)
    {
        switch (value)
        {
        case EffectMaterialAdditiveCoveragePolicy::AmountOnly:
            return "Amount Only";
        case EffectMaterialAdditiveCoveragePolicy::CoverageAndAmount:
            return "Coverage And Amount";
        case EffectMaterialAdditiveCoveragePolicy::Independent:
            return "Independent";
        default:
            return "Amount Only";
        }
    }

    template <typename EnumType>
    bool Draw_EnumComboProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        EnumType& value,
        EnumType defaultValue,
        const EnumType* options,
        size_t optionCount,
        const char* (*getLabel)(EnumType))
    {
        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = false;

        if (ImGui::BeginCombo("##Value", getLabel(value)))
        {
            for (size_t i = 0; i < optionCount; ++i)
            {
                const EnumType option = options[i];
                const bool selected = value == option;
                if (ImGui::Selectable(getLabel(option), selected))
                {
                    value = option;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        const bool resetClicked = detailContext.Draw_ResetButton(value != defaultValue);
        if (resetClicked)
        {
            value = defaultValue;
            changed = true;
        }

        ImGui::PopID();
        return changed;
    }

    bool Draw_AdditivePresetProperty(DetailPropertyContext& detailContext, EffectMaterialAdditiveContributionData& additive)
    {
        struct AdditivePreset
        {
            const char* label{};
            EffectMaterialAdditiveContributionData data{};
        };

        static const AdditivePreset kPresets[]{
            {
                "RGB Additive",
                EffectMaterialAdditiveContributionData{
                    .colorSource = EffectMaterialAdditiveColorSource::MainRGB,
                    .amountSource = EffectMaterialAdditiveAmountSource::Alpha
                }
            },
            {
                "Tinted RGB Additive",
                EffectMaterialAdditiveContributionData{
                    .colorSource = EffectMaterialAdditiveColorSource::TintedRGB,
                    .amountSource = EffectMaterialAdditiveAmountSource::Alpha
                }
            },
            {
                "Emissive Additive",
                EffectMaterialAdditiveContributionData{
                    .colorSource = EffectMaterialAdditiveColorSource::EmissiveColor,
                    .amountSource = EffectMaterialAdditiveAmountSource::Alpha
                }
            },
            {
                "Channel Packed Additive",
                EffectMaterialAdditiveContributionData{
                    .colorSource = EffectMaterialAdditiveColorSource::ConstantColor,
                    .amountSource = EffectMaterialAdditiveAmountSource::Red
                }
            },
            {
                "Unmasked Additive",
                EffectMaterialAdditiveContributionData{
                    .colorSource = EffectMaterialAdditiveColorSource::MainRGB,
                    .amountSource = EffectMaterialAdditiveAmountSource::One
                }
            },
        };

        ImGui::PushID("Additive Preset");
        detailContext.Draw_PropertyLabel("Preset");
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = false;
        if (ImGui::BeginCombo("##Value", "Apply preset..."))
        {
            for (const AdditivePreset& preset : kPresets)
            {
                if (ImGui::Selectable(preset.label))
                {
                    additive = preset.data;
                    changed = true;
                }
            }

            ImGui::EndCombo();
        }

        detailContext.Draw_ResetButton(false);
        ImGui::PopID();
        return changed;
    }

    bool Draw_AdditiveContributionPanel(
        DetailPropertyContext& detailContext,
        EffectMaterialAdditiveContributionData& additive,
        const EffectMaterialAdditiveContributionData& defaultAdditive)
    {
        bool changed = false;
        if (!ImGui::TreeNodeEx("Additive Contribution", ImGuiTreeNodeFlags_DefaultOpen))
            return changed;

        if (detailContext.Begin_PropertyTable("EffectMaterialInstanceAdditiveContribution"))
        {
            static constexpr EffectMaterialAdditiveColorSource kColorSources[]{
                EffectMaterialAdditiveColorSource::MainRGB,
                EffectMaterialAdditiveColorSource::TintedRGB,
                EffectMaterialAdditiveColorSource::EmissiveColor,
                EffectMaterialAdditiveColorSource::ConstantColor
            };
            static constexpr EffectMaterialAdditiveAmountSource kAmountSources[]{
                EffectMaterialAdditiveAmountSource::Alpha,
                EffectMaterialAdditiveAmountSource::Red,
                EffectMaterialAdditiveAmountSource::Luminance,
                EffectMaterialAdditiveAmountSource::Mask,
                EffectMaterialAdditiveAmountSource::One
            };
            static constexpr EffectMaterialAdditiveCoveragePolicy kCoveragePolicies[]{
                EffectMaterialAdditiveCoveragePolicy::AmountOnly,
                EffectMaterialAdditiveCoveragePolicy::CoverageAndAmount,
                EffectMaterialAdditiveCoveragePolicy::Independent
            };

            changed |= Draw_AdditivePresetProperty(detailContext, additive);
            changed |= Draw_EnumComboProperty(
                detailContext,
                "Color Source",
                additive.colorSource,
                defaultAdditive.colorSource,
                kColorSources,
                std::size(kColorSources),
                Get_AdditiveColorSourceLabel
            );
            changed |= Draw_EnumComboProperty(
                detailContext,
                "Amount Source",
                additive.amountSource,
                defaultAdditive.amountSource,
                kAmountSources,
                std::size(kAmountSources),
                Get_AdditiveAmountSourceLabel
            );
            changed |= Draw_EnumComboProperty(
                detailContext,
                "Coverage Policy",
                additive.coveragePolicy,
                defaultAdditive.coveragePolicy,
                kCoveragePolicies,
                std::size(kCoveragePolicies),
                Get_AdditiveCoveragePolicyLabel
            );
            changed |= detailContext.Draw_ColorProperty("Emissive Color", additive.emissiveColor, defaultAdditive.emissiveColor);
            changed |= detailContext.Draw_ColorProperty("Constant Color", additive.constantColor, defaultAdditive.constantColor);
            changed |= detailContext.Draw_FloatProperty("Intensity Scale", additive.intensityScale, defaultAdditive.intensityScale);
            changed |= detailContext.Draw_BoolProperty("Black Neutral", additive.blackNeutral, defaultAdditive.blackNeutral);
            detailContext.End_PropertyTable();
        }

        ImGui::TreePop();
        return changed;
    }

    const char* Get_UVTilingModeLabel(EffectTextureUVTilingMode value)
    {
        switch (value)
        {
        case EffectTextureUVTilingMode::Wrap:
            return "Wrap";
        case EffectTextureUVTilingMode::Stretch:
            return "Stretch";
        case EffectTextureUVTilingMode::Clamp:
            return "Clamp";
        case EffectTextureUVTilingMode::Raw:
            return "Raw";
        default:
            return "Wrap";
        }
    }

    bool Draw_UVTilingModeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        EffectTextureUVTilingMode& value,
        EffectTextureUVTilingMode defaultValue)
    {
        static constexpr EffectTextureUVTilingMode kModes[]{
            EffectTextureUVTilingMode::Wrap,
            EffectTextureUVTilingMode::Stretch,
            EffectTextureUVTilingMode::Clamp,
            EffectTextureUVTilingMode::Raw
        };

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = false;

        if (ImGui::BeginCombo("##Value", Get_UVTilingModeLabel(value)))
        {
            for (const EffectTextureUVTilingMode mode : kModes)
            {
                const bool selected = value == mode;
                if (ImGui::Selectable(Get_UVTilingModeLabel(mode), selected))
                {
                    value = mode;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        const bool resetClicked = detailContext.Draw_ResetButton(value != defaultValue);
        if (resetClicked)
        {
            value = defaultValue;
            changed = true;
        }

        ImGui::PopID();
        return changed;
    }

    const char* Get_MaterialFamilyLabel(EffectMaterialFamily value)
    {
        switch (value)
        {
        case EffectMaterialFamily::Default:
            return "Default";
        case EffectMaterialFamily::SpriteDistortion:
            return "SpriteDistortion";
        default:
            return "Unknown";
        }
    }

    const char* Get_DistortionShapeModeLabel(EffectDistortionShapeMode value)
    {
        switch (value)
        {
        case EffectDistortionShapeMode::None:
            return "None";
        case EffectDistortionShapeMode::Radial:
            return "Radial";
        case EffectDistortionShapeMode::Ring:
            return "Ring";
        case EffectDistortionShapeMode::ShockwaveRing:
            return "ShockwaveRing";
        default:
            return "None";
        }
    }

    bool Draw_DistortionShapeModeProperty(
        DetailPropertyContext& detailContext,
        EffectDistortionShapeMode& value,
        EffectDistortionShapeMode defaultValue)
    {
        static constexpr EffectDistortionShapeMode kModes[]{
            EffectDistortionShapeMode::None,
            EffectDistortionShapeMode::Radial,
            EffectDistortionShapeMode::Ring,
            EffectDistortionShapeMode::ShockwaveRing
        };

        return Draw_EnumComboProperty(
            detailContext,
            "Shape Mode",
            value,
            defaultValue,
            kModes,
            sizeof(kModes) / sizeof(kModes[0]),
            Get_DistortionShapeModeLabel
        );
    }

    void Draw_ReadOnlyTextProperty(DetailPropertyContext& detailContext, const char* label, const char* value)
    {
        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::TextDisabled("%s", value);
        ImGui::PopID();
    }

    void Draw_ReadOnlyFloatProperty(DetailPropertyContext& detailContext, const char* label, float value)
    {
        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::TextDisabled("%.3f", value);
        ImGui::PopID();
    }

    void Draw_ReadOnlyCheckedBoolProperty(DetailPropertyContext& detailContext, const char* label, bool value, const char* tooltip = nullptr)
    {
        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label, tooltip);
        ImGui::BeginDisabled();
        ImGui::Checkbox("##Value", &value);
        ImGui::EndDisabled();
        detailContext.Draw_ResetButton(false);
        ImGui::PopID();
    }
}

EffectMaterialInstance_View::EffectMaterialInstance_View()
    : Editor_Window{ L"Effect Material Instance", ICON_FA_PALETTE }
    , _previewRenderer{ EffectMaterialPreviewRenderer::Create() }
{
}

EffectMaterialInstance_View::~EffectMaterialInstance_View()
{
    _textureThumbnailCache.clear();
}

void EffectMaterialInstance_View::Update(float timeDelta)
{
    __super::Update(timeDelta);

    if (Is_Open() && _previewRenderer != nullptr)
        _previewRenderer->Update_MaterialTime(timeDelta);
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

    if (const MaterialScalarModulationModuleData* modulation = Find_MaterialScalarModulationModuleData(*emitter))
    {
        _previewRenderer->Render_Material(
            requiredData->material,
            Build_CoreColorRgbModulationRuntimeDesc(*modulation),
            Build_MaterialScalarModulationRuntimeDesc(*modulation),
            requiredData->duration,
            false
        );
    }
    else
        _previewRenderer->Render_Material(requiredData->material);
}

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
            bool restartPreview = false;
            const float contentWidth = ImGui::GetContentRegionAvail().x;
            const bool useSideBySideLayout = contentWidth >= 760.f;
            const EffectAuthoringSelection selection{
                EffectAuthoringSelectionKind::Module,
                _targetEmitterId,
                _targetModuleId
            };
            const Emitter_View::AuthoringSnapshot beforeSnapshot = emitterView->Capture_AuthoringSnapshot();
            const EffectMaterialTwoSidedDisplayMode twoSidedDisplayMode =
                emitter->typeData.kind == AuthoringTypeDataKind::None
                ? EffectMaterialTwoSidedDisplayMode::ForcedEnabledReadOnly
                : emitter->typeData.kind == AuthoringTypeDataKind::Trail
                  ? EffectMaterialTwoSidedDisplayMode::EditableUnimplemented
                  : EffectMaterialTwoSidedDisplayMode::EditableImplemented;

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
                    changed |= Draw_InstanceProperties(
                        _detailPropertyContext,
                        _textureThumbnailCache,
                        requiredData->material,
                        &restartPreview,
                        twoSidedDisplayMode
                    );
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

                changed |= Draw_InstanceProperties(
                    _detailPropertyContext,
                    _textureThumbnailCache,
                    requiredData->material,
                    &restartPreview,
                    twoSidedDisplayMode
                );
            }

            if (changed)
            {
                Apply_MaterialChanged(emitterView, *emitter, *requiredData);
                Begin_PendingAuthoringEdit(emitterView, selection, beforeSnapshot, "Edit Material Instance");

                if (restartPreview && EDITOR != nullptr)
                    EDITOR->Request_RestartPreview();
            }

            Commit_PendingAuthoringEditIfIdle(emitterView);
        }
    }

    Set_Open(isOpen);
    ImGui::End();
}

bool EffectMaterialInstance_View::Draw_InstanceProperties(
    DetailPropertyContext& detailPropertyContext,
    map<string, Shared<Texture>>& textureThumbnailCache,
    EffectMaterialInstanceData& material,
    bool* outRestartPreview,
    EffectMaterialTwoSidedDisplayMode twoSidedDisplayMode,
    bool disableMeshDistortionShapeControls,
    const EffectMaterialRenderPolicyBinding* renderPolicyBinding)
{
    const EffectMaterialInstanceData defaultData{};
    const EffectMaterialBlendMode effectiveBlendMode =
        renderPolicyBinding != nullptr ? renderPolicyBinding->blendMode : material.blendMode;
    bool changed = false;
    bool restartPreview = false;

    constexpr const char* sourceOptions[]{ "Alpha", "Red", "Luminance" };

    if (ImGui::CollapsingHeader("Main", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailPropertyContext.Begin_PropertyTable("EffectMaterialInstanceMain"))
        {
            changed |= Internal::Draw_TextureProperty(
                textureThumbnailCache,
                detailPropertyContext,
                "Main Texture",
                material.mainTextureGuid,
                material.mainTexturePath,
                defaultData.mainTextureGuid,
                defaultData.mainTexturePath
            );
            changed |= Internal::Draw_StringComboProperty(
                detailPropertyContext,
                "Opacity Source",
                material.opacitySource,
                defaultData.opacitySource,
                sourceOptions,
                std::size(sourceOptions)
            );
            changed |= detailPropertyContext.Draw_ColorProperty("Tint", material.tint, defaultData.tint);
            changed |= detailPropertyContext.Draw_FloatProperty("Intensity", material.intensity, defaultData.intensity);
            changed |= detailPropertyContext.Draw_FloatProperty("Opacity Power", material.opacityPower, defaultData.opacityPower);
            const bool mainModeChanged = Internal::Draw_UVTilingModeProperty(
                detailPropertyContext,
                "Main UV Tiling Mode",
                material.mainUVTilingMode,
                defaultData.mainUVTilingMode
            );
            changed |= mainModeChanged;
            restartPreview |= mainModeChanged;
            changed |= detailPropertyContext.Draw_Vec2Property("Main UV Scale", material.mainUVScale, defaultData.mainUVScale);
            changed |= detailPropertyContext.Draw_Vec2Property("Main UV Offset", material.mainUVOffset, defaultData.mainUVOffset);
            changed |= detailPropertyContext.Draw_Vec2Property("Main UV Scroll Speed", material.mainUVScrollSpeed, defaultData.mainUVScrollSpeed);
            detailPropertyContext.End_PropertyTable();
        }

        if (!Internal::textureAssignStatus.empty())
            ImGui::TextDisabled("%s", Internal::textureAssignStatus.c_str());
    }

    if (ImGui::CollapsingHeader("Noise", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailPropertyContext.Begin_PropertyTable("EffectMaterialInstanceNoise"))
        {
            changed |= Internal::Draw_TextureProperty(
                textureThumbnailCache,
                detailPropertyContext,
                "Noise Texture",
                material.noiseTextureGuid,
                material.noiseTexturePath,
                defaultData.noiseTextureGuid,
                defaultData.noiseTexturePath
            );
            changed |= Internal::Draw_StringComboProperty(
                detailPropertyContext,
                "Noise Source",
                material.noiseSource,
                defaultData.noiseSource,
                sourceOptions,
                std::size(sourceOptions)
            );
            changed |= detailPropertyContext.Draw_BoolProperty("Noise Invert", material.noiseInvert, defaultData.noiseInvert);
            changed |= detailPropertyContext.Draw_FloatProperty("Noise Strength", material.noiseStrength, defaultData.noiseStrength);
            const bool noiseModeChanged = Internal::Draw_UVTilingModeProperty(
                detailPropertyContext,
                "Noise UV Tiling Mode",
                material.noiseUVTilingMode,
                defaultData.noiseUVTilingMode
            );
            changed |= noiseModeChanged;
            restartPreview |= noiseModeChanged;
            changed |= detailPropertyContext.Draw_Vec2Property("Noise UV Scale", material.noiseUVScale, defaultData.noiseUVScale);
            changed |= detailPropertyContext.Draw_Vec2Property("Noise UV Offset", material.noiseUVOffset, defaultData.noiseUVOffset);
            changed |= detailPropertyContext.Draw_Vec2Property("Noise UV Scroll Speed", material.noiseUVScrollSpeed, defaultData.noiseUVScrollSpeed);
            detailPropertyContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("Mask", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailPropertyContext.Begin_PropertyTable("EffectMaterialInstanceMask"))
        {
            changed |= Internal::Draw_TextureProperty(
                textureThumbnailCache,
                detailPropertyContext,
                "Mask Texture",
                material.maskTextureGuid,
                material.maskTexturePath,
                defaultData.maskTextureGuid,
                defaultData.maskTexturePath
            );
            changed |= Internal::Draw_StringComboProperty(
                detailPropertyContext,
                "Mask Source",
                material.maskSource,
                defaultData.maskSource,
                sourceOptions,
                std::size(sourceOptions)
            );
            changed |= detailPropertyContext.Draw_BoolProperty("Mask Invert", material.maskInvert, defaultData.maskInvert);
            if (renderPolicyBinding != nullptr)
                Internal::Draw_ReadOnlyFloatProperty(detailPropertyContext, "Alpha Cutoff", renderPolicyBinding->alphaCutoff);
            else
                changed |= detailPropertyContext.Draw_FloatProperty("Alpha Cutoff", material.alphaCutoff, defaultData.alphaCutoff);
            changed |= detailPropertyContext.Draw_FloatProperty("Alpha Erosion", material.alphaErosion, defaultData.alphaErosion);
            const bool maskModeChanged = Internal::Draw_UVTilingModeProperty(
                detailPropertyContext,
                "Mask UV Tiling Mode",
                material.maskUVTilingMode,
                defaultData.maskUVTilingMode
            );
            changed |= maskModeChanged;
            restartPreview |= maskModeChanged;
            changed |= detailPropertyContext.Draw_Vec2Property("Mask UV Scale", material.maskUVScale, defaultData.maskUVScale);
            changed |= detailPropertyContext.Draw_Vec2Property("Mask UV Offset", material.maskUVOffset, defaultData.maskUVOffset);
            changed |= detailPropertyContext.Draw_Vec2Property("Mask UV Scroll Speed", material.maskUVScrollSpeed, defaultData.maskUVScrollSpeed);
            detailPropertyContext.End_PropertyTable();
        }
    }

    if (material.materialFamily == EffectMaterialFamily::SpriteDistortion &&
        ImGui::CollapsingHeader("Flow / Distortion", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailPropertyContext.Begin_PropertyTable("EffectMaterialInstanceDistortion"))
        {
            changed |= Internal::Draw_TextureProperty(
                textureThumbnailCache,
                detailPropertyContext,
                "Flow Texture",
                material.flowTextureGuid,
                material.flowTexturePath,
                defaultData.flowTextureGuid,
                defaultData.flowTexturePath
            );
            const bool flowModeChanged = Internal::Draw_UVTilingModeProperty(
                detailPropertyContext,
                "Flow UV Tiling Mode",
                material.flowUVTilingMode,
                defaultData.flowUVTilingMode
            );
            changed |= flowModeChanged;
            restartPreview |= flowModeChanged;
            changed |= detailPropertyContext.Draw_Vec2Property("Flow UV Scale", material.flowUVScale, defaultData.flowUVScale);
            changed |= detailPropertyContext.Draw_Vec2Property("Flow UV Offset", material.flowUVOffset, defaultData.flowUVOffset);
            changed |= detailPropertyContext.Draw_Vec2Property("Flow UV Scroll Speed", material.flowUVScrollSpeed, defaultData.flowUVScrollSpeed);
            changed |= detailPropertyContext.Draw_FloatProperty("Refraction Intensity", material.refractionIntensity, defaultData.refractionIntensity);
            changed |= detailPropertyContext.Draw_FloatProperty("Refraction Presence", material.refractionPresence, defaultData.refractionPresence);
            if (disableMeshDistortionShapeControls)
            {
                Internal::Draw_ReadOnlyTextProperty(
                    detailPropertyContext,
                    "Shape Mode",
                    "Mesh Distortion v0에서는 Shape 미지원"
                );
                detailPropertyContext.End_PropertyTable();
            }
            else
            {
                const EffectDistortionShapeMode previousShapeMode = material.distortionShapeMode;
                const bool shapeModeChanged = Internal::Draw_DistortionShapeModeProperty(
                    detailPropertyContext,
                    material.distortionShapeMode,
                    defaultData.distortionShapeMode
                );
                changed |= shapeModeChanged;
                if (shapeModeChanged &&
                    previousShapeMode != EffectDistortionShapeMode::ShockwaveRing &&
                    material.distortionShapeMode == EffectDistortionShapeMode::ShockwaveRing)
                {
                    material.distortionShapeThickness = 14.f;
                    material.distortionShapeSoftness = 4.f;
                }
                if (material.distortionShapeMode == EffectDistortionShapeMode::Radial ||
                    material.distortionShapeMode == EffectDistortionShapeMode::Ring)
                {
                    changed |= detailPropertyContext.Draw_FloatProperty(
                        "Shape Radius",
                        material.distortionShapeRadius,
                        defaultData.distortionShapeRadius
                    );
                    if (material.distortionShapeMode == EffectDistortionShapeMode::Ring)
                    {
                        changed |= detailPropertyContext.Draw_FloatProperty(
                            "Shape Thickness",
                            material.distortionShapeThickness,
                            defaultData.distortionShapeThickness
                        );
                    }
                    changed |= detailPropertyContext.Draw_FloatProperty(
                        "Shape Softness",
                        material.distortionShapeSoftness,
                        defaultData.distortionShapeSoftness
                    );
                }
                else if (material.distortionShapeMode == EffectDistortionShapeMode::ShockwaveRing)
                {
                    changed |= detailPropertyContext.Draw_FloatProperty(
                        "Shape Thickness (px)",
                        material.distortionShapeThickness,
                        defaultData.distortionShapeThickness
                    );
                    changed |= detailPropertyContext.Draw_FloatProperty(
                        "Shape Softness (px)",
                        material.distortionShapeSoftness,
                        defaultData.distortionShapeSoftness
                    );
                }
                detailPropertyContext.End_PropertyTable();
            }
        }
    }
    else if (material.materialFamily != EffectMaterialFamily::SpriteDistortion)
    {
        ImGui::TextDisabled("Flow / Distortion fields are shown only when the embedded material family is SpriteDistortion.");
    }

    if (ImGui::CollapsingHeader("Blend / Render", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailPropertyContext.Begin_PropertyTable("EffectMaterialInstanceBlendRender"))
        {
            Internal::Draw_ReadOnlyTextProperty(
                detailPropertyContext,
                "Family",
                Internal::Get_MaterialFamilyLabel(material.materialFamily)
            );
            if (renderPolicyBinding != nullptr)
            {
                Internal::Draw_ReadOnlyTextProperty(
                    detailPropertyContext,
                    "Render Policy Source",
                    renderPolicyBinding->sourceLabel
                );
                Internal::Draw_ReadOnlyTextProperty(
                    detailPropertyContext,
                    "Blend Mode",
                    Internal::Get_BlendModeLabel(renderPolicyBinding->blendMode)
                );
            }
            else
                changed |= Internal::Draw_BlendModeProperty(detailPropertyContext, material.blendMode, defaultData.blendMode);
            switch (twoSidedDisplayMode)
            {
            case EffectMaterialTwoSidedDisplayMode::EditableImplemented:
                changed |= detailPropertyContext.Draw_BoolProperty("Two Sided", material.twoSided, defaultData.twoSided);
                break;

            case EffectMaterialTwoSidedDisplayMode::ForcedEnabledReadOnly:
                Internal::Draw_ReadOnlyCheckedBoolProperty(
                    detailPropertyContext,
                    "Two Sided",
                    true,
                    "현재 sprite renderer는 항상 양면 렌더라서 이 옵션을 개별 제어하지 않습니다."
                );
                break;

            case EffectMaterialTwoSidedDisplayMode::EditableUnimplemented:
            default:
                changed |= detailPropertyContext.Draw_BoolProperty("Two Sided (미구현)", material.twoSided, defaultData.twoSided);
                break;
            }
            changed |= detailPropertyContext.Draw_UintProperty("SubUV Cols", material.subUVCols, defaultData.subUVCols);
            changed |= detailPropertyContext.Draw_UintProperty("SubUV Rows", material.subUVRows, defaultData.subUVRows);
            detailPropertyContext.End_PropertyTable();
        }

        if (effectiveBlendMode == EffectMaterialBlendMode::Additive)
            changed |= Internal::Draw_AdditiveContributionPanel(detailPropertyContext, material.additive, defaultData.additive);

        if (twoSidedDisplayMode == EffectMaterialTwoSidedDisplayMode::ForcedEnabledReadOnly)
            ImGui::TextDisabled("Sprite는 현재 항상 양면 렌더입니다.");
    }

    if (ImGui::CollapsingHeader("Core Emissive", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailPropertyContext.Begin_PropertyTable("EffectMaterialInstanceCoreEmissive"))
        {
            changed |= detailPropertyContext.Draw_BoolProperty(
                "Enabled",
                material.coreEmissive.enabled,
                defaultData.coreEmissive.enabled
            );
            changed |= detailPropertyContext.Draw_ColorRgbProperty(
                "Core Color",
                material.coreEmissive.coreColor,
                defaultData.coreEmissive.coreColor
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Core Power",
                material.coreEmissive.corePower,
                defaultData.coreEmissive.corePower
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Core Intensity",
                material.coreEmissive.coreIntensity,
                defaultData.coreEmissive.coreIntensity
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Outer Power",
                material.coreEmissive.outerPower,
                defaultData.coreEmissive.outerPower
            );
            changed |= detailPropertyContext.Draw_FloatProperty(
                "Outer Intensity",
                material.coreEmissive.outerIntensity,
                defaultData.coreEmissive.outerIntensity
            );
            detailPropertyContext.End_PropertyTable();
        }
    }

    if (outRestartPreview != nullptr)
        *outRestartPreview = restartPreview;

    return changed;
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

void EffectMaterialInstance_View::Apply_MaterialChanged(
    const Shared<Emitter_View>& emitterView,
    AuthoringEmitter& emitter,
    const RequiredModuleData& requiredData)
{
    emitter.previewDirty = true;
    emitter.rendererType = emitter.typeData.kind == AuthoringTypeDataKind::None
                           ? requiredData.rendererType
                           : Authoring::Resolve_RendererType(emitter.typeData);
    emitter.textureId = requiredData.material.mainTexturePath;
    emitter.resourceSummary = requiredData.material.mainTexturePath;

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
