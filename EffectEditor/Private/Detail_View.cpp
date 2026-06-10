#include "Detail_View.h"

#include "DetailPropertyContext.h"
#include "Editor_Context.h"
#include "EffectEditorInstance.h"
#include "GameInstance.h"
#include "Helper_String.h"
#include "MeshDataPreview_View.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    constexpr TrailCurveQuality kTrailCurveQualityValues[] = {
        TrailCurveQuality::Basic,
        TrailCurveQuality::Smooth,
        TrailCurveQuality::HighQuality,
        TrailCurveQuality::Custom,
    };

    static const char* Get_TrailCurveQualityLabel(TrailCurveQuality value)
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

    static const char* Get_TrailCurveQualityTooltip(TrailCurveQuality value)
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

    static const char* Get_SourceHistoryPreviewMotionPresetLabel(SourceHistoryPreviewMotionPreset value)
    {
        switch (value)
        {
        case SourceHistoryPreviewMotionPreset::Off:
            return "Off";
        case SourceHistoryPreviewMotionPreset::Line:
            return "Line";
        case SourceHistoryPreviewMotionPreset::Arc:
            return "Arc";
        case SourceHistoryPreviewMotionPreset::Loop:
            return "Loop";
        default:
            return "Off";
        }
    }

    static const char* Get_SourceHistoryPreviewMotionAxisLabel(SourceHistoryPreviewMotionAxis value)
    {
        switch (value)
        {
        case SourceHistoryPreviewMotionAxis::X:
            return "X";
        case SourceHistoryPreviewMotionAxis::Y:
            return "Y";
        case SourceHistoryPreviewMotionAxis::Z:
            return "Z";
        default:
            return "X";
        }
    }

    static const char* Get_SourceHistoryRibbonSourceModeLabel(SourceHistoryRibbonSourceMode value)
    {
        switch (value)
        {
        case SourceHistoryRibbonSourceMode::SelfRoot:
            return "Self Root";
        case SourceHistoryRibbonSourceMode::ParticleEmitter:
            return "Particle Emitter";
        case SourceHistoryRibbonSourceMode::SourceEmitter:
            return "Source Emitter (Legacy)";
        default:
            return "Self Root";
        }
    }

    static const char* Get_RenderLayerOverrideLabel(EmitterRenderLayerOverride value)
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

    static const char* Get_TrailPreviewGateModeLabel(TrailPreviewGateMode value)
    {
        switch (value)
        {
        case TrailPreviewGateMode::Always:
            return "항상";
        case TrailPreviewGateMode::Window:
            return "구간";
        default:
            return "항상";
        }
    }

    static bool Draw_TrailPreviewGateModeProperty(
        DetailPropertyContext& detailContext,
        TrailPreviewGateMode& value,
        TrailPreviewGateMode defaultValue)
    {
        static constexpr TrailPreviewGateMode kValues[] = {
            TrailPreviewGateMode::Always,
            TrailPreviewGateMode::Window,
        };

        ImGui::PushID("TrailPreviewGateMode");
        detailContext.Draw_PropertyLabel("활성 방식", "트레일 프리뷰 sample을 항상 받거나 Play 타임라인 비율 구간에서만 받습니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);

        bool changed = false;
        if (ImGui::BeginCombo("##Value", Get_TrailPreviewGateModeLabel(value)))
        {
            for (const TrailPreviewGateMode candidate : kValues)
            {
                const bool selected = value == candidate;
                if (ImGui::Selectable(Get_TrailPreviewGateModeLabel(candidate), selected))
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

    static bool Draw_TrailPreviewGateRatioProperty(
        DetailPropertyContext& detailContext,
        const char* label,
        float& value,
        float defaultValue,
        const char* tooltip)
    {
        ImGui::PushID(label);
        detailContext.Draw_PropertyLabel(label, tooltip);
        ImGui::SetNextItemWidth(-FLT_MIN);

        bool changed = ImGui::DragFloat("##Value", &value, 0.001f, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        if (detailContext.Draw_ResetButton(fabs(value - defaultValue) > 0.0001f))
        {
            value = defaultValue;
            changed = true;
        }

        ImGui::PopID();

        if (changed)
            value = clamp(value, 0.f, 1.f);

        return changed;
    }

    static bool Draw_RenderLayerOverrideProperty(
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

    static bool Draw_SourceHistoryPreviewMotionPresetProperty(
        DetailPropertyContext& detailContext,
        SourceHistoryPreviewMotionPreset& value,
        SourceHistoryPreviewMotionPreset defaultValue)
    {
        static constexpr SourceHistoryPreviewMotionPreset kValues[] = {
            SourceHistoryPreviewMotionPreset::Off,
            SourceHistoryPreviewMotionPreset::Line,
            SourceHistoryPreviewMotionPreset::Arc,
            SourceHistoryPreviewMotionPreset::Loop,
        };

        ImGui::PushID("SourceHistoryPreviewMotionPreset");
        detailContext.Draw_PropertyLabel("프리뷰 이동");
        ImGui::SetNextItemWidth(-FLT_MIN);

        bool changed = false;
        if (ImGui::BeginCombo("##Value", Get_SourceHistoryPreviewMotionPresetLabel(value)))
        {
            for (const SourceHistoryPreviewMotionPreset candidate : kValues)
            {
                const bool selected = value == candidate;
                if (ImGui::Selectable(Get_SourceHistoryPreviewMotionPresetLabel(candidate), selected))
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

    static bool Draw_SourceHistoryPreviewMotionAxisProperty(
        DetailPropertyContext& detailContext,
        SourceHistoryPreviewMotionAxis& value,
        SourceHistoryPreviewMotionAxis defaultValue)
    {
        static constexpr SourceHistoryPreviewMotionAxis kValues[] = {
            SourceHistoryPreviewMotionAxis::X,
            SourceHistoryPreviewMotionAxis::Y,
            SourceHistoryPreviewMotionAxis::Z,
        };

        ImGui::PushID("SourceHistoryPreviewMotionAxis");
        detailContext.Draw_PropertyLabel("프리뷰 이동 축");
        ImGui::SetNextItemWidth(-FLT_MIN);

        bool changed = false;
        if (ImGui::BeginCombo("##Value", Get_SourceHistoryPreviewMotionAxisLabel(value)))
        {
            for (const SourceHistoryPreviewMotionAxis candidate : kValues)
            {
                const bool selected = value == candidate;
                if (ImGui::Selectable(Get_SourceHistoryPreviewMotionAxisLabel(candidate), selected))
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

    static bool Draw_SourceHistoryRibbonSourceModeProperty(
        DetailPropertyContext& detailContext,
        SourceHistoryRibbonSourceMode& value,
        SourceHistoryRibbonSourceMode defaultValue)
    {
        static constexpr SourceHistoryRibbonSourceMode kValues[] = {
            SourceHistoryRibbonSourceMode::SelfRoot,
            SourceHistoryRibbonSourceMode::ParticleEmitter,
        };

        ImGui::PushID("SourceHistoryRibbonSourceMode");
        detailContext.Draw_PropertyLabel("소스 모드", "Particle Emitter는 같은 effect 내부 Sprite/Mesh particle을 follower source로 사용합니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);

        bool changed = false;
        if (ImGui::BeginCombo("##Value", Get_SourceHistoryRibbonSourceModeLabel(value)))
        {
            for (const SourceHistoryRibbonSourceMode candidate : kValues)
            {
                const bool selected = value == candidate;
                if (ImGui::Selectable(Get_SourceHistoryRibbonSourceModeLabel(candidate), selected))
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

        if (value == SourceHistoryRibbonSourceMode::SourceEmitter)
            ImGui::TextDisabled("Legacy Source Emitter root follower value will be treated as Particle Emitter when a source id is set.");

        return changed;
    }

    static bool Can_UseAsSourceHistoryRibbonParticleSource(const AuthoringEmitter& emitter)
    {
        return emitter.typeData.kind == AuthoringTypeDataKind::None ||
               emitter.typeData.kind == AuthoringTypeDataKind::Mesh;
    }

    static const char* Get_SourceHistoryRibbonParticleSourceTypeLabel(const AuthoringEmitter& emitter)
    {
        if (emitter.typeData.kind == AuthoringTypeDataKind::Mesh)
            return "Mesh";

        return "Sprite";
    }

    static string Build_SourceHistoryRibbonSourceEmitterLabel(const AuthoringEmitter& emitter)
    {
        string label = emitter.name.empty() ? "Emitter" : emitter.name;
        label += " (id: ";
        label += to_string(emitter.id);
        label += ", ";
        label += Get_SourceHistoryRibbonParticleSourceTypeLabel(emitter);
        if (!emitter.enabled)
            label += ", disabled";
        label += ")";
        return label;
    }

    static string Resolve_SourceHistoryRibbonSourceEmitterPreviewLabel(
        const vector<AuthoringEmitter>& emitters,
        uint32 currentEmitterId,
        uint32 sourceEmitterId)
    {
        if (sourceEmitterId == 0u)
            return "None";

        for (const AuthoringEmitter& emitter : emitters)
        {
            if (emitter.id == sourceEmitterId)
                return emitter.id == currentEmitterId
                       ? "Self reference is invalid"
                       : Build_SourceHistoryRibbonSourceEmitterLabel(emitter);
        }

        return "Missing emitter (id: " + to_string(sourceEmitterId) + ")";
    }

    static bool Draw_SourceHistoryRibbonSourceEmitterProperty(
        DetailPropertyContext& detailContext,
        const vector<AuthoringEmitter>& emitters,
        const AuthoringEmitter& currentEmitter,
        uint32& sourceEmitterId,
        uint32 defaultValue)
    {
        bool changed = false;
        const string previewLabel =
            Resolve_SourceHistoryRibbonSourceEmitterPreviewLabel(emitters, currentEmitter.id, sourceEmitterId);

        ImGui::PushID("SourceHistoryRibbonSourceEmitter");
        detailContext.Draw_PropertyLabel("소스 Emitter", "같은 effect 내부 Sprite/Mesh particle emitter를 선택합니다.");
        ImGui::SetNextItemWidth(-FLT_MIN);

        if (ImGui::BeginCombo("##Value", previewLabel.c_str()))
        {
            const bool noneSelected = sourceEmitterId == 0u;
            if (ImGui::Selectable("None", noneSelected))
            {
                sourceEmitterId = 0u;
                changed = true;
            }
            if (noneSelected)
                ImGui::SetItemDefaultFocus();

            for (const AuthoringEmitter& candidate : emitters)
            {
                if (candidate.id == currentEmitter.id ||
                    !Can_UseAsSourceHistoryRibbonParticleSource(candidate))
                {
                    continue;
                }

                const string label = Build_SourceHistoryRibbonSourceEmitterLabel(candidate);
                const bool selected = sourceEmitterId == candidate.id;
                if (ImGui::Selectable(label.c_str(), selected))
                {
                    sourceEmitterId = candidate.id;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        const bool resetClicked = detailContext.Draw_ResetButton(sourceEmitterId != defaultValue);
        ImGui::PopID();

        if (resetClicked)
        {
            sourceEmitterId = defaultValue;
            return true;
        }

        return changed;
    }

    static TrailTypeData Build_TrailCurvePreset(TrailCurveQuality quality)
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

    static bool Apply_TrailCurvePreset(TrailTypeData& data)
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

    static fs::path Resolve_AuthoredAssetPath(const string& guid, const string& path)
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

    static string Build_MeshModelValidationMessage(const MeshTypeData& data, ImVec4& outColor)
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

    static bool Draw_MeshDataEntryButton(const MeshTypeData& data)
    {
        constexpr float kEntryHeight{ 52.f };
        const float width = max(180.f, ImGui::GetContentRegionAvail().x);
        const bool hasModel = !data.modelPath.empty();
        const bool hasMaterialSource = !data.assignedEffectMaterialPath.empty();

        ImGui::InvisibleButton("MeshDataEditEntry", ImVec2{ width, kEntryHeight });

        const ImVec2 minPos = ImGui::GetItemRectMin();
        const ImVec2 maxPos = ImGui::GetItemRectMax();
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        const bool clicked = ImGui::IsItemClicked();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 fillColor = active
                                ? IM_COL32(38, 48, 64, 255)
                                : hovered ? IM_COL32(34, 42, 58, 255) : IM_COL32(30, 34, 42, 255);
        const ImU32 borderColor = hovered ? IM_COL32(116, 156, 235, 255) : IM_COL32(70, 78, 92, 255);
        drawList->AddRectFilled(minPos, maxPos, fillColor, 6.f);
        drawList->AddRect(minPos, maxPos, borderColor, 6.f, 0, hovered ? 2.f : 1.f);

        const ImVec2 textMin{ minPos.x + 12.f, minPos.y + 8.f };
        drawList->AddText(textMin, IM_COL32(238, 241, 248, 255), ICON_FA_CUBES " MeshData 편집");
        drawList->AddText(
            ImGui::GetFont(),
            ImGui::GetFontSize() * 0.9f,
            ImVec2{ textMin.x, minPos.y + 30.f },
            IM_COL32(150, 160, 176, 255),
            "모델 / 머티리얼 설정"
        );

        const bool setupReady = hasModel && hasMaterialSource && data.hasAssignedMaterialInstance;
        const string status = width >= 420.f
                              ? string{ hasModel ? "Model OK" : "Model missing" } +
                                " | " +
                                (hasMaterialSource ? "Material OK" : "Material missing") +
                                " | " +
                                (data.hasAssignedMaterialInstance ? "Copy OK" : "Copy missing")
                              : setupReady ? string{ "Ready" } : string{ "Setup needed" };
        const ImVec2 statusSize = ImGui::CalcTextSize(status.c_str());
        const ImVec2 statusPos{
            max(textMin.x, maxPos.x - statusSize.x - 12.f),
            minPos.y + 18.f
        };
        const ImU32 statusColor =
            setupReady
            ? IM_COL32(105, 220, 135, 255)
            : IM_COL32(245, 165, 92, 255);
        drawList->AddText(statusPos, statusColor, status.c_str());

        if (hovered)
            ImGui::SetTooltip("MeshData Preview 창에서 model/material 조합과 내부 material copy를 편집합니다.");

        return clicked;
    }

    static bool Has_PostProcessFeature(PostProcessFeature features, PostProcessFeature feature)
    {
        return Has_Flag(features, feature);
    }

    static const char* Get_PostProcessPresetLabel(EffectEditorPostProcessPreset preset)
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

    static const char* Get_ToneMapModeLabel(uint32 toneMapMode)
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

    static void Draw_PostProcessPresetProperty(DetailPropertyContext& detailContext)
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

    static bool Draw_ToneMapModeProperty(DetailPropertyContext& detailContext, uint32& toneMapMode, uint32 defaultToneMapMode)
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

    static bool Draw_TrailCurveQualityProperty(DetailPropertyContext& detailContext, TrailTypeData& data, const TrailTypeData& defaultData)
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
        if (EDITOR != nullptr)
        {
            bool previewPlaneVisible = EDITOR->Is_PreviewPlaneVisible();
            if (_detailPropertyContext.Draw_BoolProperty("Preview Plane", previewPlaneVisible, false))
                EDITOR->Set_PreviewPlaneVisible(previewPlaneVisible);
        }
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
    TrailPreviewGateMode gateMode = EDITOR->Get_TrailPreviewGateMode();
    float gateStartRatio = EDITOR->Get_TrailPreviewGateStartRatio();
    float gateEndRatio = EDITOR->Get_TrailPreviewGateEndRatio();

    bool changed = false;
    if (_detailPropertyContext.Begin_PropertyTable("TrailPreview"))
    {
        if (_detailPropertyContext.Draw_BoolProperty("트레일 프리뷰", visible, false))
            EDITOR->Set_TrailPreviewVisible(visible);
        if (_detailPropertyContext.Draw_BoolProperty("디버그 렌더", debugRenderEnabled, true))
            EDITOR->Set_TrailPreviewDebugRenderEnabled(debugRenderEnabled);
        changed |= _detailPropertyContext.Draw_Vec3Property("Base 로컬", baseLocalOffset, defaultBaseLocalOffset, 0.005f);
        changed |= _detailPropertyContext.Draw_Vec3Property("Tip 로컬", tipLocalOffset, defaultTipLocalOffset, 0.005f);
        if (Internal::Draw_TrailPreviewGateModeProperty(_detailPropertyContext, gateMode, TrailPreviewGateMode::Window))
            EDITOR->Set_TrailPreviewGateMode(gateMode);
        if (gateMode == TrailPreviewGateMode::Window)
        {
            bool gateWindowChanged = false;
            gateWindowChanged |= Internal::Draw_TrailPreviewGateRatioProperty(
                _detailPropertyContext,
                "시작 비율",
                gateStartRatio,
                EDITOR->Get_DefaultTrailPreviewGateStartRatio(),
                "Trail Preview 공격 모션에서 trail sample 공급을 시작할 normalized ratio입니다. 종료 비율보다 크면 loop 경계를 넘는 구간입니다."
            );
            gateWindowChanged |= Internal::Draw_TrailPreviewGateRatioProperty(
                _detailPropertyContext,
                "종료 비율",
                gateEndRatio,
                EDITOR->Get_DefaultTrailPreviewGateEndRatio(),
                "Trail Preview 공격 모션에서 trail sample 공급을 끝낼 normalized ratio입니다. 시작 비율보다 작으면 loop 초반에서 닫힙니다."
            );
            if (gateWindowChanged)
                EDITOR->Set_TrailPreviewGateWindow(gateStartRatio, gateEndRatio);
        }
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

    bool hdr = Internal::Has_PostProcessFeature(options.features, PostProcessFeature::HDR);
    bool exposure = Internal::Has_PostProcessFeature(options.features, PostProcessFeature::Exposure);
    bool toneMapping = Internal::Has_PostProcessFeature(options.features, PostProcessFeature::ToneMapping);
    bool bloom = Internal::Has_PostProcessFeature(options.features, PostProcessFeature::Bloom);
    bool gamma = Internal::Has_PostProcessFeature(options.features, PostProcessFeature::Gamma);

    const bool defaultHdr = Internal::Has_PostProcessFeature(defaultOptions.features, PostProcessFeature::HDR);
    const bool defaultExposure = Internal::Has_PostProcessFeature(defaultOptions.features, PostProcessFeature::Exposure);
    const bool defaultToneMapping = Internal::Has_PostProcessFeature(defaultOptions.features, PostProcessFeature::ToneMapping);
    const bool defaultBloom = Internal::Has_PostProcessFeature(defaultOptions.features, PostProcessFeature::Bloom);
    const bool defaultGamma = Internal::Has_PostProcessFeature(defaultOptions.features, PostProcessFeature::Gamma);

    if (_detailPropertyContext.Begin_PropertyTable("PostProcessPreset"))
    {
        Internal::Draw_PostProcessPresetProperty(_detailPropertyContext);
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
        paramsChanged |= Internal::Draw_ToneMapModeProperty(_detailPropertyContext, params.toneMapMode, defaultOptions.params.toneMapMode);
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

        changed |= Internal::Draw_RenderLayerOverrideProperty(
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
                changed |= _detailPropertyContext.Draw_BoolProperty(
                    "자동 수명 페이드",
                    data->autoLifeFade,
                    defaultData.autoLifeFade,
                    "trail sample age 기준 alpha fade를 적용할지 결정합니다. 꺼도 세그먼트 수명이 지나면 sample은 제거됩니다."
                );
                _detailPropertyContext.End_PropertyTable();
            }
        }

        if (ImGui::CollapsingHeader("곡선 품질", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (_detailPropertyContext.Begin_PropertyTable("TrailTypeDataCurveQuality"))
            {
                changed |= Internal::Draw_TrailCurveQualityProperty(_detailPropertyContext, *data, defaultData);
                changed |= Internal::Apply_TrailCurvePreset(*data);

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

        if (Internal::Draw_MeshDataEntryButton(*data))
        {
            const Shared<Editor_Window> meshPreviewWindow = EDITOR != nullptr ? EDITOR->Get_Window(L"MeshData Preview") : nullptr;
            const Shared<MeshDataPreview_View> meshPreviewView = dynamic_pointer_cast<MeshDataPreview_View>(meshPreviewWindow);
            if (meshPreviewView != nullptr)
                meshPreviewView->Open_Target(emitter.id);
        }

        ImVec4 validationColor{};
        const string validationMessage = Internal::Build_MeshModelValidationMessage(*data, validationColor);
        if (validationMessage != "Model asset is valid for authoring.")
        {
            ImGui::PushStyleColor(ImGuiCol_Text, validationColor);
            ImGui::TextWrapped("%s", validationMessage.c_str());
            ImGui::PopStyleColor();
        }

        break;
    }
    case AuthoringTypeDataKind::SourceHistoryRibbon:
    {
        SourceHistoryRibbonTypeData* data = get_if<SourceHistoryRibbonTypeData>(&emitter.typeData.payload);
        if (data == nullptr)
        {
            ImGui::TextDisabled("리본 TypeData payload is invalid.");
            break;
        }

        const SourceHistoryRibbonTypeData defaultData{};
        if (ImGui::CollapsingHeader("리본", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (_detailPropertyContext.Begin_PropertyTable("SourceHistoryRibbonSource"))
            {
                changed |= Internal::Draw_SourceHistoryRibbonSourceModeProperty(
                    _detailPropertyContext,
                    data->sourceMode,
                    defaultData.sourceMode
                );

                if (data->sourceMode == SourceHistoryRibbonSourceMode::SourceEmitter)
                {
                    data->sourceMode = data->sourceEmitterId != 0u
                                       ? SourceHistoryRibbonSourceMode::ParticleEmitter
                                       : SourceHistoryRibbonSourceMode::SelfRoot;
                    changed = true;
                }

                if (data->sourceMode == SourceHistoryRibbonSourceMode::SelfRoot && data->sourceEmitterId != 0u)
                {
                    data->sourceEmitterId = 0u;
                    changed = true;
                }
                else if (data->sourceEmitterId == emitter.id)
                {
                    data->sourceEmitterId = 0u;
                    changed = true;
                }

                if (data->sourceMode == SourceHistoryRibbonSourceMode::ParticleEmitter)
                {
                    changed |= Internal::Draw_SourceHistoryRibbonSourceEmitterProperty(
                        _detailPropertyContext,
                        emitterView->Get_Emitters(),
                        emitter,
                        data->sourceEmitterId,
                        defaultData.sourceEmitterId
                    );
                    changed |= _detailPropertyContext.Draw_UintProperty(
                        "Follower Lane 수",
                        data->followerLaneCount,
                        defaultData.followerLaneCount,
                        "동시에 따라갈 representative particle lane 상한입니다."
                    );
                    data->followerLaneCount = max(1u, data->followerLaneCount);
                }

                _detailPropertyContext.End_PropertyTable();
            }

            if (_detailPropertyContext.Begin_PropertyTable("SourceHistoryRibbonTypeData"))
            {
                changed |= _detailPropertyContext.Draw_UintProperty(
                    "히스토리 수",
                    data->maxSampleCount,
                    defaultData.maxSampleCount,
                    "소스 위치 히스토리가 보관할 최대 샘플 개수입니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "샘플 간격",
                    data->sampleSpacing,
                    defaultData.sampleSpacing,
                    0.005f,
                    "소스가 이 거리 이상 움직였을 때 새 샘플을 추가합니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "샘플 보강 간격",
                    data->sampleInterval,
                    defaultData.sampleInterval,
                    0.005f,
                    "0보다 크면 이동이 작아도 이 시간마다 샘플을 보강합니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "최대 표시 길이",
                    data->maxLength,
                    defaultData.maxLength,
                    0.01f,
                    "머리 기준 누적 거리로 제한하는 최대 표시 길이입니다. 0 이하면 제한하지 않습니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "꼬리 페이드 길이",
                    data->tailFadeLength,
                    defaultData.tailFadeLength,
                    0.01f,
                    "최대 표시 길이 끝으로 갈수록 alpha를 줄이는 거리입니다. 최대 표시 길이가 0이거나 이 값이 0이면 적용하지 않습니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "기본 폭",
                    data->baseWidth,
                    defaultData.baseWidth,
                    0.01f,
                    "SizeByLife를 곱하기 전 소스 히스토리 strip 기본 폭입니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "타일링 거리",
                    data->tilingDistance,
                    defaultData.tilingDistance,
                    0.01f,
                    "0 이하면 전체 표시 길이에 맞춰 늘이고, 양수면 거리 기준으로 texture를 반복합니다."
                );
                changed |= _detailPropertyContext.Draw_BoolProperty(
                    "Auto Life Fade",
                    data->autoLifeFade,
                    defaultData.autoLifeFade,
                    "켜면 샘플 나이에 따라 알파가 자동 감소합니다."
                );
                _detailPropertyContext.End_PropertyTable();
            }

            ImGui::TextDisabled("최종 알파는 ColorOverLife 알파에 Auto Life Fade와 Tail Fade가 추가로 곱해질 수 있습니다.");
        }

        if (ImGui::CollapsingHeader("Preview Motion (에디터 내 확인용 수치. 저장 X)", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (_detailPropertyContext.Begin_PropertyTable("SourceHistoryRibbonPreviewMotion"))
            {
                changed |= Internal::Draw_SourceHistoryPreviewMotionPresetProperty(
                    _detailPropertyContext,
                    data->previewMotionPreset,
                    defaultData.previewMotionPreset
                );
                changed |= Internal::Draw_SourceHistoryPreviewMotionAxisProperty(
                    _detailPropertyContext,
                    data->previewMotionAxis,
                    defaultData.previewMotionAxis
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "프리뷰 이동 크기",
                    data->previewMotionAmplitude,
                    defaultData.previewMotionAmplitude,
                    0.01f,
                    "Line/Arc/Loop 프리뷰 이동 오프셋 크기입니다."
                );
                changed |= _detailPropertyContext.Draw_FloatProperty(
                    "프리뷰 이동 속도",
                    data->previewMotionSpeed,
                    defaultData.previewMotionSpeed,
                    0.01f,
                    "Line/Arc는 한 번 이동하는 속도, Loop는 반복 속도입니다."
                );
                _detailPropertyContext.End_PropertyTable();
            }
        }

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

    case AuthoringModuleType::OrbitOverLife:
        if (auto* data = get_if<OrbitOverLifeModuleData>(&module.data))
            changed = _orbitOverLifeModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::InitialRotation:
        if (auto* data = get_if<InitialRotationModuleData>(&module.data))
            changed = _initialRotationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
        break;

    case AuthoringModuleType::SphereRadialOrientation:
        if (auto* data = get_if<SphereRadialOrientationModuleData>(&module.data))
            changed = _sphereRadialOrientationModuleDetail.Draw(emitter, module, *data, _detailPropertyContext);
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
