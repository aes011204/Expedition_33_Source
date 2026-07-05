#include "HistoryBudget_View.h"

#include "Emitter_View.h"
#include "EffectEditorInstance.h"
#include "EffectEditorPreviewDefinitionBuilder.h"
#include "GameInstance.h"

NS_BEGIN(EffectEditor)

namespace
{
    constexpr float kHighlightDuration{ 1.5f };

    const char* Get_PresetLabel(HistoryBudgetPreset value)
    {
        switch (value)
        {
        case HistoryBudgetPreset::High:
            return "High";
        case HistoryBudgetPreset::Medium:
            return "Medium";
        case HistoryBudgetPreset::Performance:
            return "Performance";
        case HistoryBudgetPreset::Low:
            return "Low";
        case HistoryBudgetPreset::Full:
        default:
            return "Full";
        }
    }

    const char* Get_PriorityLabel(HistoryBudgetPriority value)
    {
        switch (value)
        {
        case HistoryBudgetPriority::Secondary:
            return "Secondary";
        case HistoryBudgetPriority::Decorative:
            return "Decorative";
        case HistoryBudgetPriority::Distortion:
            return "Distortion";
        case HistoryBudgetPriority::Core:
        default:
            return "Core";
        }
    }

    const char* Get_DensityBiasLabel(HistoryBudgetDensityBias value)
    {
        switch (value)
        {
        case HistoryBudgetDensityBias::Dense:
            return "Dense";
        case HistoryBudgetDensityBias::Sparse:
            return "Sparse";
        case HistoryBudgetDensityBias::Normal:
        default:
            return "Normal";
        }
    }

    const char* Get_TypeLabel(AuthoringTypeDataKind value)
    {
        switch (value)
        {
        case AuthoringTypeDataKind::Trail:
            return "Trail";
        case AuthoringTypeDataKind::Ribbon:
            return "Ribbon";
        case AuthoringTypeDataKind::SourceHistorySpriteTrail:
            return "SpriteTrail";
        case AuthoringTypeDataKind::Mesh:
            return "Mesh";
        case AuthoringTypeDataKind::Beam:
            return "Beam";
        case AuthoringTypeDataKind::None:
        default:
            return "Sprite";
        }
    }

    bool Is_HistoryFamily(AuthoringTypeDataKind value)
    {
        return value == AuthoringTypeDataKind::Trail ||
               value == AuthoringTypeDataKind::Ribbon ||
               value == AuthoringTypeDataKind::SourceHistorySpriteTrail;
    }

    const RequiredModuleData* Find_RequiredModule(const AuthoringEmitter& emitter)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type != AuthoringModuleType::Required)
                continue;

            return get_if<RequiredModuleData>(&module.data);
        }

        return nullptr;
    }

    bool Has_DistortionSignal(const AuthoringEmitter& emitter)
    {
        if (emitter.historyBudget.priority == HistoryBudgetPriority::Distortion)
            return true;

        const RequiredModuleData* required = Find_RequiredModule(emitter);
        return required != nullptr &&
               required->material.materialFamily == EffectMaterialFamily::SpriteDistortion;
    }

    string Format_Counts(const EffectHistoryBudgetEffectiveDesc& data)
    {
        return format("{} / {} / {}", data.sourceSampleCount, data.renderSegmentCount, data.spriteStampCount);
    }

    const EffectEmitterDefinition* Find_Definition(const Shared<const EffectDefinition>& definition, uint32 emitterId)
    {
        if (definition == nullptr)
            return nullptr;

        for (const EffectEmitterDefinition& emitter : definition->emitters)
        {
            if (emitter.id == emitterId)
                return &emitter;
        }

        return nullptr;
    }

    EffectHistoryBudgetEffectiveDesc Compute_TargetEffective(
        const EffectDefinition& budgetDefinition,
        const EffectEmitterDefinition& fullEmitter)
    {
        return Compute_EffectHistoryBudgetEffective(
            budgetDefinition.historyBudget,
            fullEmitter.historyBudget,
            EffectHistoryBudgetEffectiveInput{
                fullEmitter.effectiveHistoryBudget.family,
                fullEmitter.effectiveHistoryBudget.sourceSampleCount,
                fullEmitter.effectiveHistoryBudget.renderSegmentCount,
                fullEmitter.effectiveHistoryBudget.spriteStampCount
            }
        );
    }

    string Build_WarningText(
        const AuthoringEmitter& emitter,
        const EffectHistoryBudgetEffectiveDesc* target,
        const EffectHistoryBudgetEffectiveDesc* actual,
        uint32 historyFamilyCount)
    {
        vector<string> warnings{};
        if (target != nullptr && actual != nullptr)
        {
            if (target->sourceSampleCount < actual->sourceSampleCount)
                warnings.push_back("retention-preserved");
            if (target->renderSegmentCount < actual->renderSegmentCount ||
                target->spriteStampCount < actual->spriteStampCount)
            {
                warnings.push_back("floor-limited");
            }
        }

        if (Is_HistoryFamily(emitter.typeData.kind) && historyFamilyCount > 1u)
            warnings.push_back("shared-history-candidate");

        if (Has_DistortionSignal(emitter))
            warnings.push_back("distortion-auto-scaling-deferred");

        if (warnings.empty())
            return "-";

        string text = warnings.front();
        for (size_t index = 1; index < warnings.size(); ++index)
            text += ", " + warnings[index];
        return text;
    }

    const char* Get_SourceGroupKindLabel(EffectHistorySourceGroupKind value)
    {
        switch (value)
        {
        case EffectHistorySourceGroupKind::TrailPairHistory:
            return "TrailPairHistory";
        case EffectHistorySourceGroupKind::SourcePointHistory:
            return "SourcePointHistory";
        case EffectHistorySourceGroupKind::None:
        default:
            return "None";
        }
    }

    string Build_SourceGroupId(const AuthoringEmitter& emitter)
    {
        switch (emitter.typeData.kind)
        {
        case AuthoringTypeDataKind::Trail:
            return "TrailPairHistory|PreviewTrailProvider";
        case AuthoringTypeDataKind::Ribbon:
        {
            const RibbonTypeData* data = get_if<RibbonTypeData>(&emitter.typeData.payload);
            if (data == nullptr)
                return {};

            if (data->sourceMode == SourceHistoryRibbonSourceMode::ParticleEmitter)
                return format("SourcePointHistory|ParticleEmitter:{}", data->sourceEmitterId);

            return "SourcePointHistory|SelfRoot";
        }
        case AuthoringTypeDataKind::SourceHistorySpriteTrail:
        {
            const SourceHistorySpriteTrailTypeData* data = get_if<SourceHistorySpriteTrailTypeData>(&emitter.typeData.payload);
            if (data == nullptr)
                return {};

            if (data->sourceMode == SourceHistoryRibbonSourceMode::ParticleEmitter)
                return format("SourcePointHistory|ParticleEmitter:{}", data->sourceEmitterId);

            return "SourcePointHistory|SelfRoot";
        }
        default:
            return {};
        }
    }

    EffectHistorySourceGroupKind Resolve_SourceGroupKind(const string& groupId)
    {
        if (groupId.starts_with("TrailPairHistory|"))
            return EffectHistorySourceGroupKind::TrailPairHistory;
        if (groupId.starts_with("SourcePointHistory|"))
            return EffectHistorySourceGroupKind::SourcePointHistory;
        return EffectHistorySourceGroupKind::None;
    }

    string Build_SourceGroupLabel(const string& groupId)
    {
        const size_t separator = groupId.find('|');
        if (separator == string::npos || separator + 1u >= groupId.size())
            return groupId;

        return groupId.substr(separator + 1u);
    }

    struct EmitterBudgetRow
    {
        const AuthoringEmitter* emitter{};
        const EffectEmitterDefinition* fullEmitter{};
        const EffectEmitterDefinition* actualEmitter{};
        optional<EffectHistoryBudgetEffectiveDesc> targetEffective{};
        string groupId{};
    };

    struct SourceGroupRow
    {
        string id{};
        string label{};
        EffectHistorySourceGroupKind kind{ EffectHistorySourceGroupKind::None };
        vector<size_t> rowIndices{};
        uint32 currentRequestCount{};
        uint32 sharedRequestCount{};
        uint32 savedRequestCount{};
        uint32 currentSourceSamples{};
        uint32 sharedSourceSamples{};
        uint32 savedSourceSamples{};
        bool hasRibbon{};
        bool hasSpriteTrail{};
        bool hasDistortion{};
    };

    vector<EmitterBudgetRow> Build_EmitterBudgetRows(
        const vector<AuthoringEmitter>& emitters,
        const Shared<const EffectDefinition>& fullDefinition,
        const Shared<const EffectDefinition>& actualDefinition)
    {
        vector<EmitterBudgetRow> rows{};
        rows.reserve(emitters.size());

        for (const AuthoringEmitter& emitter : emitters)
        {
            if (!emitter.enabled || !Is_HistoryFamily(emitter.typeData.kind))
                continue;

            const string groupId = Build_SourceGroupId(emitter);
            if (groupId.empty())
                continue;

            const EffectEmitterDefinition* fullEmitter = Find_Definition(fullDefinition, emitter.id);
            const EffectEmitterDefinition* actualEmitter = Find_Definition(actualDefinition, emitter.id);

            optional<EffectHistoryBudgetEffectiveDesc> targetEffective{};
            if (fullEmitter != nullptr && actualDefinition != nullptr)
                targetEffective = Compute_TargetEffective(*actualDefinition, *fullEmitter);

            rows.push_back(
                EmitterBudgetRow{
                    .emitter = &emitter,
                    .fullEmitter = fullEmitter,
                    .actualEmitter = actualEmitter,
                    .targetEffective = targetEffective,
                    .groupId = groupId
                }
            );
        }

        return rows;
    }

    vector<SourceGroupRow> Build_SourceGroupRows(const vector<EmitterBudgetRow>& emitterRows)
    {
        vector<SourceGroupRow> groups{};

        for (size_t rowIndex = 0; rowIndex < emitterRows.size(); ++rowIndex)
        {
            const EmitterBudgetRow& emitterRow = emitterRows[rowIndex];
            const AuthoringEmitter* emitter = emitterRow.emitter;
            if (emitter == nullptr)
                continue;

            auto groupIter = ranges::find_if(
                groups,
                [&emitterRow](const SourceGroupRow& group)
                {
                    return group.id == emitterRow.groupId;
                }
            );
            if (groupIter == groups.end())
            {
                SourceGroupRow group{};
                group.id = emitterRow.groupId;
                group.label = Build_SourceGroupLabel(emitterRow.groupId);
                group.kind = Resolve_SourceGroupKind(emitterRow.groupId);
                groups.push_back(group);
                groupIter = prev(groups.end());
            }

            SourceGroupRow& group = *groupIter;
            group.rowIndices.push_back(rowIndex);
            group.currentRequestCount++;
            group.sharedRequestCount = 1u;

            const EffectHistoryBudgetEffectiveDesc* actual =
                emitterRow.actualEmitter != nullptr ? &emitterRow.actualEmitter->effectiveHistoryBudget : nullptr;
            if (actual != nullptr)
            {
                group.currentSourceSamples += actual->sourceSampleCount;
                group.sharedSourceSamples = max(group.sharedSourceSamples, actual->sourceSampleCount);
            }

            group.hasRibbon = group.hasRibbon || emitter->typeData.kind == AuthoringTypeDataKind::Ribbon;
            group.hasSpriteTrail = group.hasSpriteTrail || emitter->typeData.kind == AuthoringTypeDataKind::SourceHistorySpriteTrail;
            group.hasDistortion = group.hasDistortion || Has_DistortionSignal(*emitter);
        }

        for (SourceGroupRow& group : groups)
        {
            group.savedRequestCount = group.currentRequestCount > group.sharedRequestCount
                                      ? group.currentRequestCount - group.sharedRequestCount
                                      : 0u;
            group.savedSourceSamples = group.currentSourceSamples > group.sharedSourceSamples
                                       ? group.currentSourceSamples - group.sharedSourceSamples
                                       : 0u;
        }

        return groups;
    }

    string Build_SourceGroupWarningText(const SourceGroupRow& group, bool sharedSourceHistoryEnabled)
    {
        vector<string> warnings{};
        if (group.currentRequestCount <= 1u)
            warnings.push_back("single-emitter");
        else if (sharedSourceHistoryEnabled)
            warnings.push_back("shared-provider-cache-active");
        else
            warnings.push_back("shared-provider-cache-disabled");

        if (group.hasRibbon && group.hasSpriteTrail)
            warnings.push_back("cross-family-source-point");
        if (group.hasDistortion)
            warnings.push_back("distortion-auto-scaling-deferred");

        string text = warnings.front();
        for (size_t index = 1; index < warnings.size(); ++index)
            text += ", " + warnings[index];
        return text;
    }

    bool Draw_PresetCombo(HistoryBudgetPreset& value)
    {
        bool changed = false;
        constexpr HistoryBudgetPreset kValues[]{
            HistoryBudgetPreset::Full,
            HistoryBudgetPreset::High,
            HistoryBudgetPreset::Medium,
            HistoryBudgetPreset::Performance,
            HistoryBudgetPreset::Low
        };

        if (ImGui::BeginCombo("Preset", Get_PresetLabel(value)))
        {
            for (const HistoryBudgetPreset candidate : kValues)
            {
                const bool selected = value == candidate;
                if (ImGui::Selectable(Get_PresetLabel(candidate), selected))
                {
                    value = candidate;
                    changed = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        return changed;
    }
}

HistoryBudget_View::HistoryBudget_View()
    : Editor_Window{ L"History Budget", ICON_FA_CHART_SIMPLE }
{
    Set_Open(false);
}

void HistoryBudget_View::Update(float timeDelta)
{
    if (_highlightTimer > 0.f)
        _highlightTimer = max(0.f, _highlightTimer - max(0.f, timeDelta));

    if (_highlightTimer <= 0.f)
        _highlightEmitterId.reset();
}

void HistoryBudget_View::Render()
{
    bool isOpen = Is_Open();
    if (!isOpen)
        return;

    ImGui::SetNextWindowSize(ImVec2{ 900.f, 520.f }, ImGuiCond_FirstUseEver);
    Apply_PendingFocusBeforeBegin();
    if (ImGui::Begin(Get_ImGuiWindowName().c_str(), &isOpen))
    {
        Clear_PendingFocusAfterBegin();
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        const Shared<Emitter_View> emitterView = Get_EmitterView();
        if (emitterView == nullptr)
        {
            ImGui::TextDisabled("Emitter view is unavailable.");
        }
        else
        {
            Draw_GlobalBudget(*emitterView);
            ImGui::Separator();
            Draw_SourceGroupSummary(*emitterView);
        }
    }
    ImGui::End();

    Set_Open(isOpen);
}

void HistoryBudget_View::Open_Target(uint32 emitterId)
{
    Set_Open(true);
    Request_FocusOnOpen();
    _pendingFocusEmitterId = emitterId;
    _highlightEmitterId = emitterId;
    _highlightTimer = kHighlightDuration;
}

Shared<Emitter_View> HistoryBudget_View::Get_EmitterView() const
{
    if (EDITOR == nullptr)
        return nullptr;

    return dynamic_pointer_cast<Emitter_View>(EDITOR->Get_Window(L"Emitter"));
}

void HistoryBudget_View::Draw_GlobalBudget(Emitter_View& emitterView)
{
    HistoryBudgetData nextBudget = emitterView.Get_HistoryBudget();
    bool changed = false;

    ImGui::TextUnformatted("Global Budget");
    ImGui::PushItemWidth(180.f);
    changed |= Draw_PresetCombo(nextBudget.preset);
    ImGui::SameLine(0.f, 18.f);
    changed |= ImGui::Checkbox("Preserve Length", &nextBudget.preserveLength);
    ImGui::SameLine(0.f, 18.f);
    changed |= ImGui::Checkbox("Shared Source History", &nextBudget.sharedSourceHistory);

    ImGui::SetNextItemWidth(120.f);
    changed |= ImGui::DragFloat("Trail Density", &nextBudget.trailDensityScale, 0.01f, 0.f, 2.f, "%.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.f);
    changed |= ImGui::DragFloat("Ribbon Density", &nextBudget.ribbonDensityScale, 0.01f, 0.f, 2.f, "%.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.f);
    changed |= ImGui::DragFloat("Sprite Stamp Density", &nextBudget.spriteStampDensityScale, 0.01f, 0.f, 2.f, "%.2f");
    ImGui::PopItemWidth();

    nextBudget.trailDensityScale = max(0.f, nextBudget.trailDensityScale);
    nextBudget.ribbonDensityScale = max(0.f, nextBudget.ribbonDensityScale);
    nextBudget.spriteStampDensityScale = max(0.f, nextBudget.spriteStampDensityScale);

    if (changed)
        emitterView.Set_HistoryBudget(nextBudget, "Edit History Budget");
}

void HistoryBudget_View::Draw_SourceGroupSummary(Emitter_View& emitterView)
{
    const vector<AuthoringEmitter>& emitters = emitterView.Get_Emitters();
    uint32 historyFamilyCount = 0u;
    for (const AuthoringEmitter& emitter : emitters)
    {
        if (emitter.enabled && Is_HistoryFamily(emitter.typeData.kind))
            ++historyFamilyCount;
    }

    HistoryBudgetData fullBudget = emitterView.Get_HistoryBudget();
    fullBudget.preset = HistoryBudgetPreset::Full;

    const Shared<const EffectDefinition> fullDefinition =
        PreviewDefinition::Build_EffectDefinition(emitters, fullBudget, true);
    const Shared<const EffectDefinition> actualDefinition =
        PreviewDefinition::Build_EffectDefinition(emitters, emitterView.Get_HistoryBudget(), true);

    const vector<EmitterBudgetRow> emitterRows = Build_EmitterBudgetRows(emitters, fullDefinition, actualDefinition);
    const vector<SourceGroupRow> sourceGroups = Build_SourceGroupRows(emitterRows);

    if (_pendingFocusEmitterId.has_value())
    {
        for (const EmitterBudgetRow& row : emitterRows)
        {
            if (row.emitter != nullptr && row.emitter->id == _pendingFocusEmitterId.value())
            {
                _selectedSourceGroupId = row.groupId;
                break;
            }
        }
    }

    const bool selectedGroupValid = ranges::any_of(
        sourceGroups,
        [this](const SourceGroupRow& group)
        {
            return group.id == _selectedSourceGroupId;
        }
    );
    if (!selectedGroupValid)
        _selectedSourceGroupId = sourceGroups.empty() ? string{} : sourceGroups.front().id;

    ImGui::TextUnformatted("Source Groups");
    constexpr ImGuiTableFlags tableFlags =
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("HistoryBudgetSourceGroups", 7, tableFlags, ImVec2(0.f, 150.f)))
    {
        ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 210.f);
        ImGui::TableSetupColumn("Family", ImGuiTableColumnFlags_WidthFixed, 130.f);
        ImGui::TableSetupColumn("Emitters", ImGuiTableColumnFlags_WidthFixed, 70.f);
        ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed, 140.f);
        ImGui::TableSetupColumn("Shared Target", ImGuiTableColumnFlags_WidthFixed, 140.f);
        ImGui::TableSetupColumn("Saved", ImGuiTableColumnFlags_WidthFixed, 120.f);
        ImGui::TableSetupColumn("Warnings", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const SourceGroupRow& group : sourceGroups)
        {
            ImGui::TableNextRow();
            if (group.id == _selectedSourceGroupId)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(62, 92, 130, 115));

            ImGui::TableSetColumnIndex(0);
            const bool selected = group.id == _selectedSourceGroupId;
            if (ImGui::Selectable(group.label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
                _selectedSourceGroupId = group.id;
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(Get_SourceGroupKindLabel(group.kind));
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%u", static_cast<uint32>(group.rowIndices.size()));
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%u calls / %u src", group.currentRequestCount, group.currentSourceSamples);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%u call / %u src", group.sharedRequestCount, group.sharedSourceSamples);
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%u / %u", group.savedRequestCount, group.savedSourceSamples);
            ImGui::TableSetColumnIndex(6);
            const string warningText = Build_SourceGroupWarningText(group, emitterView.Get_HistoryBudget().sharedSourceHistory);
            ImGui::TextWrapped("%s", warningText.c_str());
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Emitter Breakdown");
    if (!ImGui::BeginTable("HistoryBudgetEmitterBreakdown", 8, tableFlags, ImVec2(0.f, 0.f)))
        return;

    ImGui::TableSetupColumn("Emitter", ImGuiTableColumnFlags_WidthFixed, 190.f);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 86.f);
    ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed, 86.f);
    ImGui::TableSetupColumn("Bias", ImGuiTableColumnFlags_WidthFixed, 70.f);
    ImGui::TableSetupColumn("Full s/r/st", ImGuiTableColumnFlags_WidthFixed, 105.f);
    ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthFixed, 105.f);
    ImGui::TableSetupColumn("Actual", ImGuiTableColumnFlags_WidthFixed, 105.f);
    ImGui::TableSetupColumn("Warnings", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    for (const EmitterBudgetRow& row : emitterRows)
    {
        if (row.groupId != _selectedSourceGroupId)
            continue;

        const AuthoringEmitter* emitter = row.emitter;
        if (emitter == nullptr)
            continue;

        const EffectHistoryBudgetEffectiveDesc* targetPtr =
            row.targetEffective.has_value() ? &row.targetEffective.value() : nullptr;
        const EffectHistoryBudgetEffectiveDesc* actualPtr =
            row.actualEmitter != nullptr ? &row.actualEmitter->effectiveHistoryBudget : nullptr;

        ImGui::TableNextRow();
        if (_highlightEmitterId.has_value() && _highlightEmitterId.value() == emitter->id)
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(86, 116, 60, 130));

        if (_pendingFocusEmitterId.has_value() && _pendingFocusEmitterId.value() == emitter->id)
        {
            ImGui::SetScrollHereY(0.5f);
            _pendingFocusEmitterId.reset();
        }

        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(emitter->name.c_str());
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(Get_TypeLabel(emitter->typeData.kind));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(Get_PriorityLabel(emitter->historyBudget.priority));
        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted(Get_DensityBiasLabel(emitter->historyBudget.densityBias));
        ImGui::TableSetColumnIndex(4);
        ImGui::TextUnformatted(row.fullEmitter != nullptr ? Format_Counts(row.fullEmitter->effectiveHistoryBudget).c_str() : "-");
        ImGui::TableSetColumnIndex(5);
        ImGui::TextUnformatted(targetPtr != nullptr ? Format_Counts(*targetPtr).c_str() : "-");
        ImGui::TableSetColumnIndex(6);
        ImGui::TextUnformatted(actualPtr != nullptr ? Format_Counts(*actualPtr).c_str() : "-");
        ImGui::TableSetColumnIndex(7);
        const string warningText = Build_WarningText(*emitter, targetPtr, actualPtr, historyFamilyCount);
        ImGui::TextWrapped("%s", warningText.c_str());
    }

    ImGui::EndTable();
}

Shared<HistoryBudget_View> HistoryBudget_View::Create()
{
    return make_shared<HistoryBudget_View>();
}

NS_END
