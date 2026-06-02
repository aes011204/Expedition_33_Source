#include "Detail_View.h"

#include "DetailPropertyContext.h"
#include "Editor_Context.h"
#include "EffectEditorInstance.h"
#include "EffectMaterialPresetReader.h"
#include "GameInstance.h"
#include "Helper_String.h"
#include "MeshDataPreview_View.h"

NS_BEGIN(EffectEditor)

namespace
{
    constexpr TrailCurveQuality kTrailCurveQualityValues[] = {
        TrailCurveQuality::Basic,
        TrailCurveQuality::Smooth,
        TrailCurveQuality::HighQuality,
        TrailCurveQuality::Custom,
    };

    const char* Get_TrailCurveQualityLabel(TrailCurveQuality value)
    {
        switch (value)
        {
        case TrailCurveQuality::Basic:
            return "기본";
        case TrailCurveQuality::Smooth:
            return "부드럽게";
        case TrailCurveQuality::HighQuality:
            return "고품질";
        case TrailCurveQuality::Custom:
            return "사용자 지정";
        default:
            return "기본";
        }
    }

    const char* Get_TrailCurveQualityTooltip(TrailCurveQuality value)
    {
        switch (value)
        {
        case TrailCurveQuality::Basic:
            return "기존 호환에 가까운 낮은 보간 비용 설정입니다.";
        case TrailCurveQuality::Smooth:
            return "샘플 간격과 보간 수를 늘려 swing arc의 각짐을 줄입니다.";
        case TrailCurveQuality::HighQuality:
            return "더 촘촘한 샘플로 곡선을 만들지만 preview 비용이 늘 수 있습니다.";
        case TrailCurveQuality::Custom:
            return "아래 세부 곡선 값을 직접 편집합니다.";
        default:
            return "";
        }
    }

    const char* Get_RenderLayerOverrideLabel(EmitterRenderLayerOverride value)
    {
        switch (value)
        {
        case EmitterRenderLayerOverride::UIEffect:
            return "UIEffect";
        case EmitterRenderLayerOverride::Auto:
        default:
            return "Auto";
        }
    }

    bool Draw_RenderLayerOverrideProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        EmitterRenderLayerOverride& value,
        EmitterRenderLayerOverride defaultValue)
    {
        static constexpr EmitterRenderLayerOverride kValues[] = {
            EmitterRenderLayerOverride::Auto,
            EmitterRenderLayerOverride::UIEffect,
        };

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);

        bool changed = false;
        if (ImGui::BeginCombo("##Value", Get_RenderLayerOverrideLabel(value)))
        {
            for (const EmitterRenderLayerOverride candidate : kValues)
            {
                const bool selected = value == candidate;
                if (ImGui::Selectable(Get_RenderLayerOverrideLabel(candidate), selected))
                {
                    value = candidate;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        const bool resetClicked = detailContext.Draw_ResetButton(value != defaultValue);
        ImGui::PopID();

        if (resetClicked)
        {
            value = defaultValue;
            return true;
        }

        return changed;
    }

    TrailTypeData Build_TrailCurvePreset(TrailCurveQuality quality)
    {
        TrailTypeData preset{};

        switch (quality)
        {
        case TrailCurveQuality::Smooth:
            preset.sampleSpacing = 0.04f;
            preset.curveSubdivision = 8u;
            preset.smoothTangent = true;
            preset.sideFade = 0.12f;
            break;
        case TrailCurveQuality::HighQuality:
            preset.sampleSpacing = 0.02f;
            preset.curveSubdivision = 12u;
            preset.smoothTangent = true;
            preset.sideFade = 0.16f;
            break;
        case TrailCurveQuality::Basic:
        case TrailCurveQuality::Custom:
        default:
            preset.sampleSpacing = 0.08f;
            preset.curveSubdivision = 4u;
            preset.smoothTangent = true;
            preset.sideFade = 0.08f;
            break;
        }

        return preset;
    }

    bool Apply_TrailCurvePreset(TrailTypeData& data)
    {
        if (data.curveQuality == TrailCurveQuality::Custom)
            return false;

        const TrailTypeData preset = Build_TrailCurvePreset(data.curveQuality);
        bool changed = false;

        if (data.sampleSpacing != preset.sampleSpacing)
        {
            data.sampleSpacing = preset.sampleSpacing;
            changed = true;
        }
        if (data.curveSubdivision != preset.curveSubdivision)
        {
            data.curveSubdivision = preset.curveSubdivision;
            changed = true;
        }
        if (data.smoothTangent != preset.smoothTangent)
        {
            data.smoothTangent = preset.smoothTangent;
            changed = true;
        }
        if (data.sideFade != preset.sideFade)
        {
            data.sideFade = preset.sideFade;
            changed = true;
        }

        return changed;
    }

    string Make_AssetRelativePath(const fs::path& filePath)
    {
        if (GAME == nullptr)
            return String::ToString(filePath.lexically_normal().wstring());

        const fs::path assetRoot = fs::path(GAME->Get_AssetRoot()).lexically_normal();
        const fs::path normalizedPath = filePath.lexically_normal();
        if (!assetRoot.empty())
        {
            const fs::path relativePath = normalizedPath.lexically_relative(assetRoot);
            if (!relativePath.empty() && relativePath.native().find(L"..") != 0)
                return String::ToString(relativePath.generic_wstring());
        }

        return String::ToString(normalizedPath.wstring());
    }

    fs::path Resolve_AuthoredAssetPath(const string& guid, const string& path)
    {
        if (GAME == nullptr)
            return fs::path(String::ToWString(path));

        if (!guid.empty())
        {
            const wstring resolvedPath = GAME->Resolve_AssetPath(guid);
            if (!resolvedPath.empty())
                return fs::path(resolvedPath).lexically_normal();
        }

        fs::path resolvedPath = fs::path(String::ToWString(path));
        if (resolvedPath.is_relative())
            resolvedPath = fs::path(GAME->Get_AssetRoot()) / resolvedPath;

        return resolvedPath.lexically_normal();
    }

    bool Apply_ModelFileToMeshData(const wchar_t* payloadPath, MeshTypeData& data)
    {
        if (payloadPath == nullptr || GAME == nullptr)
            return false;

        const fs::path filePath = fs::path(payloadPath).lexically_normal();
        if (String::ToLowerCopy(filePath.extension().string()) != ".model")
            return false;

        data.modelGuid = GAME->Ensure_AssetGUID(filePath.wstring(), "Model");
        data.modelPath = Make_AssetRelativePath(filePath);
        return true;
    }

    bool Apply_EffectMaterialFileToMeshData(const wchar_t* payloadPath, MeshTypeData& data)
    {
        if (payloadPath == nullptr || GAME == nullptr)
            return false;

        const fs::path filePath = fs::path(payloadPath).lexically_normal();
        if (String::ToLowerCopy(filePath.extension().string()) != ".json")
            return false;

        data.assignedEffectMaterialGuid = GAME->Ensure_AssetGUID(filePath.wstring(), "EffectMaterial");
        data.assignedEffectMaterialPath = Make_AssetRelativePath(filePath);
        data.hasAssignedMaterialInstance = EffectMaterialPresetReader::Read(filePath, data.assignedMaterial);
        if (!data.hasAssignedMaterialInstance)
            data.assignedMaterial = {};
        return true;
    }

    bool Try_PickMeshTypeDataAssetDialog(
        const wchar_t* title,
        const wchar_t* filterName,
        const wchar_t* filterSpec,
        const wchar_t* defaultFolder,
        wstring& outFilePath)
    {
        outFilePath.clear();

        const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool shouldUnInit = SUCCEEDED(initHr);

        IFileOpenDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || dialog == nullptr)
        {
            if (shouldUnInit)
                CoUninitialize();
            return false;
        }

        dialog->SetTitle(title);

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST);

        if (GAME != nullptr && defaultFolder != nullptr)
        {
            const fs::path folderPath = fs::path(GAME->Get_AssetRoot()) / defaultFolder;
            IShellItem* folderItem = nullptr;
            if (SUCCEEDED(
                SHCreateItemFromParsingName(folderPath.lexically_normal().wstring().c_str(), nullptr, IID_PPV_ARGS(&folderItem))
            ) && folderItem != nullptr)
            {
                dialog->SetFolder(folderItem);
                folderItem->Release();
            }
        }

        const COMDLG_FILTERSPEC filters[] =
        {
            { filterName, filterSpec },
            { L"All", L"*.*" },
        };
        dialog->SetFileTypes(std::size(filters), filters);

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

        if (shouldUnInit)
            CoUninitialize();

        return !outFilePath.empty();
    }

    string Build_MeshModelValidationMessage(const MeshTypeData& data, ImVec4& outColor)
    {
        outColor = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        if (data.modelGuid.empty() && data.modelPath.empty())
            return "Model asset is not assigned.";

        const fs::path resolvedPath = Resolve_AuthoredAssetPath(data.modelGuid, data.modelPath);
        if (resolvedPath.empty() || !fs::exists(resolvedPath) || !fs::is_regular_file(resolvedPath))
        {
            outColor = ImVec4{ 1.f, 0.55f, 0.25f, 1.f };
            return "Missing model asset: " + String::ToString(resolvedPath.wstring());
        }

        if (String::ToLowerCopy(resolvedPath.extension().string()) != ".model")
        {
            outColor = ImVec4{ 1.f, 0.35f, 0.35f, 1.f };
            return "Unsupported model file extension.";
        }

        if (!data.modelGuid.empty() && GAME != nullptr)
        {
            const AssetMeta* assetMeta = GAME->Find_AssetByGUID(data.modelGuid);
            if (assetMeta == nullptr)
            {
                outColor = ImVec4{ 1.f, 0.55f, 0.25f, 1.f };
                return "Model guid is not registered in the asset registry.";
            }

            if (assetMeta->type != "Model")
            {
                outColor = ImVec4{ 1.f, 0.35f, 0.35f, 1.f };
                return "Assigned guid is not a Model asset.";
            }

            const string modelType = String::ToLowerCopy(assetMeta->modelType);
            if (!modelType.empty() && modelType != "nonanim" && modelType != "staticmesh")
            {
                outColor = ImVec4{ 1.f, 0.55f, 0.25f, 1.f };
                return "Unsupported model type for mesh emitter v1: " + assetMeta->modelType;
            }
        }

        outColor = ImVec4{ 0.45f, 0.9f, 0.55f, 1.f };
        return "Model asset is valid for authoring.";
    }

    bool Draw_MeshAssetDropSlot(
        const char* id,
        const char* title,
        const char* payloadType,
        const string& path,
        const char* emptyText,
        const function<bool(const wchar_t*)>& applyPayload,
        const function<bool()>& pickFile)
    {
        constexpr float browseButtonWidth{ 32.f };
        constexpr float slotHeight = 56.f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const ImGuiPayload* activePayload = ImGui::GetDragDropPayload();
        const bool isMatchingPayload = activePayload != nullptr && activePayload->IsDataType(payloadType);

        ImGui::PushID(id);
        const ImVec2 slotMin = ImGui::GetCursorScreenPos();
        const float slotWidth = max(220.f, ImGui::GetContentRegionAvail().x - browseButtonWidth - spacing);
        ImGui::InvisibleButton("##DropSlot", ImVec2(slotWidth, slotHeight));

        bool changed = false;
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payloadType, ImGuiDragDropFlags_AcceptBeforeDelivery))
            {
                if (payload->IsDelivery())
                    changed = applyPayload(static_cast<const wchar_t*>(payload->Data));
            }
            ImGui::EndDragDropTarget();
        }

        const bool isHovered = ImGui::IsItemHovered();
        const ImVec2 slotMax{ slotMin.x + slotWidth, slotMin.y + slotHeight };
        const ImU32 fillColor = isMatchingPayload ? IM_COL32(38, 56, 48, 255) : IM_COL32(30, 34, 42, 255);
        const ImU32 borderColor = isMatchingPayload
                                  ? IM_COL32(74, 222, 128, 255)
                                  : isHovered ? IM_COL32(120, 130, 150, 255) : IM_COL32(70, 78, 92, 255);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(slotMin, slotMax, fillColor, 6.f);
        drawList->AddRect(slotMin, slotMax, borderColor, 6.f, 0, isMatchingPayload ? 2.5f : 1.5f);
        drawList->AddText(ImVec2(slotMin.x + 10.f, slotMin.y + 8.f), IM_COL32(235, 238, 245, 255), title);

        const string displayPath = path.empty() ? emptyText : path;
        drawList->PushClipRect(ImVec2(slotMin.x + 10.f, slotMin.y + 28.f), ImVec2(slotMax.x - 10.f, slotMax.y), true);
        drawList->AddText(ImVec2(slotMin.x + 10.f, slotMin.y + 30.f), IM_COL32(150, 160, 176, 255), displayPath.c_str());
        drawList->PopClipRect();

        if (isHovered)
            ImGui::SetTooltip("%s", path.empty() ? emptyText : path.c_str());

        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2{ 0.5f, 0.5f });
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 0.f, 0.f });
        if (ImGui::Button(ICON_FA_FOLDER_OPEN "##PickAsset", ImVec2{ browseButtonWidth, slotHeight }))
            changed |= pickFile();
        ImGui::PopStyleVar(2);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("파일 탐색기에서 선택");

        ImGui::PopID();
        return changed;
    }

    bool Has_PostProcessFeature(PostProcessFeature features, PostProcessFeature feature)
    {
        return Has_Flag(features, feature);
    }

    const char* Get_PostProcessPresetLabel(EffectEditorPostProcessPreset preset)
    {
        switch (preset)
        {
        case EffectEditorPostProcessPreset::PreviewFriendly:
            return "Preview Friendly";
        case EffectEditorPostProcessPreset::ClientCurrent:
            return "Client Current";
        case EffectEditorPostProcessPreset::Custom:
        default:
            return "Custom";
        }
    }

    const char* Get_ToneMapModeLabel(uint32 toneMapMode)
    {
        switch (toneMapMode)
        {
        case 0:
            return "Reinhard";
        case 1:
            return "ACES";
        case 2:
        default:
            return "Uncharted2";
        }
    }

    void Draw_PostProcessPresetProperty(DetailPropertyContext& detailContext)
    {
        if (EDITOR == nullptr)
            return;

        const EffectEditorPostProcessPreset currentPreset = EDITOR->Get_PostProcessPreset();

        ImGui::PushID("PostProcessPreset");
        detailContext.Draw_PropertyLabel("Preset");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_PostProcessPresetLabel(currentPreset)))
        {
            const bool previewSelected = currentPreset == EffectEditorPostProcessPreset::PreviewFriendly;
            if (ImGui::Selectable("Preview Friendly", previewSelected))
                EDITOR->Apply_PostProcessOptions(EDITOR->Get_DefaultPostProcessOptions());

            if (previewSelected)
                ImGui::SetItemDefaultFocus();

            if (EDITOR->Has_ClientPostProcessSnapshot())
            {
                const bool clientSelected = currentPreset == EffectEditorPostProcessPreset::ClientCurrent;
                if (ImGui::Selectable("Client Current", clientSelected))
                    EDITOR->Apply_PostProcessOptions(EDITOR->Get_ClientPostProcessSnapshot());

                if (clientSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        detailContext.Draw_ResetButton(false);
        ImGui::PopID();
    }

    bool Draw_ToneMapModeProperty(DetailPropertyContext& detailContext, uint32& toneMapMode, uint32 defaultToneMapMode)
    {
        static constexpr uint32 kToneMapModes[] = { 0u, 1u, 2u };

        ImGui::PushID("ToneMapMode");
        detailContext.Draw_PropertyLabel("ToneMap Mode");
        ImGui::SetNextItemWidth(-FLT_MIN);

        bool changed = false;
        if (ImGui::BeginCombo("##Value", Get_ToneMapModeLabel(toneMapMode)))
        {
            for (const uint32 candidate : kToneMapModes)
            {
                const bool selected = toneMapMode == candidate;
                if (ImGui::Selectable(Get_ToneMapModeLabel(candidate), selected))
                {
                    toneMapMode = candidate;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        const bool resetClicked = detailContext.Draw_ResetButton(toneMapMode != defaultToneMapMode);
        ImGui::PopID();

        if (resetClicked)
        {
            toneMapMode = defaultToneMapMode;
            return true;
        }

        return changed;
    }

    bool Draw_TrailCurveQualityProperty(DetailPropertyContext& detailContext, TrailTypeData& data, const TrailTypeData& defaultData)
    {
        bool changed = false;

        ImGui::PushID("TrailCurveQuality");
        detailContext.Draw_PropertyLabel("곡선 품질", "트레일 곡선 샘플 밀도와 가장자리 페이드 preset입니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_TrailCurveQualityLabel(data.curveQuality)))
        {
            for (const TrailCurveQuality value : kTrailCurveQualityValues)
            {
                const bool selected = data.curveQuality == value;
                if (ImGui::Selectable(Get_TrailCurveQualityLabel(value), selected))
                {
                    data.curveQuality = value;
                    changed = true;
                }

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("%s", Get_TrailCurveQualityTooltip(value));

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(data.curveQuality != defaultData.curveQuality))
        {
            data.curveQuality = defaultData.curveQuality;
            changed = true;
        }
        ImGui::PopID();

        if (changed)
            changed |= Apply_TrailCurvePreset(data);

        return changed;
    }
}

Detail_View::Detail_View()
    : Editor_Window{ L"Detail", ICON_FA_PEN_TO_SQUARE }
{
}

Detail_View::~Detail_View()
{
    Free();
}

void Detail_View::Update(float timeDelta)
{
    __super::Update(timeDelta);
    _detailTimeDelta = max(0.f, timeDelta);
}

void Detail_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string& windowName = Get_ImGuiWindowName();

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        Render_EffectAuthoringDetail();
    }

    Set_Open(isOpen);
    ImGui::End();
}

void Detail_View::Render_EffectAuthoringDetail()
{
    if (EDITOR == nullptr || EDITOR->Get_EditorContext() == nullptr)
    {
        ImGui::TextDisabled("Editor context is not available.");
        return;
    }

    const EffectAuthoringSelection& selection = EDITOR->Get_EditorContext()->Get_EffectSelection();
    const Shared<Editor_Window> emitterWindow = EDITOR->Get_Window(L"Emitter");
    const Shared<Emitter_View> emitterView = dynamic_pointer_cast<Emitter_View>(emitterWindow);
    if (emitterView == nullptr)
    {
        ImGui::TextDisabled("Emitter view is not available.");
        return;
    }

    Commit_PendingAuthoringEditOnTargetChange(emitterView, selection);

    if (selection.kind == EffectAuthoringSelectionKind::None)
    {
        Render_ParticleSystemDetail(emitterView, selection, emitterView->Get_ParticleSystemData());
        return;
    }

    AuthoringEmitter* emitter = emitterView->Find_Emitter(selection.emitterId);
    if (nullptr == emitter)
    {
        Commit_PendingAuthoringEdit(emitterView);
        ImGui::TextDisabled("Selected emitter is not available.");
        return;
    }

    if (selection.kind == EffectAuthoringSelectionKind::Emitter)
    {
        Render_EmitterDetail(emitterView, selection, *emitter);
        return;
    }

    if (selection.kind == EffectAuthoringSelectionKind::TypeData)
    {
        Render_TypeDataDetail(emitterView, selection, *emitter);
        return;
    }

    AuthoringModule* module = emitterView->Find_Module(selection.emitterId, selection.moduleId);
    if (nullptr == module)
    {
        Commit_PendingAuthoringEdit(emitterView);
        ImGui::TextDisabled("Selected module is not available.");
        return;
    }

    Render_ModuleDetail(emitterView, selection, *emitter, *module);
}

void Detail_View::Render_ParticleSystemDetail(
    const Shared<Emitter_View>& emitterView,
    const EffectAuthoringSelection& selection,
    ParticleSystemAuthoringData& data)
{
    ImGui::TextUnformatted("파티클 시스템");
    ImGui::Separator();

    const Emitter_View::AuthoringSnapshot beforeSnapshot = emitterView->Capture_AuthoringSnapshot();
    const ParticleSystemAuthoringData defaultData{};
    bool changed = false;
    if (_detailPropertyContext.Begin_PropertyTable("ParticleSystem"))
    {
        changed |= _detailPropertyContext.Draw_BoolProperty("자동 비활성화", data.autoDeactivate, defaultData.autoDeactivate);
        _detailPropertyContext.End_PropertyTable();
    }
    if (changed)
    {
        Begin_PendingAuthoringEdit(emitterView, selection, beforeSnapshot, "Edit Particle System Detail");
        emitterView->MarkDirty();
        MarkDirty();
    }

    Render_TrailPreviewDetail();
    Render_PostProcessShaderDetail();
    Commit_PendingAuthoringEditIfIdle(emitterView);
}

void Detail_View::Render_TrailPreviewDetail()
{
    if (EDITOR == nullptr)
        return;

    if (!ImGui::CollapsingHeader("트레일 프리뷰", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    Vec3 baseLocalOffset = EDITOR->Get_TrailPreviewBaseLocalOffset();
    Vec3 tipLocalOffset = EDITOR->Get_TrailPreviewTipLocalOffset();
    const Vec3 defaultBaseLocalOffset = EDITOR->Get_DefaultTrailPreviewBaseLocalOffset();
    const Vec3 defaultTipLocalOffset = EDITOR->Get_DefaultTrailPreviewTipLocalOffset();
    bool visible = EDITOR->Is_TrailPreviewVisible();
    bool debugRenderEnabled = EDITOR->Is_TrailPreviewDebugRenderEnabled();

    bool changed = false;
    if (_detailPropertyContext.Begin_PropertyTable("TrailPreview"))
    {
        if (_detailPropertyContext.Draw_BoolProperty("트레일 프리뷰", visible, false))
            EDITOR->Set_TrailPreviewVisible(visible);
        if (_detailPropertyContext.Draw_BoolProperty("디버그 렌더", debugRenderEnabled, true))
            EDITOR->Set_TrailPreviewDebugRenderEnabled(debugRenderEnabled);
        changed |= _detailPropertyContext.Draw_Vec3Property("Base 로컬", baseLocalOffset, defaultBaseLocalOffset, 0.005f);
        changed |= _detailPropertyContext.Draw_Vec3Property("Tip 로컬", tipLocalOffset, defaultTipLocalOffset, 0.005f);
        _detailPropertyContext.End_PropertyTable();
    }

    if (changed)
        EDITOR->Set_TrailPreviewLocalOffsets(baseLocalOffset, tipLocalOffset);
}

void Detail_View::Render_PostProcessShaderDetail()
{
    if (EDITOR == nullptr)
        return;

    if (!ImGui::CollapsingHeader("후처리 셰이더", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    const EffectEditorPostProcessOptions options = EDITOR->Get_PostProcessOptions();
    const EffectEditorPostProcessOptions defaultOptions = EDITOR->Get_DefaultPostProcessOptions();

    bool hdr = Has_PostProcessFeature(options.features, PostProcessFeature::HDR);
    bool exposure = Has_PostProcessFeature(options.features, PostProcessFeature::Exposure);
    bool toneMapping = Has_PostProcessFeature(options.features, PostProcessFeature::ToneMapping);
    bool bloom = Has_PostProcessFeature(options.features, PostProcessFeature::Bloom);
    bool gamma = Has_PostProcessFeature(options.features, PostProcessFeature::Gamma);

    const bool defaultHdr = Has_PostProcessFeature(defaultOptions.features, PostProcessFeature::HDR);
    const bool defaultExposure = Has_PostProcessFeature(defaultOptions.features, PostProcessFeature::Exposure);
    const bool defaultToneMapping = Has_PostProcessFeature(defaultOptions.features, PostProcessFeature::ToneMapping);
    const bool defaultBloom = Has_PostProcessFeature(defaultOptions.features, PostProcessFeature::Bloom);
    const bool defaultGamma = Has_PostProcessFeature(defaultOptions.features, PostProcessFeature::Gamma);

    if (_detailPropertyContext.Begin_PropertyTable("PostProcessPreset"))
    {
        Draw_PostProcessPresetProperty(_detailPropertyContext);
        _detailPropertyContext.End_PropertyTable();
    }

    if (_detailPropertyContext.Begin_PropertyTable("PostProcessFeatures"))
    {
        if (_detailPropertyContext.Draw_BoolProperty("HDR", hdr, defaultHdr))
            EDITOR->Set_PostProcessFeature(PostProcessFeature::HDR, hdr);
        if (_detailPropertyContext.Draw_BoolProperty("Exposure", exposure, defaultExposure))
            EDITOR->Set_PostProcessFeature(PostProcessFeature::Exposure, exposure);
        if (_detailPropertyContext.Draw_BoolProperty("ToneMapping", toneMapping, defaultToneMapping))
            EDITOR->Set_PostProcessFeature(PostProcessFeature::ToneMapping, toneMapping);
        if (_detailPropertyContext.Draw_BoolProperty("Bloom", bloom, defaultBloom))
            EDITOR->Set_PostProcessFeature(PostProcessFeature::Bloom, bloom);
        if (_detailPropertyContext.Draw_BoolProperty("Gamma", gamma, defaultGamma))
            EDITOR->Set_PostProcessFeature(PostProcessFeature::Gamma, gamma);
        _detailPropertyContext.End_PropertyTable();
    }

    PostProcessCB params = EDITOR->Get_PostProcessOptions().params;
    bool paramsChanged = false;
    if (_detailPropertyContext.Begin_PropertyTable("PostProcessParams"))
    {
        paramsChanged |= _detailPropertyContext.Draw_FloatProperty("Exposure", params.exposure, defaultOptions.params.exposure);
        paramsChanged |= _detailPropertyContext.Draw_FloatProperty("Inv White Point", params.invWhitePoint, defaultOptions.params.invWhitePoint);
        paramsChanged |= _detailPropertyContext.Draw_FloatProperty("Gamma", params.gamma, defaultOptions.params.gamma);
        paramsChanged |= _detailPropertyContext.Draw_FloatProperty("Bloom Radius", params.bloomRadius, defaultOptions.params.bloomRadius);
        paramsChanged |= _detailPropertyContext.Draw_FloatProperty("Bloom Threshold", params.bloomThreshold, defaultOptions.params.bloomThreshold);
        paramsChanged |= _detailPropertyContext.Draw_FloatProperty("Bloom Intensity", params.bloomIntensity, defaultOptions.params.bloomIntensity);
        paramsChanged |= _detailPropertyContext.Draw_FloatProperty("Bloom Soft Knee", params.bloomSoftKnee, defaultOptions.params.bloomSoftKnee);
        paramsChanged |= _detailPropertyContext.Draw_FloatProperty("Bloom Clamp", params.bloomClamp, defaultOptions.params.bloomClamp, 0.05f);
        paramsChanged |= Draw_ToneMapModeProperty(_detailPropertyContext, params.toneMapMode, defaultOptions.params.toneMapMode);
        _detailPropertyContext.End_PropertyTable();
    }

    if (paramsChanged)
        EDITOR->Set_PostProcessParams(params);
}

void Detail_View::Render_EmitterDetail(
    const Shared<Emitter_View>& emitterView,
    const EffectAuthoringSelection& selection,
    AuthoringEmitter& emitter)
{
    ImGui::TextUnformatted("파티클");
    ImGui::Separator();

    const Emitter_View::AuthoringSnapshot beforeSnapshot = emitterView->Capture_AuthoringSnapshot();
    const AuthoringEmitter defaultData{};
    bool changed = false;

    if (_detailPropertyContext.Begin_PropertyTable("Emitter"))
    {
        changed |= _detailPropertyContext.Draw_StringProperty("이미터 이름", emitter.name, defaultData.name);

        ImGui::PushID("RendererType");
        _detailPropertyContext.Draw_PropertyLabel("렌더러 타입");
        ImGui::TextDisabled("%s", emitter.rendererType.c_str());
        _detailPropertyContext.Draw_ResetButton(false);
        ImGui::PopID();

        changed |= Draw_RenderLayerOverrideProperty(
            _detailPropertyContext,
            "렌더 레이어",
            emitter.renderLayerOverride,
            defaultData.renderLayerOverride
        );

        _detailPropertyContext.End_PropertyTable();
    }

    if (changed)
        Begin_PendingAuthoringEdit(emitterView, selection, beforeSnapshot, "Edit Emitter Detail");

    Mark_Changed(emitterView, emitter, changed);
    Commit_PendingAuthoringEditIfIdle(emitterView);
}

void Detail_View::Render_TypeDataDetail(
    const Shared<Emitter_View>& emitterView,
    const EffectAuthoringSelection& selection,
    AuthoringEmitter& emitter)
{
    ImGui::TextUnformatted("타입 데이터");
    ImGui::Separator();

    bool changed = false;
    const Emitter_View::AuthoringSnapshot beforeSnapshot = emitterView->Capture_AuthoringSnapshot();
    switch (emitter.typeData.kind)
    {
    case AuthoringTypeDataKind::Trail:
    {
        TrailTypeData* data = get_if<TrailTypeData>(&emitter.typeData.payload);
        if (data == nullptr)
        {
            ImGui::TextDisabled("Trail TypeData payload is invalid.");
            break;
        }

        const TrailTypeData defaultData{};
        if (ImGui::CollapsingHeader("기본", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (_detailPropertyContext.Begin_PropertyTable("TrailTypeDataBase"))
            {
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "폭 비율",
                    data->width,
                    defaultData.width,
                    0.01f,
                    "Base/Tip 사이 길이에 곱할 트레일 strip 폭 비율입니다. 1이면 원본 폭을 유지합니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "세그먼트 수명",
                    data->segmentLifetime,
                    defaultData.segmentLifetime,
                    0.01f,
                    "trail sample이 유지되는 시간입니다. 길수록 잔상이 오래 남습니다."
                );
                changed |= _detailPropertyContext.Draw_UintProperty(
                    "히스토리 수",
                    data->historyCount,
                    defaultData.historyCount,
                    "저장할 trail sample 개수입니다. 많을수록 긴 궤적을 유지할 수 있습니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "UV 타일링",
                    data->uvTiling,
                    defaultData.uvTiling,
                    0.01f,
                    "trail noise/breakup texture의 길이 방향 반복 배율입니다. Main/Mask body는 표시 길이에 맞춰 stretch됩니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "최대 표시 길이",
                    data->maxTrailLength,
                    defaultData.maxTrailLength,
                    0.01f,
                    "head 기준 누적 거리로 제한하는 최대 표시 길이입니다. 0 이하면 제한하지 않습니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "꼬리 페이드 길이",
                    data->tailFadeLength,
                    defaultData.tailFadeLength,
                    0.01f,
                    "최대 표시 길이 끝으로 갈수록 alpha를 줄이는 거리입니다."
                );
                _detailPropertyContext.End_PropertyTable();
            }
        }

        if (ImGui::CollapsingHeader("곡선 품질", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (_detailPropertyContext.Begin_PropertyTable("TrailTypeDataCurveQuality"))
            {
                changed |= Draw_TrailCurveQualityProperty(_detailPropertyContext, *data, defaultData);
                changed |= Apply_TrailCurvePreset(*data);

                const bool presetLocked = data->curveQuality != TrailCurveQuality::Custom;
                if (presetLocked)
                    ImGui::BeginDisabled();

                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "샘플 간격",
                    data->sampleSpacing,
                    defaultData.sampleSpacing,
                    0.005f,
                    "이동 거리 기준으로 중간 trail sample을 삽입하는 간격입니다. 낮을수록 곡선이 촘촘해집니다."
                );
                changed |= _detailPropertyContext.Draw_UintProperty(
                    "곡선 분할",
                    data->curveSubdivision,
                    defaultData.curveSubdivision,
                    "한 frame 안에서 보강할 수 있는 최대 중간 sample 수입니다."
                );
                changed |= _detailPropertyContext.Draw_BoolProperty(
                    "탄젠트 스무딩",
                    data->smoothTangent,
                    defaultData.smoothTangent,
                    "앞뒤 sample을 함께 보아 base/tip 위치를 완만하게 평균 보정합니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "폭 가장자리 페이드",
                    data->sideFade,
                    defaultData.sideFade,
                    0.005f,
                    "trail 폭 방향 양끝 alpha를 줄이는 강도입니다. 높일수록 두꺼운 벽 느낌이 줄어듭니다."
                );

                if (presetLocked)
                    ImGui::EndDisabled();

                _detailPropertyContext.End_PropertyTable();
            }
        }
        break;
    }
    case AuthoringTypeDataKind::Mesh:
    {
        const MeshTypeData* data = get_if<MeshTypeData>(&emitter.typeData.payload);
        if (data == nullptr)
        {
            ImGui::TextDisabled("Mesh TypeData payload is invalid.");
            break;
        }

        ImGui::TextDisabled("MeshData editing is handled in the dedicated preview window.");
        if (_detailPropertyContext.Begin_PropertyTable("MeshTypeDataCompactSummary"))
        {
            _detailPropertyContext.Draw_PropertyLabel("Model Asset");
            ImGui::TextWrapped("%s", data->modelPath.empty() ? "(not assigned)" : data->modelPath.c_str());
            _detailPropertyContext.Draw_ResetButton(false);

            _detailPropertyContext.Draw_PropertyLabel("Effect Material");
            ImGui::TextWrapped("%s", data->assignedEffectMaterialPath.empty() ? "(not assigned)" : data->assignedEffectMaterialPath.c_str());
            _detailPropertyContext.Draw_ResetButton(false);

            _detailPropertyContext.Draw_PropertyLabel("Preview Scale");
            ImGui::Text("%.3f, %.3f, %.3f", data->previewScale.x, data->previewScale.y, data->previewScale.z);
            _detailPropertyContext.Draw_ResetButton(false);

            _detailPropertyContext.End_PropertyTable();
        }

        ImVec4 validationColor{};
        const string validationMessage = Build_MeshModelValidationMessage(*data, validationColor);
        ImGui::PushStyleColor(ImGuiCol_Text, validationColor);
        ImGui::TextWrapped("%s", validationMessage.c_str());
        ImGui::PopStyleColor();

        if (ImGui::Button(ICON_FA_CUBES " Preview / Edit"))
        {
            const Shared<Editor_Window> meshPreviewWindow = EDITOR != nullptr ? EDITOR->Get_Window(L"MeshData Preview") : nullptr;
            const Shared<MeshDataPreview_View> meshPreviewView = dynamic_pointer_cast<MeshDataPreview_View>(meshPreviewWindow);
            if (meshPreviewView != nullptr)
                meshPreviewView->Open_Target(emitter.id);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("MeshData의 model/material 조합과 내부 material instance 값을 편집합니다.");

        break;
    }
    case AuthoringTypeDataKind::None:
    default:
        ImGui::TextDisabled("장착된 타입 데이터가 없습니다.");
        break;
    }

    if (changed)
        Begin_PendingAuthoringEdit(emitterView, selection, beforeSnapshot, "Edit TypeData Detail");

    Mark_Changed(emitterView, emitter, changed);
    Commit_PendingAuthoringEditIfIdle(emitterView);
}

void Detail_View::Render_ModuleDetail(
    const Shared<Emitter_View>& emitterView,
    const EffectAuthoringSelection& selection,
    AuthoringEmitter& emitter,
    AuthoringModule& module)
{
    ImGui::TextUnformatted(String::ToString(module.displayName).c_str());
    ImGui::Separator();

    const Emitter_View::AuthoringSnapshot beforeSnapshot = emitterView->Capture_AuthoringSnapshot();
    bool changed = false;

    switch (module.type)
    {
    case AuthoringModuleType::Required:
        if (auto* data = get_if<RequiredModuleData>(&module.data))
            changed = _requiredModuleDetail.Draw(emitter, module, *data, _detailPropertyContext, _detailTimeDelta);
        break;

    case AuthoringModuleType::Spawn:
        if (auto* data = get_if<SpawnModuleData>(&module.data))
            changed = _spawnModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::Lifetime:
        if (auto* data = get_if<LifetimeModuleData>(&module.data))
            changed = _lifetimeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialSize:
        if (auto* data = get_if<InitialSizeModuleData>(&module.data))
            changed = _initialSizeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialMeshSize:
        if (auto* data = get_if<InitialMeshSizeModuleData>(&module.data))
            changed = _initialMeshSizeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialLocation:
        if (auto* data = get_if<InitialLocationModuleData>(&module.data))
            changed = _initialLocationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::SphereLocation:
        if (auto* data = get_if<SphereLocationModuleData>(&module.data))
            changed = _sphereLocationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::PlaneRadialLocation:
        if (auto* data = get_if<PlaneRadialLocationModuleData>(&module.data))
            changed = _planeRadialLocationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialVelocity:
        if (auto* data = get_if<InitialVelocityModuleData>(&module.data))
            changed = _initialVelocityModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialRadialVelocity:
        if (auto* data = get_if<InitialRadialVelocityModuleData>(&module.data))
            changed = _initialRadialVelocityModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::VelocityCone:
        if (auto* data = get_if<VelocityConeModuleData>(&module.data))
            changed = _velocityConeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::Acceleration:
        if (auto* data = get_if<AccelerationModuleData>(&module.data))
            changed = _accelerationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::Drag:
        if (auto* data = get_if<DragModuleData>(&module.data))
            changed = _dragModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::VelocityOverLife:
        if (auto* data = get_if<VelocityOverLifeModuleData>(&module.data))
            changed = _velocityOverLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialRotation:
        if (auto* data = get_if<InitialRotationModuleData>(&module.data))
            changed = _initialRotationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::PlaneRadialOrientation:
        if (auto* data = get_if<PlaneRadialOrientationModuleData>(&module.data))
            changed = _planeRadialOrientationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::RotationOverLife:
        if (auto* data = get_if<RotationOverLifeModuleData>(&module.data))
            changed = _rotationOverLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialRotationRate:
        if (auto* data = get_if<InitialRotationRateModuleData>(&module.data))
            changed = _initialRotationRateModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::RotationRateScaleByLife:
        if (auto* data = get_if<RotationRateScaleByLifeModuleData>(&module.data))
            changed = _rotationRateScaleByLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialMeshRotation:
        if (auto* data = get_if<InitialMeshRotationModuleData>(&module.data))
            changed = _initialMeshRotationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::MeshRotationOverLife:
        if (auto* data = get_if<MeshRotationOverLifeModuleData>(&module.data))
            changed = _meshRotationOverLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialMeshRotationRate:
        if (auto* data = get_if<InitialMeshRotationRateModuleData>(&module.data))
            changed = _initialMeshRotationRateModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::MeshRotationRateScaleByLife:
        if (auto* data = get_if<MeshRotationRateScaleByLifeModuleData>(&module.data))
            changed = _meshRotationRateScaleByLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialColor:
        if (auto* data = get_if<InitialColorModuleData>(&module.data))
            changed = _initialColorModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::ColorOverLife:
        if (auto* data = get_if<ColorOverLifeModuleData>(&module.data))
            changed = _colorOverLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::SubUVFrameOverLife:
        if (auto* data = get_if<SubUVFrameOverLifeModuleData>(&module.data))
            changed = _subUvFrameOverLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::SizeByLife:
        if (auto* data = get_if<SizeByLifeModuleData>(&module.data))
            changed = _sizeByLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::MeshSizeByLife:
        if (auto* data = get_if<MeshSizeByLifeModuleData>(&module.data))
            changed = _meshSizeByLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::SpawnPerUnit:
        if (auto* data = get_if<SpawnPerUnitModuleData>(&module.data))
            changed = _spawnPerUnitModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::MaterialScalarModulation:
        if (auto* data = get_if<MaterialScalarModulationModuleData>(&module.data))
            changed = _materialScalarModulationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    default:
        ImGui::TextDisabled("Unsupported module type.");
        break;
    }

    if (changed)
        Begin_PendingAuthoringEdit(emitterView, selection, beforeSnapshot, "Edit Module Detail");

    Mark_Changed(emitterView, emitter, changed);
    Commit_PendingAuthoringEditIfIdle(emitterView);
}

void Detail_View::Mark_Changed(const Shared<Emitter_View>& emitterView, AuthoringEmitter& emitter, bool changed)
{
    if (!changed)
        return;

    emitter.previewDirty = true;
    if (emitterView)
        emitterView->MarkDirty();
    MarkDirty();
}

void Detail_View::Commit_PendingAuthoringEditOnTargetChange(
    const Shared<Emitter_View>& emitterView,
    const EffectAuthoringSelection& selection)
{
    if (!_hasPendingAuthoringEdit || Is_SameSelection(_pendingAuthoringSelection, selection))
        return;

    Commit_PendingAuthoringEdit(emitterView);
}

void Detail_View::Begin_PendingAuthoringEdit(
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

void Detail_View::Commit_PendingAuthoringEditIfIdle(const Shared<Emitter_View>& emitterView)
{
    if (!_hasPendingAuthoringEdit || ImGui::IsAnyItemActive())
        return;

    Commit_PendingAuthoringEdit(emitterView);
}

void Detail_View::Commit_PendingAuthoringEdit(const Shared<Emitter_View>& emitterView)
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

bool Detail_View::Is_SameSelection(const EffectAuthoringSelection& lhs, const EffectAuthoringSelection& rhs)
{
    return
        lhs.kind == rhs.kind &&
        lhs.emitterId == rhs.emitterId &&
        lhs.moduleId == rhs.moduleId;
}

Shared<Detail_View> Detail_View::Create()
{
    return make_shared<Detail_View>();
}

void Detail_View::Free()
{
    __super::Free();
}

NS_END
