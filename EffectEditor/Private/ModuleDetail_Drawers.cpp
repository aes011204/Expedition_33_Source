#include "ModuleDetail_Drawers.h"

#include "EffectEditorInstance.h"
#include "EffectEditor_Macro.h"
#include "EffectMaterialInstance_View.h"
#include "EffectMaterialPreviewRenderer.h"
#include "EffectMaterialScalarModulationPreview.h"
#include "EffectMaterial_View.h"
#include "EffectMaterialPresetReader.h"
#include "Helper_EffectAuthoring.h"
#include "Notification_Manager.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    constexpr const char* kEffectMaterialPayloadType = "CONTENT_BROWSER_EFFECT_MATERIAL";
    constexpr const wchar_t* kMaterialDialogDefaultFolder = L"../../../Client/Bin/Resources/Effects/Materials";
    constexpr float kMaterialFloatEpsilon = 0.0001f;

    bool Uses_DirectionalAlignmentOptions(EmitterScreenAlignment alignment)
    {
        return alignment == EmitterScreenAlignment::AwayFromCenter ||
               alignment == EmitterScreenAlignment::Velocity;
    }

    void Notify_DirectionalAlignmentModeChanged(EmitterScreenAlignment alignment, EmitterDirectionalAlignmentMode mode)
    {
        if (!Uses_DirectionalAlignmentOptions(alignment))
            return;

        const char* directionName = alignment == EmitterScreenAlignment::AwayFromCenter ? "Away from Center" : "Velocity";
        const char* modeHelp = mode == EmitterDirectionalAlignmentMode::LookDirection
                               ? "기준 방향을 sprite 카드의 바라보는 방향으로 사용합니다."
                               : "texture 축을 기준 방향에 맞추고 나머지 축은 카메라 가시성을 유지합니다.";

        NOTIFY("{}: {}", directionName, modeHelp);
    }

    void Notify_TextureAxisChanged(EmitterSpriteTextureAxis axis)
    {
        NOTIFY(
            "{}",
            axis == EmitterSpriteTextureAxis::X
            ? "Texture X: 정렬 방향에 texture의 가로축을 맞춥니다."
            : "Texture Y: 정렬 방향에 texture의 세로축을 맞춥니다."
        );
    }

    fs::path Normalize_MaterialDialogPath(const fs::path& filePath)
    {
        try
        {
            if (fs::exists(filePath))
                return fs::weakly_canonical(filePath);
        }
        catch (...)
        {}

        try
        {
            return fs::absolute(filePath).lexically_normal();
        }
        catch (...)
        {}

        return filePath.lexically_normal();
    }

    static bool NearlyEqual(float lhs, float rhs)
    {
        return abs(lhs - rhs) <= kMaterialFloatEpsilon;
    }

    static bool Is_SameColor(const Color& lhs, const Color& rhs)
    {
        return
            NearlyEqual(lhs.x, rhs.x) &&
            NearlyEqual(lhs.y, rhs.y) &&
            NearlyEqual(lhs.z, rhs.z) &&
            NearlyEqual(lhs.w, rhs.w);
    }

    static bool Is_SameAdditiveContribution(
        const EffectMaterialAdditiveContributionData& lhs,
        const EffectMaterialAdditiveContributionData& rhs)
    {
        return
            lhs.colorSource == rhs.colorSource &&
            lhs.amountSource == rhs.amountSource &&
            lhs.coveragePolicy == rhs.coveragePolicy &&
            NearlyEqual(lhs.intensityScale, rhs.intensityScale) &&
            lhs.blackNeutral == rhs.blackNeutral &&
            Is_SameColor(lhs.emissiveColor, rhs.emissiveColor) &&
            Is_SameColor(lhs.constantColor, rhs.constantColor);
    }

    static bool Is_SameVec2(const Vec2& lhs, const Vec2& rhs)
    {
        return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y);
    }

    static const char* Get_SubUVFramePlaybackModeLabel(SubUVFramePlaybackMode mode)
    {
        switch (mode)
        {
        case SubUVFramePlaybackMode::FixedFrame:
            return "고정 프레임";
        case SubUVFramePlaybackMode::LifeProgress:
            return "수명 맞춤 재생";
        case SubUVFramePlaybackMode::FramesPerSecond:
            return "초당 프레임 재생";
        case SubUVFramePlaybackMode::RandomFrame:
            return "랜덤 프레임";
        default:
            return "고정 프레임";
        }
    }

    static const char* Get_SphereLocationSpawnModeLabel(SphereLocationSpawnMode mode)
    {
        switch (mode)
        {
        case SphereLocationSpawnMode::Volume:
            return "부피";
        case SphereLocationSpawnMode::Surface:
            return "표면";
        default:
            return "부피";
        }
    }

    static const char* Get_MaterialScalarTargetLabel(MaterialScalarModulationTargetField target)
    {
        switch (target)
        {
        case MaterialScalarModulationTargetField::Intensity:
            return "Intensity";
        case MaterialScalarModulationTargetField::OpacityPower:
            return "OpacityPower";
        case MaterialScalarModulationTargetField::NoiseStrength:
            return "NoiseStrength";
        case MaterialScalarModulationTargetField::AlphaErosion:
            return "AlphaErosion";
        case MaterialScalarModulationTargetField::AlphaCutoff:
            return "AlphaCutoff";
        case MaterialScalarModulationTargetField::CoreIntensity:
            return "CoreIntensity";
        case MaterialScalarModulationTargetField::OuterIntensity:
            return "OuterIntensity";
        case MaterialScalarModulationTargetField::CoreColor:
            return "CoreColor Multiplier";
        default:
            return "Intensity";
        }
    }

    static const char* Get_MaterialModulationTargetLabel(bool isCoreColorRgb, MaterialScalarModulationTargetField target)
    {
        return isCoreColorRgb ? "Core Color RGB" : Get_MaterialScalarTargetLabel(target);
    }

    static const char* Get_MaterialScalarOperationLabel(MaterialScalarModulationOperation operation)
    {
        switch (operation)
        {
        case MaterialScalarModulationOperation::Multiply:
            return "Multiply";
        default:
            return "Multiply";
        }
    }

    static const char* Get_MaterialScalarTimeSourceLabel(MaterialScalarModulationTimeSource timeSource)
    {
        switch (timeSource)
        {
        case MaterialScalarModulationTimeSource::ParticleLife:
            return "ParticleLife (미구현)";
        case MaterialScalarModulationTimeSource::EmitterTime:
            return "EmitterTime";
        default:
            return "EmitterTime";
        }
    }

    static bool Is_MaterialScalarTimeSourceSelectable(MaterialScalarModulationTimeSource timeSource)
    {
        return timeSource != MaterialScalarModulationTimeSource::ParticleLife;
    }

    static bool Draw_MaterialModulationTargetProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        bool isCoreColorRgb,
        MaterialScalarModulationTargetField scalarTarget,
        bool& outNextCoreColorRgb,
        MaterialScalarModulationTargetField& outNextScalarTarget)
    {
        bool changed = false;
        outNextCoreColorRgb = isCoreColorRgb;
        outNextScalarTarget = scalarTarget;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_MaterialModulationTargetLabel(isCoreColorRgb, scalarTarget)))
        {
            const MaterialScalarModulationTargetField candidates[] = {
                MaterialScalarModulationTargetField::Intensity,
                MaterialScalarModulationTargetField::OpacityPower,
                MaterialScalarModulationTargetField::NoiseStrength,
                MaterialScalarModulationTargetField::AlphaErosion,
                MaterialScalarModulationTargetField::AlphaCutoff,
                MaterialScalarModulationTargetField::CoreIntensity,
                MaterialScalarModulationTargetField::OuterIntensity,
                MaterialScalarModulationTargetField::CoreColor,
            };

            for (const MaterialScalarModulationTargetField candidate : candidates)
            {
                const bool isSelected = !isCoreColorRgb && scalarTarget == candidate;
                if (ImGui::Selectable(Get_MaterialScalarTargetLabel(candidate), isSelected))
                {
                    outNextCoreColorRgb = false;
                    outNextScalarTarget = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            const bool isRgbSelected = isCoreColorRgb;
            if (ImGui::Selectable("Core Color RGB", isRgbSelected))
            {
                outNextCoreColorRgb = true;
                outNextScalarTarget = MaterialScalarModulationTargetField::CoreColor;
                changed = true;
            }

            if (isRgbSelected)
                ImGui::SetItemDefaultFocus();

            ImGui::EndCombo();
        }

        const bool canReset = isCoreColorRgb || scalarTarget != MaterialScalarModulationTargetField::Intensity;
        if (detailContext.Draw_ResetButton(canReset))
        {
            outNextCoreColorRgb = false;
            outNextScalarTarget = MaterialScalarModulationTargetField::Intensity;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static bool Draw_MaterialScalarTimeSourceProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        MaterialScalarModulationTimeSource& value,
        MaterialScalarModulationTimeSource defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_MaterialScalarTimeSourceLabel(value)))
        {
            const MaterialScalarModulationTimeSource candidates[] = {
                MaterialScalarModulationTimeSource::ParticleLife,
                MaterialScalarModulationTimeSource::EmitterTime,
            };

            for (const MaterialScalarModulationTimeSource candidate : candidates)
            {
                const bool isSelected = value == candidate;
                const bool selectable = Is_MaterialScalarTimeSourceSelectable(candidate);
                if (!selectable)
                    ImGui::BeginDisabled();

                if (ImGui::Selectable(Get_MaterialScalarTimeSourceLabel(candidate), isSelected) && selectable)
                {
                    value = candidate;
                    changed = true;
                }

                if (!selectable)
                    ImGui::EndDisabled();

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static bool Draw_SphereLocationSpawnModeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        SphereLocationSpawnMode& value,
        SphereLocationSpawnMode defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_SphereLocationSpawnModeLabel(value)))
        {
            const SphereLocationSpawnMode candidates[] = {
                SphereLocationSpawnMode::Volume,
                SphereLocationSpawnMode::Surface,
            };

            for (const SphereLocationSpawnMode candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_SphereLocationSpawnModeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static bool Draw_SubUVFramePlaybackModeProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        SubUVFramePlaybackMode& value,
        SubUVFramePlaybackMode defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_SubUVFramePlaybackModeLabel(value)))
        {
            const SubUVFramePlaybackMode candidates[] = {
                SubUVFramePlaybackMode::FixedFrame,
                SubUVFramePlaybackMode::LifeProgress,
                SubUVFramePlaybackMode::FramesPerSecond,
                SubUVFramePlaybackMode::RandomFrame,
            };

            for (const SubUVFramePlaybackMode candidate : candidates)
            {
                const bool isSelected = value == candidate;
                if (ImGui::Selectable(Get_SubUVFramePlaybackModeLabel(candidate), isSelected))
                {
                    value = candidate;
                    changed = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static bool Normalize_SubUVFrameIndexDistribution(FloatDistributionData& data, uint32 lastFrame)
    {
        bool changed = false;
        const float minFrame = 0.f;
        const float maxFrame = static_cast<float>(lastFrame);

        if (data.mode == DistributionMode::Uniform)
        {
            data = FloatDistributionData::Make_ConstantCurve(0.f);
            return true;
        }

        if (auto* constant = get_if<ConstantFloatDistributionData>(&data.payload))
        {
            const float clampedValue = clamp(constant->value, minFrame, maxFrame);
            if (!NearlyEqual(constant->value, clampedValue))
            {
                constant->value = clampedValue;
                changed = true;
            }
        }
        else if (auto* curve = get_if<ConstantCurveFloatDistributionData>(&data.payload))
        {
            for (FloatCurveKeyData& key : curve->keys)
            {
                const float clampedValue = clamp(key.value, minFrame, maxFrame);
                if (!NearlyEqual(key.value, clampedValue))
                {
                    key.value = clampedValue;
                    changed = true;
                }
            }
        }

        return changed;
    }

    static bool Is_SameMaterial(const EffectMaterialInstanceData& lhs, const EffectMaterialInstanceData& rhs)
    {
        return
            lhs.mainTextureGuid == rhs.mainTextureGuid &&
            lhs.mainTexturePath == rhs.mainTexturePath &&
            lhs.noiseTextureGuid == rhs.noiseTextureGuid &&
            lhs.noiseTexturePath == rhs.noiseTexturePath &&
            lhs.maskTextureGuid == rhs.maskTextureGuid &&
            lhs.maskTexturePath == rhs.maskTexturePath &&
            Is_SameColor(lhs.tint, rhs.tint) &&
            NearlyEqual(lhs.intensity, rhs.intensity) &&
            NearlyEqual(lhs.opacityPower, rhs.opacityPower) &&
            NearlyEqual(lhs.noiseStrength, rhs.noiseStrength) &&
            NearlyEqual(lhs.alphaCutoff, rhs.alphaCutoff) &&
            NearlyEqual(lhs.alphaErosion, rhs.alphaErosion) &&
            Is_SameVec2(lhs.mainUVScale, rhs.mainUVScale) &&
            Is_SameVec2(lhs.mainUVOffset, rhs.mainUVOffset) &&
            Is_SameVec2(lhs.mainUVScrollSpeed, rhs.mainUVScrollSpeed) &&
            lhs.mainUVTilingMode == rhs.mainUVTilingMode &&
            Is_SameVec2(lhs.noiseUVScale, rhs.noiseUVScale) &&
            Is_SameVec2(lhs.noiseUVOffset, rhs.noiseUVOffset) &&
            Is_SameVec2(lhs.noiseUVScrollSpeed, rhs.noiseUVScrollSpeed) &&
            lhs.noiseUVTilingMode == rhs.noiseUVTilingMode &&
            Is_SameVec2(lhs.maskUVScale, rhs.maskUVScale) &&
            Is_SameVec2(lhs.maskUVOffset, rhs.maskUVOffset) &&
            Is_SameVec2(lhs.maskUVScrollSpeed, rhs.maskUVScrollSpeed) &&
            lhs.maskUVTilingMode == rhs.maskUVTilingMode &&
            lhs.blendMode == rhs.blendMode &&
            lhs.opacitySource == rhs.opacitySource &&
            Is_SameAdditiveContribution(lhs.additive, rhs.additive) &&
            lhs.coreEmissive.enabled == rhs.coreEmissive.enabled &&
            Is_SameColor(lhs.coreEmissive.coreColor, rhs.coreEmissive.coreColor) &&
            NearlyEqual(lhs.coreEmissive.corePower, rhs.coreEmissive.corePower) &&
            NearlyEqual(lhs.coreEmissive.coreIntensity, rhs.coreEmissive.coreIntensity) &&
            NearlyEqual(lhs.coreEmissive.outerPower, rhs.coreEmissive.outerPower) &&
            NearlyEqual(lhs.coreEmissive.outerIntensity, rhs.coreEmissive.outerIntensity) &&
            lhs.twoSided == rhs.twoSided &&
            lhs.subUVRows == rhs.subUVRows &&
            lhs.subUVCols == rhs.subUVCols;
    }

    static string Try_ResolvePresetNameFromMaterial(const EffectMaterialInstanceData& material)
    {
        const fs::path presetFolder = Normalize_MaterialDialogPath(kMaterialDialogDefaultFolder);
        if (!fs::exists(presetFolder) || !fs::is_directory(presetFolder))
            return {};

        for (const auto& entry : fs::directory_iterator(presetFolder))
        {
            if (!entry.is_regular_file())
                continue;

            const fs::path presetPath = entry.path();
            if (!String::ToLowerCopy(presetPath.generic_string()).ends_with(".effectmaterial.json"))
                continue;

            EffectMaterialInstanceData presetMaterial{};
            if (!EffectMaterialPresetReader::Read(presetPath, presetMaterial))
                continue;

            if (Is_SameMaterial(material, presetMaterial))
                return presetMaterial.sourcePresetName;
        }

        return {};
    }

    static wstring Try_ResolvePresetPathFromMaterial(const EffectMaterialInstanceData& material)
    {
        const fs::path presetFolder = Normalize_MaterialDialogPath(kMaterialDialogDefaultFolder);
        if (!fs::exists(presetFolder) || !fs::is_directory(presetFolder))
            return {};

        if (!material.sourcePresetName.empty())
        {
            const fs::path candidatePath =
                presetFolder / (String::ToWString(material.sourcePresetName) + L".effectmaterial.json");
            if (fs::exists(candidatePath) && fs::is_regular_file(candidatePath))
                return candidatePath.wstring();
        }

        for (const auto& entry : fs::directory_iterator(presetFolder))
        {
            if (!entry.is_regular_file())
                continue;

            const fs::path presetPath = entry.path();
            if (!String::ToLowerCopy(presetPath.generic_string()).ends_with(".effectmaterial.json"))
                continue;

            EffectMaterialInstanceData presetMaterial{};
            if (!EffectMaterialPresetReader::Read(presetPath, presetMaterial))
                continue;

            if (Is_SameMaterial(material, presetMaterial))
                return presetPath.wstring();
        }

        return {};
    }

    static const string& Resolve_MaterialDisplayName(RequiredModuleData& data)
    {
        if (data.material.sourcePresetName.empty())
            data.material.sourcePresetName = Try_ResolvePresetNameFromMaterial(data.material);

        return data.material.sourcePresetName;
    }

    bool Try_PickMaterialPresetDialog(wstring& outFilePath)
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

        dialog->SetTitle(L"Select Effect Material");

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST);

        const fs::path defaultFolder = Normalize_MaterialDialogPath(kMaterialDialogDefaultFolder);
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
            { L"Effect Materials", L"*.effectmaterial.json" },
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

    Shared<EffectMaterial_View> Get_MaterialView()
    {
        if (nullptr == EDITOR)
            return nullptr;

        const Shared<Editor_Window> materialWindow = EDITOR->Get_Window(L"Effect Material");
        return dynamic_pointer_cast<EffectMaterial_View>(materialWindow);
    }

    void Open_MaterialInstanceWindow(uint32 emitterId, uint32 moduleId)
    {
        if (nullptr == EDITOR)
            return;

        const Shared<Editor_Window> materialInstanceWindow = EDITOR->Get_Window(L"Effect Material Instance");
        const Shared<EffectMaterialInstance_View> materialInstanceView =
            dynamic_pointer_cast<EffectMaterialInstance_View>(materialInstanceWindow);
        if (nullptr == materialInstanceView)
            return;

        materialInstanceView->Open_Instance(emitterId, moduleId);
    }

    void Open_PresetBrowserWindow(const RequiredModuleData& data)
    {
        const Shared<EffectMaterial_View> materialView = Get_MaterialView();
        if (nullptr == materialView)
            return;

        const wstring presetPath = Try_ResolvePresetPathFromMaterial(data.material);
        if (!presetPath.empty())
        {
            materialView->Open_Preset(presetPath);
            return;
        }

        materialView->Open_PresetBrowser();
    }

    bool Apply_PresetFileToRequired(const wchar_t* filePath, RequiredModuleData& data)
    {
        if (filePath == nullptr || filePath[0] == L'\0')
            return false;

        const Shared<EffectMaterial_View> materialView = Get_MaterialView();
        if (materialView == nullptr)
            return false;

        materialView->Open_Preset(filePath);
        if (!materialView->Has_SelectedPreset())
            return false;

        data.material = materialView->Get_SelectedPresetMaterial();
        return true;
    }

    bool Draw_MaterialDropSlot(
        const AuthoringEmitter& emitter,
        RequiredModuleData& data,
        EffectMaterialPreviewRenderer* materialSlotPreview,
        float timeDelta)
    {
        constexpr float browseButtonWidth{ 28.f };
        constexpr float slotHeight = 54.f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const ImGuiPayload* activePayload = ImGui::GetDragDropPayload();
        const bool isMaterialDragging =
            activePayload != nullptr &&
            activePayload->IsDataType(kEffectMaterialPayloadType);

        const ImVec2 slotMin = ImGui::GetCursorScreenPos();
        const float slotWidth = max(180.f, ImGui::GetContentRegionAvail().x - browseButtonWidth - spacing);
        ImGui::InvisibleButton("##MaterialDropSlot", ImVec2(slotWidth, slotHeight));

        bool changed = false;
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                kEffectMaterialPayloadType,
                ImGuiDragDropFlags_AcceptBeforeDelivery
            ))
            {
                if (payload->IsDelivery())
                    changed = Apply_PresetFileToRequired(static_cast<const wchar_t*>(payload->Data), data);
            }

            ImGui::EndDragDropTarget();
        }

        const bool isHovered = ImGui::IsItemHovered();
        const ImVec2 slotMax{ slotMin.x + slotWidth, slotMin.y + slotHeight };
        const ImU32 fillColor = isMaterialDragging
                                ? IM_COL32(42, 56, 78, 255)
                                : IM_COL32(30, 34, 42, 255);
        const ImU32 borderColor = isMaterialDragging
                                  ? IM_COL32(96, 165, 250, 255)
                                  : isHovered ? IM_COL32(120, 130, 150, 255) : IM_COL32(70, 78, 92, 255);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(slotMin, slotMax, fillColor, 6.f);
        drawList->AddRect(slotMin, slotMax, borderColor, 6.f, 0, isMaterialDragging ? 2.5f : 1.5f);

        ShaderResourceView* previewSrv = nullptr;
        if (materialSlotPreview != nullptr)
        {
            materialSlotPreview->Update_MaterialTime(timeDelta);
            if (const MaterialScalarModulationModuleData* modulation = Find_MaterialScalarModulationModuleData(emitter))
            {
                materialSlotPreview->Render_Material(
                    data.material,
                    Build_CoreColorRgbModulationRuntimeDesc(*modulation),
                    Build_MaterialScalarModulationRuntimeDesc(*modulation),
                    data.duration,
                    false
                );
            }
            else
                materialSlotPreview->Render_Material(data.material);
            previewSrv = materialSlotPreview->Get_SRV();
        }
        const ImVec2 thumbnailMin{ slotMin.x + 7.f, slotMin.y + 7.f };
        const ImVec2 thumbnailMax{ thumbnailMin.x + 40.f, thumbnailMin.y + 40.f };
        if (previewSrv != nullptr)
        {
            const ImTextureID textureId = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(previewSrv));
            drawList->AddImage(textureId, thumbnailMin, thumbnailMax);
        }
        else
        {
            drawList->AddRectFilled(thumbnailMin, thumbnailMax, IM_COL32(18, 20, 24, 255), 4.f);
            drawList->AddText(ImVec2(thumbnailMin.x + 8.f, thumbnailMin.y + 12.f), IM_COL32(150, 160, 176, 255), "MAT");
        }
        drawList->AddRect(thumbnailMin, thumbnailMax, IM_COL32(85, 96, 116, 255), 4.f);

        const string& resolvedName = Resolve_MaterialDisplayName(data);
        const string displayName = resolvedName.empty() ? "(drop material here)" : resolvedName;
        const string blendModeName = string(magic_enum::enum_name(data.material.blendMode));
        const string sourceSummary =
            data.material.blendMode == EffectMaterialBlendMode::Additive
            ? string(magic_enum::enum_name(data.material.additive.colorSource)) + " / " +
              string(magic_enum::enum_name(data.material.additive.amountSource))
            : data.material.opacitySource;
        const string materialSummary = blendModeName + " / " + sourceSummary +
                                       " / SubUV " + to_string(data.material.subUVRows) + "x" + to_string(data.material.subUVCols);
        drawList->PushClipRect(
            ImVec2(slotMin.x + 57.f, slotMin.y),
            ImVec2(slotMax.x - 8.f, slotMax.y),
            true
        );
        drawList->AddText(ImVec2(slotMin.x + 57.f, slotMin.y + 9.f), IM_COL32(235, 238, 245, 255), displayName.c_str());
        drawList->AddText(
            ImGui::GetFont(),
            ImGui::GetFontSize() * 0.9f,
            ImVec2(slotMin.x + 57.f, slotMin.y + 29.f),
            IM_COL32(145, 154, 172, 255),
            materialSummary.c_str()
        );
        drawList->PopClipRect();

        if (isHovered)
        {
            const string mainTooltip =
                "GUID: " + (data.material.mainTextureGuid.empty() ? string{ "(none)" } : data.material.mainTextureGuid) +
                "\nPath: " + (data.material.mainTexturePath.empty() ? string{ "(none)" } : data.material.mainTexturePath);
            const string noiseTooltip =
                data.material.noiseTextureGuid.empty() && data.material.noiseTexturePath.empty()
                ? string{ "GUID: (none)\nPath: (none)" }
                : "GUID: " + (data.material.noiseTextureGuid.empty() ? string{ "(none)" } : data.material.noiseTextureGuid) +
                  "\nPath: " + (data.material.noiseTexturePath.empty() ? string{ "(none)" } : data.material.noiseTexturePath);
            ImGui::SetTooltip(
                "Content Browser의 .effectmaterial.json 머티리얼 파일을 여기에 드롭하면 현재 embedded material instance로 복사합니다.\n"
                "현재 sprite effect 경로에서는 Main Texture를 emissive color source로, Opacity Source를 별도 opacity 해석 기준으로 사용합니다.\n\n"
                "Main Texture (Emissive)\n%s\n\nNoise Texture\n%s\n\n%s / %s / SubUV %ux%u",
                mainTooltip.c_str(),
                noiseTooltip.c_str(),
                blendModeName.c_str(),
                sourceSummary.c_str(),
                data.material.subUVRows,
                data.material.subUVCols
            );
        }

        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2{ 0.5f, 0.5f });
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 0.f, 0.f });
        if (ImGui::Button(ICON_FA_FOLDER_OPEN "##PickMaterial", ImVec2{ browseButtonWidth, slotHeight }))
        {
            wstring pickedFilePath{};
            if (Try_PickMaterialPresetDialog(pickedFilePath))
                changed = Apply_PresetFileToRequired(pickedFilePath.c_str(), data);
        }
        ImGui::PopStyleVar(2);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("머티리얼 원본 파일 선택");

        return changed;
    }

    static uint32 Resolve_SubUvFrameCount(const AuthoringEmitter& emitter)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type != AuthoringModuleType::Required)
                continue;

            const RequiredModuleData* requiredData = get_if<RequiredModuleData>(&module.data);
            if (requiredData == nullptr)
                return 1;

            const uint32 rows = max(1u, requiredData->material.subUVRows);
            const uint32 cols = max(1u, requiredData->material.subUVCols);
            return max(1u, rows * cols);
        }

        return 1;
    }

    static bool Is_SameVec3(const Vec3& lhs, const Vec3& rhs)
    {
        return
            NearlyEqual(lhs.x, rhs.x) &&
            NearlyEqual(lhs.y, rhs.y) &&
            NearlyEqual(lhs.z, rhs.z);
    }

    static Vec3 Resolve_CoreColorRgbDefault(const AuthoringEmitter& emitter)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type != AuthoringModuleType::Required)
                continue;

            const RequiredModuleData* requiredData = get_if<RequiredModuleData>(&module.data);
            if (requiredData == nullptr)
                break;

            return Vec3{
                requiredData->material.coreEmissive.coreColor.x,
                requiredData->material.coreEmissive.coreColor.y,
                requiredData->material.coreEmissive.coreColor.z
            };
        }

        return Vec3{ 1.f, 0.85f, 0.45f };
    }

    static MaterialCoreColorRgbModulatorData Make_CoreColorRgbModulator(const AuthoringEmitter& emitter)
    {
        MaterialCoreColorRgbModulatorData data{};
        data.distribution = Vector3DistributionData::Make_Constant(Resolve_CoreColorRgbDefault(emitter));
        return data;
    }

    static bool Draw_RadialPivotProperty(
        DetailPropertyContext& detailContext,
        Vec3& value,
        const Vec3& defaultValue)
    {
        bool changed = false;

        ImGui::PushID("RadialPivot");
        detailContext.Draw_PropertyLabel("방사 기준점");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            ImGui::SetTooltip(
                "입자 생성 위치에서 이 기준점을 뺀 방향으로 초기 속도를 더합니다.\n"
                "기준점이 생성 위치보다 위에 있으면 +속도에서도 아래 방향으로 퍼질 수 있습니다."
            );
        }

        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::DragFloat3("##Value", &value.x, 0.01f);
        if (detailContext.Draw_ResetButton(!Is_SameVec3(value, defaultValue)))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }

    static bool Draw_RadialWorldSpaceProperty(
        DetailPropertyContext& detailContext,
        bool& value,
        bool defaultValue)
    {
        bool changed = false;

        ImGui::PushID("RadialWorldSpace");
        detailContext.Draw_PropertyLabel("월드 스페이스");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            ImGui::SetTooltip(
                "켜면 방사 기준점을 world offset으로 사용합니다.\n"
                "끄면 emitter 회전 기준 local offset으로 변환합니다."
            );
        }

        changed |= ImGui::Checkbox("##Value", &value);
        if (detailContext.Draw_ResetButton(value != defaultValue))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();

        return changed;
    }
}

bool RequiredModuleDetail::Draw(
    AuthoringEmitter& emitter,
    const AuthoringModule& module,
    RequiredModuleData& data,
    DetailPropertyContext& detailContext,
    float timeDelta)
{
    const RequiredModuleData defaultData{};
    bool changed = false;

    if (_materialSlotPreview == nullptr)
        _materialSlotPreview = EffectMaterialPreviewRenderer::Create(64);

    if (ImGui::CollapsingHeader("머티리얼", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredMaterial"))
        {
            ImGui::PushID("MaterialDropSlot");
            detailContext.Draw_PropertyLabel("머티리얼");
            changed |= Internal::Draw_MaterialDropSlot(emitter, data, _materialSlotPreview.get(), timeDelta);
            detailContext.Draw_ResetButton(false);
            ImGui::PopID();

            ImGui::PushID("MaterialEdit");
            detailContext.Draw_PropertyLabel("작업");

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float availableWidth = ImGui::GetContentRegionAvail().x;
            const float editButtonWidth = (availableWidth - spacing) * 0.5f;
            const float sourceButtonWidth = (availableWidth - spacing) * 0.5f;

            if (ImGui::Button("인스턴스 편집", ImVec2{ editButtonWidth, 24.f }))
                Internal::Open_MaterialInstanceWindow(emitter.id, module.id);

            ImGui::SameLine();

            if (ImGui::Button("원본 보기", ImVec2{ sourceButtonWidth, 24.f }))
                Internal::Open_PresetBrowserWindow(data);

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("원본 머티리얼을 확인");
            ImGui::PopID();

            detailContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("이미터 좌표", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredTransform"))
        {
            changed |= detailContext.Draw_Vec3Property("원점", data.emitterOrigin, defaultData.emitterOrigin);
            changed |= detailContext.Draw_Vec3Property("회전 (도)", data.emitterRotationDegrees, defaultData.emitterRotationDegrees);
            detailContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("공간 / 정렬", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredSpaceSort"))
        {
            changed |= detailContext.Draw_ScreenAlignmentProperty("화면 정렬", data.screenAlignment, defaultData.screenAlignment);
            if (Internal::Uses_DirectionalAlignmentOptions(data.screenAlignment))
            {
                const EmitterDirectionalAlignmentMode oldMode = data.directionalAlignmentMode;
                const EmitterSpriteTextureAxis oldAxis = data.spriteTextureAxis;

                if (detailContext.Draw_DirectionalAlignmentModeProperty("방향 정렬 방식", data.directionalAlignmentMode, defaultData.directionalAlignmentMode))
                {
                    changed = true;
                    if (oldMode != data.directionalAlignmentMode)
                        Internal::Notify_DirectionalAlignmentModeChanged(data.screenAlignment, data.directionalAlignmentMode);
                }

                if (data.directionalAlignmentMode == EmitterDirectionalAlignmentMode::TextureAxis)
                {
                    if (detailContext.Draw_SpriteTextureAxisProperty("정렬할 텍스처 축", data.spriteTextureAxis, defaultData.spriteTextureAxis))
                    {
                        changed = true;
                        if (oldAxis != data.spriteTextureAxis)
                            Internal::Notify_TextureAxisChanged(data.spriteTextureAxis);
                    }

                    changed |= detailContext.Draw_FloatProperty("롤 보정 (도)", data.spriteRollOffsetDegrees, defaultData.spriteRollOffsetDegrees, 0.1f);
                }
            }

            changed |= detailContext.Draw_BoolProperty("로컬 스페이스 사용", data.useLocalSpace, defaultData.useLocalSpace);
            changed |= detailContext.Draw_SortModeProperty("정렬 모드 (미구현)", data.sortMode, defaultData.sortMode);
            changed |= detailContext.Draw_FloatProperty("카메라 모션 블러 양 (미구현)", data.cameraMotionBlurAmount, defaultData.cameraMotionBlurAmount);
            changed |= detailContext.Draw_BoolProperty("초기화 시 기존 파티클 지우기 (미구현)", data.clearExistingParticlesOnInit, defaultData.clearExistingParticlesOnInit);
            detailContext.End_PropertyTable();
        }

        if (Internal::Uses_DirectionalAlignmentOptions(data.screenAlignment))
        {
            if (data.directionalAlignmentMode == EmitterDirectionalAlignmentMode::TextureAxis)
                ImGui::TextDisabled("Texture Axis는 world X/Y/Z가 아니라 texture 가로/세로 축입니다. 나머지 축은 카메라 가시성을 유지합니다.");
            else
                ImGui::TextDisabled("Look Direction은 기준 방향을 sprite 카드의 바라보는 방향으로 사용합니다.");
        }

        detailContext.Draw_ScreenAlignmentImplementationNote(data.screenAlignment);
    }

    if (ImGui::CollapsingHeader("이미터 재생 / 스폰 시간", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredTime"))
        {
            changed |= detailContext.Draw_UintProperty("루프 횟수", data.loopCount, defaultData.loopCount);
            changed |= detailContext.Draw_FloatProperty("스폰 활성 시간", data.duration, defaultData.duration);
            changed |= detailContext.Draw_FloatProperty("스폰 활성 시간 최소값 (범위 미구현)", data.durationLow, defaultData.durationLow);
            changed |= detailContext.Draw_BoolProperty("스폰 활성 시간 범위 사용 (미구현)", data.useDurationRange, defaultData.useDurationRange);
            changed |= detailContext.Draw_BoolProperty("루프마다 스폰 시간 재계산 (미구현)", data.recalculateDurationEachLoop, defaultData.recalculateDurationEachLoop);
            changed |= detailContext.Draw_FloatProperty("스폰 시작 딜레이", data.delay, defaultData.delay);
            changed |= detailContext.Draw_FloatProperty("스폰 시작 딜레이 최소값 (범위 미구현)", data.delayLow, defaultData.delayLow);
            changed |= detailContext.Draw_BoolProperty("딜레이 범위 사용 (미구현)", data.useDelayRange, defaultData.useDelayRange);
            changed |= detailContext.Draw_BoolProperty("첫 루프만 스폰 딜레이", data.delayFirstLoopOnly, defaultData.delayFirstLoopOnly);
            detailContext.End_PropertyTable();
        }

        ImGui::TextDisabled("스폰 활성 시간은 새 파티클을 만들 수 있는 emitter loop 구간입니다. 이미 태어난 파티클은 Particle Lifetime이 끝날 때까지 남을 수 있습니다.");
    }

    if (ImGui::CollapsingHeader("종료 / 킬", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredKill"))
        {
            changed |= detailContext.Draw_BoolProperty("비활성화 시 킬 (외부 비활성화 계약 보류)", data.killOnDeactivate, defaultData.killOnDeactivate);
            changed |= detailContext.Draw_BoolProperty("완료 시 킬", data.killOnCompleted, defaultData.killOnCompleted);
            detailContext.End_PropertyTable();
        }

        if (data.killOnCompleted)
            ImGui::TextDisabled("완료 시 active particle도 즉시 종료합니다.");
    }

    if (ImGui::CollapsingHeader("렌더링 제한", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredRenderLimit"))
        {
            changed |= detailContext.Draw_BoolProperty("최대 드로우 수 사용", data.useMaxDrawCount, defaultData.useMaxDrawCount);
            changed |= detailContext.Draw_UintProperty("최대 드로우 수", data.maxDrawCount, defaultData.maxDrawCount);
            detailContext.End_PropertyTable();
        }
    }

    if (changed)
    {
        emitter.rendererType = emitter.typeData.kind == AuthoringTypeDataKind::None
                               ? data.rendererType
                               : Authoring::Resolve_RendererType(emitter.typeData);
        emitter.textureId = data.material.mainTexturePath;
        emitter.resourceSummary = data.material.mainTexturePath;
    }

    return changed;
}

bool SpawnModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, SpawnModuleData& data, DetailPropertyContext& detailContext)
{
    const SpawnModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("연속 스폰", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SpawnRate"))
        {
            changed |= detailContext.Draw_BoolProperty("연속 스폰 사용", data.processSpawnRate, defaultData.processSpawnRate);
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_FloatDistributionGroup("초당 생성 수", data.spawnRate, defaultData.spawnRate);
        changed |= detailContext.Draw_FloatDistributionGroup("스폰 속도 스케일", data.spawnRateScale, defaultData.spawnRateScale);
    }

    if (ImGui::CollapsingHeader("버스트", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SpawnBurst"))
        {
            changed |= detailContext.Draw_BoolProperty("버스트 사용", data.processBurstList, defaultData.processBurstList);
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_FloatDistributionGroup("버스트 스케일", data.burstScale, defaultData.burstScale);
        changed |= detailContext.Draw_BurstListProperty(data.burstList, defaultData.burstList);
    }

    if (ImGui::CollapsingHeader("제한", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SpawnLimit"))
        {
            changed |= detailContext.Draw_UintProperty("최대 활성 파티클 수", data.maxParticleCount, defaultData.maxParticleCount);
            detailContext.End_PropertyTable();
        }
    }

    return changed;
}

bool LifetimeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, LifetimeModuleData& data, DetailPropertyContext& detailContext)
{
    const LifetimeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("파티클 생존 시간", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_FloatDistributionGroup("파티클 생존 시간", data.lifeTime, defaultData.lifeTime);
        ImGui::TextDisabled("각 particle이 spawn된 뒤 살아 있는 시간입니다. Over Life 계열 curve의 기준이 됩니다.");
    }

    return changed;
}

bool InitialSizeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialSizeModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialSizeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("크기", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_Vector2DistributionGroup("크기", data.size, defaultData.size);

    return changed;
}

bool InitialMeshSizeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialMeshSizeModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialMeshSizeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("메시 크기", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_Vector3DistributionGroup("크기 XYZ", data.size, defaultData.size);

    return changed;
}

bool InitialLocationModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialLocationModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialLocationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("위치", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_Vector3DistributionGroup("위치", data.location, defaultData.location, 0.01f, false, true);

    return changed;
}

bool SphereLocationModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, SphereLocationModuleData& data, DetailPropertyContext& detailContext)
{
    const SphereLocationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("구 위치", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SphereLocation"))
        {
            changed |= detailContext.Draw_Vec3Property("오프셋", data.offset, defaultData.offset);
            changed |= detailContext.Draw_FloatProperty("반지름", data.radius, defaultData.radius);
            changed |= Internal::Draw_SphereLocationSpawnModeProperty(
                detailContext,
                "생성 방식",
                data.spawnMode,
                defaultData.spawnMode
            );
            detailContext.End_PropertyTable();
        }
    }

    data.radius = max(0.f, data.radius);
    return changed;
}

bool InitialVelocityModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialVelocityModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialVelocityModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("속도", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("InitialVelocity"))
        {
            changed |= detailContext.Draw_BoolProperty("월드 스페이스", data.inWorldSpace, defaultData.inWorldSpace);
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_Vector3DistributionGroup("속도", data.velocity, defaultData.velocity);
    }

    return changed;
}

bool InitialRadialVelocityModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    InitialRadialVelocityModuleData& data,
    DetailPropertyContext& detailContext)
{
    const InitialRadialVelocityModuleData defaultData{};
    bool changed = false;

    const bool radialVelocityOpen = ImGui::CollapsingHeader("방사성 속도", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
    {
        ImGui::SetTooltip(
            "생성 위치와 방사 기준점의 방향으로 더하는 초기 속도입니다.\n"
            "중앙 발산/흡입형 sprite streak의 1차 후보입니다.\n"
            "양수는 기준점에서 멀어지고, 음수는 기준점 쪽으로 이동합니다.\n"
            "지속 attractor가 아니라 생성 시점의 속도 항입니다."
        );
    }

    if (radialVelocityOpen)
    {
        if (detailContext.Begin_PropertyTable("InitialRadialVelocity"))
        {
            changed |= Internal::Draw_RadialPivotProperty(detailContext, data.radialPivot, defaultData.radialPivot);
            changed |= Internal::Draw_RadialWorldSpaceProperty(detailContext, data.inWorldSpace, defaultData.inWorldSpace);
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_FloatDistributionGroup("속도", data.speed, defaultData.speed);
    }

    return changed;
}

bool AccelerationModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, AccelerationModuleData& data, DetailPropertyContext& detailContext)
{
    const AccelerationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("가속", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_Vector3DistributionGroup("가속", data.acceleration, defaultData.acceleration);

    return changed;
}

bool DragModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, DragModuleData& data, DetailPropertyContext& detailContext)
{
    const DragModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("드래그", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_FloatDistributionGroup("드래그", data.drag, defaultData.drag);

    return changed;
}

bool VelocityOverLifeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, VelocityOverLifeModuleData& data, DetailPropertyContext& detailContext)
{
    const VelocityOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 속도", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("VelocityOverLife"))
        {
            changed |= detailContext.Draw_FloatProperty("시작 배율", data.scaleStart, defaultData.scaleStart);
            changed |= detailContext.Draw_FloatProperty("종료 배율", data.scaleEnd, defaultData.scaleEnd);
            detailContext.End_PropertyTable();
        }
    }

    return changed;
}

bool InitialRotationModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialRotationModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialRotationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("초기 회전", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_FloatDistributionGroup("회전각", data.rotationDegrees, defaultData.rotationDegrees);

    return changed;
}

bool RotationOverLifeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, RotationOverLifeModuleData& data, DetailPropertyContext& detailContext)
{
    const RotationOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 회전", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_FloatDistributionGroup("추가 회전각", data.rotationOverLife, defaultData.rotationOverLife);

    return changed;
}

bool InitialRotationRateModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialRotationRateModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialRotationRateModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("초기 회전 속도", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_FloatDistributionGroup("회전 속도", data.rotationRateDegrees, defaultData.rotationRateDegrees);

    return changed;
}

bool RotationRateScaleByLifeModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    RotationRateScaleByLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const RotationRateScaleByLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 회전 속도", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_FloatDistributionGroup("회전 속도 배율", data.scaleOverLife, defaultData.scaleOverLife);

    return changed;
}

bool InitialMeshRotationModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    InitialMeshRotationModuleData& data,
    DetailPropertyContext& detailContext)
{
    const InitialMeshRotationModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("초기 메시 회전", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_Vector3DistributionGroup("회전 XYZ", data.rotationDegrees, defaultData.rotationDegrees);

    return changed;
}

bool MeshRotationOverLifeModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    MeshRotationOverLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const MeshRotationOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 메시 회전", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_Vector3DistributionGroup("추가 회전 XYZ", data.rotationOverLife, defaultData.rotationOverLife);

    return changed;
}

bool InitialMeshRotationRateModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    InitialMeshRotationRateModuleData& data,
    DetailPropertyContext& detailContext)
{
    const InitialMeshRotationRateModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("초기 메시 회전 속도", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_Vector3DistributionGroup("회전 속도 XYZ", data.rotationRateDegrees, defaultData.rotationRateDegrees);

    return changed;
}

bool MeshRotationRateScaleByLifeModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    MeshRotationRateScaleByLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const MeshRotationRateScaleByLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 메시 회전 속도", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_Vector3DistributionGroup("회전 속도 배율 XYZ", data.scaleOverLife, defaultData.scaleOverLife);

    return changed;
}

bool InitialColorModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialColorModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialColorModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("초기 컬러", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_ColorRgbDistributionGroup("색상(RGB)", data.color, defaultData.color);
        changed |= detailContext.Draw_FloatDistributionGroup("알파", data.alpha, defaultData.alpha);
    }

    return changed;
}

bool ColorOverLifeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, ColorOverLifeModuleData& data, DetailPropertyContext& detailContext)
{
    const ColorOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 색상", ImGuiTreeNodeFlags_DefaultOpen))
    {
        changed |= detailContext.Draw_ColorRgbDistributionGroup("색상(RGB)", data.colorOverLife, defaultData.colorOverLife);
        changed |= detailContext.Draw_FloatDistributionGroup("알파", data.alphaOverLife, defaultData.alphaOverLife);
    }

    return changed;
}

bool SubUVFrameOverLifeModuleDetail::Draw(
    const AuthoringEmitter& emitter,
    AuthoringModule&,
    SubUVFrameOverLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const SubUVFrameOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("서브UV 프레임 선택/재생", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SubUVFrameOverLife"))
        {
            changed |= Internal::Draw_SubUVFramePlaybackModeProperty(
                detailContext,
                "재생 방식",
                data.playbackMode,
                defaultData.playbackMode
            );

            if (data.playbackMode == SubUVFramePlaybackMode::FixedFrame)
                changed |= detailContext.Draw_UintProperty("프레임", data.startFrame, defaultData.startFrame);
            else
            {
                changed |= detailContext.Draw_UintProperty("시작 프레임", data.startFrame, defaultData.startFrame);
                changed |= detailContext.Draw_UintProperty("종료 프레임", data.endFrame, defaultData.endFrame);

                if (data.playbackMode == SubUVFramePlaybackMode::FramesPerSecond)
                {
                    changed |= detailContext.Draw_BoolProperty("루프", data.loop, defaultData.loop);
                    changed |= detailContext.Draw_FloatProperty("FPS", data.framesPerSecond, defaultData.framesPerSecond, 0.1f);
                }
            }
            detailContext.End_PropertyTable();
        }

        const uint32 frameCount = Internal::Resolve_SubUvFrameCount(emitter);
        const uint32 lastFrame = frameCount > 0 ? frameCount - 1 : 0;
        data.startFrame = min(data.startFrame, lastFrame);
        data.endFrame = min(data.endFrame, lastFrame);
        data.framesPerSecond = max(0.f, data.framesPerSecond);
        changed |= Internal::Normalize_SubUVFrameIndexDistribution(data.frameIndex, lastFrame);

        ImGui::TextDisabled("현재 atlas frame 수: %u", frameCount);
    }

    return changed;
}

bool SizeByLifeModuleDetail::Draw(
    AuthoringEmitter& emitter,
    AuthoringModule&,
    SizeByLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const SizeByLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 크기", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SizeByLife"))
        {
            changed |= detailContext.Draw_BoolProperty("X축 적용", data.multiplyX, defaultData.multiplyX);
            changed |= detailContext.Draw_BoolProperty("Y축 적용", data.multiplyY, defaultData.multiplyY);
            detailContext.End_PropertyTable();
        }

        const auto axis_lock_label = [](SizeByLifeModuleData::AxisLock axisLock)
        {
            switch (axisLock)
            {
            case SizeByLifeModuleData::AxisLock::X:
                return "X";
            case SizeByLifeModuleData::AxisLock::Y:
                return "Y";
            case SizeByLifeModuleData::AxisLock::None:
            default:
                return "None";
            }
        };

        ImGui::PushID("SizeByLifeAxisLock");
        ImGui::TextUnformatted("축 잠금");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.f);
        if (ImGui::BeginCombo("##AxisLock", axis_lock_label(data.axisLock)))
        {
            const auto draw_axis_lock = [&](SizeByLifeModuleData::AxisLock value)
            {
                const bool selected = data.axisLock == value;
                if (ImGui::Selectable(axis_lock_label(value), selected))
                {
                    data.axisLock = value;
                    changed = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            };

            draw_axis_lock(SizeByLifeModuleData::AxisLock::None);
            draw_axis_lock(SizeByLifeModuleData::AxisLock::X);
            draw_axis_lock(SizeByLifeModuleData::AxisLock::Y);
            ImGui::EndCombo();
        }
        ImGui::PopID();

        changed |= detailContext.Draw_Vector2DistributionGroup("수명 배율", data.scaleOverLife, defaultData.scaleOverLife, 0.01f);
    }

    return changed;
}

bool MeshSizeByLifeModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    MeshSizeByLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const MeshSizeByLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 메시 크기", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("MeshSizeByLife"))
        {
            changed |= detailContext.Draw_BoolProperty("X축 적용", data.multiplyX, defaultData.multiplyX);
            changed |= detailContext.Draw_BoolProperty("Y축 적용", data.multiplyY, defaultData.multiplyY);
            changed |= detailContext.Draw_BoolProperty("Z축 적용", data.multiplyZ, defaultData.multiplyZ);
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_Vector3DistributionGroup("수명 배율 XYZ", data.scaleOverLife, defaultData.scaleOverLife, 0.01f);
    }

    return changed;
}

bool SpawnPerUnitModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    SpawnPerUnitModuleData& data,
    DetailPropertyContext& detailContext)
{
    const SpawnPerUnitModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("거리당 스폰", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SpawnPerUnit"))
        {
            changed |= detailContext.Draw_BoolProperty("사용", data.enabled, defaultData.enabled);
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_FloatDistributionGroup("분포", data.spawnPerUnit, defaultData.spawnPerUnit);

        if (detailContext.Begin_PropertyTable("SpawnPerUnitPolicy"))
        {
            changed |= detailContext.Draw_FloatProperty(
                "단위 스칼라",
                data.unitScalar,
                defaultData.unitScalar,
                0.01f,
                "분포 값을 실제 world distance로 환산하는 기준 스칼라입니다."
            );
            changed |= detailContext.Draw_FloatProperty(
                "무브먼트 허용치",
                data.movementTolerance,
                defaultData.movementTolerance,
                0.001f,
                "이 거리 미만의 provider sample 이동은 새 history sample로 받지 않습니다."
            );
            changed |= detailContext.Draw_FloatProperty(
                "최대 프레임 거리",
                data.maxFrameDistance,
                defaultData.maxFrameDistance,
                0.01f,
                "한 frame 이동이 이 거리보다 크면 teleport/skip으로 보고 history를 재시드합니다. 0 이하면 제한이 없습니다."
            );
            detailContext.End_PropertyTable();
        }
    }

    data.unitScalar = max(0.0001f, data.unitScalar);
    data.movementTolerance = max(0.f, data.movementTolerance);
    data.maxFrameDistance = max(0.f, data.maxFrameDistance);
    return changed;
}

bool MaterialScalarModulationModuleDetail::Draw(
    const AuthoringEmitter& emitter,
    AuthoringModule&,
    MaterialScalarModulationModuleData& data,
    DetailPropertyContext& detailContext)
{
    bool changed = false;

    if (ImGui::CollapsingHeader("머티리얼 변조", ImGuiTreeNodeFlags_DefaultOpen))
    {
        constexpr uint32 kMaxMaterialScalarModulators{ 8 };
        constexpr uint32 kMaxCoreColorRgbModulators{ 8 };
        if (ImGui::Button("+ 변조 추가"))
        {
            data.modulators.push_back(MaterialScalarModulatorData{});
            changed = true;
        }

        ImGui::SameLine();
        ImGui::TextDisabled(
            "%zu / %u",
            data.modulators.size() + data.coreColorRgbModulators.size(),
            kMaxMaterialScalarModulators + kMaxCoreColorRgbModulators
        );

        struct MaterialModulationUiRow
        {
            bool isCoreColorRgb{};
            size_t index{};
        };

        vector<MaterialModulationUiRow> rows{};
        rows.reserve(data.modulators.size() + data.coreColorRgbModulators.size());
        for (size_t index = 0; index < data.modulators.size(); ++index)
            rows.push_back(MaterialModulationUiRow{ false, index });
        for (size_t index = 0; index < data.coreColorRgbModulators.size(); ++index)
            rows.push_back(MaterialModulationUiRow{ true, index });

        for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
        {
            const MaterialModulationUiRow row = rows[rowIndex];
            ImGui::PushID(row.isCoreColorRgb ? "CoreColorRgb" : "Scalar");
            ImGui::PushID(static_cast<int>(row.index));

            const MaterialScalarModulationTimeSource timeSource =
                row.isCoreColorRgb
                ? data.coreColorRgbModulators[row.index].timeSource
                : data.modulators[row.index].timeSource;
            const string header =
                string("#") + to_string(rowIndex + 1) + " " +
                Internal::Get_MaterialModulationTargetLabel(
                    row.isCoreColorRgb,
                    row.isCoreColorRgb
                    ? MaterialScalarModulationTargetField::CoreColor
                    : data.modulators[row.index].targetField
                ) + " / " +
                Internal::Get_MaterialScalarTimeSourceLabel(timeSource);
            bool structureChanged = false;

            if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (row.isCoreColorRgb)
                {
                    MaterialCoreColorRgbModulatorData& modulator = data.coreColorRgbModulators[row.index];
                    MaterialCoreColorRgbModulatorData defaultData = Internal::Make_CoreColorRgbModulator(emitter);

                    if (detailContext.Begin_PropertyTable("MaterialCoreColorRgbModulator"))
                    {
                        changed |= detailContext.Draw_BoolProperty("사용", modulator.enabled, defaultData.enabled);

                        bool nextCoreColorRgb = true;
                        MaterialScalarModulationTargetField nextTarget = MaterialScalarModulationTargetField::CoreColor;
                        if (Internal::Draw_MaterialModulationTargetProperty(
                            detailContext,
                            "대상",
                            true,
                            MaterialScalarModulationTargetField::CoreColor,
                            nextCoreColorRgb,
                            nextTarget
                        ))
                        {
                            changed = true;
                            if (!nextCoreColorRgb)
                            {
                                MaterialScalarModulatorData scalarModulator{};
                                scalarModulator.enabled = modulator.enabled;
                                scalarModulator.timeSource = modulator.timeSource;
                                scalarModulator.targetField = nextTarget;
                                scalarModulator.distribution = FloatDistributionData::Make_Constant(1.f);
                                data.modulators.push_back(scalarModulator);
                                data.coreColorRgbModulators.erase(data.coreColorRgbModulators.begin() + static_cast<ptrdiff_t>(row.index));
                                structureChanged = true;
                            }
                        }

                        if (!structureChanged)
                        {
                            changed |= Internal::Draw_MaterialScalarTimeSourceProperty(
                                detailContext,
                                "시간 축",
                                modulator.timeSource,
                                defaultData.timeSource
                            );
                        }
                        detailContext.End_PropertyTable();
                    }

                    if (!structureChanged)
                    {
                        changed |= detailContext.Draw_Vector3DistributionGroup(
                            "분포",
                            modulator.distribution,
                            defaultData.distribution
                        );

                        if (ImGui::Button("삭제"))
                        {
                            data.coreColorRgbModulators.erase(data.coreColorRgbModulators.begin() + static_cast<ptrdiff_t>(row.index));
                            changed = true;
                            structureChanged = true;
                        }
                    }
                }
                else
                {
                    MaterialScalarModulatorData& modulator = data.modulators[row.index];
                    const MaterialScalarModulatorData defaultData{};

                    if (detailContext.Begin_PropertyTable("MaterialScalarModulator"))
                    {
                        changed |= detailContext.Draw_BoolProperty("사용", modulator.enabled, defaultData.enabled);

                        bool nextCoreColorRgb = false;
                        MaterialScalarModulationTargetField nextTarget = modulator.targetField;
                        if (Internal::Draw_MaterialModulationTargetProperty(
                            detailContext,
                            "대상",
                            false,
                            modulator.targetField,
                            nextCoreColorRgb,
                            nextTarget
                        ))
                        {
                            changed = true;
                            if (nextCoreColorRgb)
                            {
                                MaterialCoreColorRgbModulatorData coreColorRgbModulator = Internal::Make_CoreColorRgbModulator(emitter);
                                coreColorRgbModulator.enabled = modulator.enabled;
                                coreColorRgbModulator.timeSource = modulator.timeSource;
                                data.coreColorRgbModulators.push_back(coreColorRgbModulator);
                                data.modulators.erase(data.modulators.begin() + static_cast<ptrdiff_t>(row.index));
                                structureChanged = true;
                            }
                            else
                                modulator.targetField = nextTarget;
                        }

                        if (!structureChanged)
                        {
                            detailContext.Draw_PropertyLabel("연산");
                            ImGui::TextUnformatted(Internal::Get_MaterialScalarOperationLabel(modulator.operation));
                            if (detailContext.Draw_ResetButton(modulator.operation != defaultData.operation))
                            {
                                modulator.operation = defaultData.operation;
                                changed = true;
                            }
                            changed |= Internal::Draw_MaterialScalarTimeSourceProperty(
                                detailContext,
                                "시간 축",
                                modulator.timeSource,
                                defaultData.timeSource
                            );
                        }
                        detailContext.End_PropertyTable();
                    }

                    if (!structureChanged)
                    {
                        changed |= detailContext.Draw_FloatDistributionGroup(
                            "분포",
                            modulator.distribution,
                            defaultData.distribution
                        );

                        if (ImGui::Button("삭제"))
                        {
                            data.modulators.erase(data.modulators.begin() + static_cast<ptrdiff_t>(row.index));
                            changed = true;
                            structureChanged = true;
                        }
                    }
                }
            }

            ImGui::PopID();
            ImGui::PopID();

            if (structureChanged)
                break;
        }
    }

    return changed;
}

NS_END
