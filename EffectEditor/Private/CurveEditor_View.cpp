#include "CurveEditor_View.h"

#include "EffectEditorInstance.h"
#include "Emitter_View.h"
#include "Helper_String.h"

NS_BEGIN(EffectEditor)

namespace
{
    constexpr float kDefaultTrackListWidth{ 230.f };
    constexpr float kTrackListMinWidth{ 170.f };
    constexpr float kTrackListSplitterWidth{ 6.f };
    constexpr float kCurvePanelMinWidth{ 260.f };
    constexpr float kGraphHeight{ 260.f };
    constexpr float kGraphPadding{ 28.f };
    constexpr float kKeyHitRadius{ 8.f };

    const char* Get_InterpolationLabel(FloatCurveInterpolationMode mode)
    {
        switch (mode)
        {
        case FloatCurveInterpolationMode::Linear:
            return "Linear";
        case FloatCurveInterpolationMode::Constant:
            return "Constant";
        case FloatCurveInterpolationMode::CurveAutoClamped:
            return "Curve Auto Clamped";
        case FloatCurveInterpolationMode::CurveAuto:
            return "Curve Auto (예정)";
        case FloatCurveInterpolationMode::CurveUser:
            return "Curve User (예정)";
        case FloatCurveInterpolationMode::CurveBreak:
            return "Curve Break (예정)";
        default:
            return "Linear";
        }
    }

    ImVec2 To_GraphPoint(const ImVec2& origin, const ImVec2& size, float time, float value, float minValue, float maxValue)
    {
        const float valueRange = max(maxValue - minValue, 0.0001f);
        const float normalizedValue = clamp((value - minValue) / valueRange, 0.f, 1.f);
        return ImVec2{
            origin.x + clamp(time, 0.f, 1.f) * size.x,
            origin.y + (1.f - normalizedValue) * size.y
        };
    }

    float From_GraphTime(const ImVec2& origin, const ImVec2& size, float x)
    {
        return clamp((x - origin.x) / max(size.x, 1.f), 0.f, 1.f);
    }

    float From_GraphValue(const ImVec2& origin, const ImVec2& size, float y, float minValue, float maxValue)
    {
        const float normalizedValue = 1.f - clamp((y - origin.y) / max(size.y, 1.f), 0.f, 1.f);
        return lerp(minValue, maxValue, normalizedValue);
    }

}

CurveEditor_View::CurveEditor_View()
    : Editor_Window{ L"Curve Editor", ICON_FA_WAVE_SQUARE }
{
    _trackListWidth = kDefaultTrackListWidth;
}

void CurveEditor_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string& windowName = Get_ImGuiWindowName();

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        vector<CurveTarget> targets = Resolve_PinnedTargets();
        Remove_StalePinnedTracks(targets);
        Ensure_ActiveTrack(targets);

        if (targets.empty())
        {
            Draw_EmptyState();
        }
        else
        {
            const float contentWidth = ImGui::GetContentRegionAvail().x;
            const float maxTrackListWidth = max(kTrackListMinWidth, contentWidth - kTrackListSplitterWidth - kCurvePanelMinWidth);
            _trackListWidth = clamp(_trackListWidth, kTrackListMinWidth, maxTrackListWidth);

            Draw_TrackList(targets);
            ImGui::SameLine(0.f, 0.f);

            const ImVec2 splitterSize{
                kTrackListSplitterWidth,
                max(ImGui::GetContentRegionAvail().y, ImGui::GetFrameHeight())
            };
            ImGui::InvisibleButton("##CurveTrackListSplitter", splitterSize);
            {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const ImVec2 min = ImGui::GetItemRectMin();
                const ImVec2 max = ImGui::GetItemRectMax();
                const ImU32 splitterColor = ImGui::IsItemHovered() || ImGui::IsItemActive()
                    ? ImGui::GetColorU32(ImGuiCol_SeparatorActive)
                    : ImGui::GetColorU32(ImGuiCol_Separator);
                drawList->AddLine(
                    ImVec2{ min.x + splitterSize.x * 0.5f, min.y },
                    ImVec2{ min.x + splitterSize.x * 0.5f, max.y },
                    splitterColor
                );
            }
            if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            if (ImGui::IsItemActive())
            {
                _trackListWidth = clamp(
                    _trackListWidth + ImGui::GetIO().MouseDelta.x,
                    kTrackListMinWidth,
                    maxTrackListWidth
                );
            }

            ImGui::SameLine(0.f, 0.f);

            if (optional<CurveTarget> activeTarget = Get_ActiveTarget(targets))
            {
                ImGui::BeginGroup();
                Draw_TargetHeader(*activeTarget);
                Draw_CurveGraph(targets, *activeTarget);
                Draw_SelectedKeyInspector(*activeTarget);
                ImGui::EndGroup();
            }
        }
    }

    Set_Open(isOpen);
    ImGui::End();
}

void CurveEditor_View::Pin_ModuleTargets(uint32 emitterId, uint32 moduleId)
{
    const Shared<Emitter_View> emitterView = Resolve_EmitterView();
    if (!emitterView)
        return;

    AuthoringEmitter* emitter = emitterView->Find_Emitter(emitterId);
    AuthoringModule* module = emitterView->Find_Module(emitterId, moduleId);
    if (emitter == nullptr || module == nullptr)
        return;

    Pin_Targets(Collect_ModuleTargets(*emitter, *module));
}

void CurveEditor_View::Clear_PinnedTracks()
{
    _pinnedTracks.clear();
    _activeTrack.reset();
    _selectedKeyIndex.reset();
}

bool CurveEditor_View::Is_SameIdentity(const CurveTargetIdentity& lhs, const CurveTargetIdentity& rhs)
{
    return lhs == rhs;
}

const char* CurveEditor_View::Get_ChannelLabel(CurveTargetChannel channel)
{
    switch (channel)
    {
    case CurveTargetChannel::X:
        return "X";
    case CurveTargetChannel::Y:
        return "Y";
    case CurveTargetChannel::Value:
    default:
        return "Value";
    }
}

float CurveEditor_View::Read_Vector2Channel(const Vec2& value, CurveTargetChannel channel)
{
    return channel == CurveTargetChannel::Y ? value.y : value.x;
}

void CurveEditor_View::Write_Vector2Channel(Vec2& value, CurveTargetChannel channel, float channelValue)
{
    if (channel == CurveTargetChannel::Y)
        value.y = channelValue;
    else
        value.x = channelValue;
}

string CurveEditor_View::Build_Label(const AuthoringEmitter& emitter, const AuthoringModule& module, const string& propertyLabel, CurveTargetChannel channel)
{
    string label = emitter.name + " / " + String::ToString(module.displayName) + " / " + propertyLabel;
    if (channel != CurveTargetChannel::Value)
        label += string{ " " } + Get_ChannelLabel(channel);
    return label;
}

ImU32 CurveEditor_View::Get_TargetColor(const CurveTarget& target)
{
    if (target.channel == CurveTargetChannel::X)
        return IM_COL32(235, 85, 85, 255);

    if (target.channel == CurveTargetChannel::Y)
        return IM_COL32(85, 210, 125, 255);

    if (target.identity.propertyId.find("alpha") != string::npos || target.identity.propertyId.find("Alpha") != string::npos)
        return IM_COL32(220, 190, 255, 255);

    return target.color != 0 ? target.color : IM_COL32(245, 205, 95, 255);
}

float CurveEditor_View::Get_TargetValue(const CurveTarget& target, const FloatCurveKeyData& key)
{
    return target.payloadKind == CurveTargetPayloadKind::Float ? key.value : 0.f;
}

float CurveEditor_View::Get_TargetValue(const CurveTarget& target, const Vector2CurveKeyData& key)
{
    return target.payloadKind == CurveTargetPayloadKind::Vector2 ? Read_Vector2Channel(key.value, target.channel) : 0.f;
}

void CurveEditor_View::Set_TargetValue(const CurveTarget& target, FloatCurveKeyData& key, float value)
{
    if (target.payloadKind == CurveTargetPayloadKind::Float)
        key.value = value;
}

void CurveEditor_View::Set_TargetValue(const CurveTarget& target, Vector2CurveKeyData& key, float value)
{
    if (target.payloadKind == CurveTargetPayloadKind::Vector2)
        Write_Vector2Channel(key.value, target.channel, value);
}

float CurveEditor_View::Get_DefaultValue(const FloatDistributionData& distribution)
{
    if (const auto* constantData = get_if<ConstantFloatDistributionData>(&distribution.payload))
        return constantData->value;

    if (const auto* uniformData = get_if<UniformFloatDistributionData>(&distribution.payload))
        return uniformData->maxValue;

    if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&distribution.payload); curve != nullptr && !curve->keys.empty())
        return curve->keys.front().value;

    return 0.f;
}

Vec2 CurveEditor_View::Get_DefaultValue(const Vector2DistributionData& distribution)
{
    if (const auto* constantData = get_if<ConstantVector2DistributionData>(&distribution.payload))
        return constantData->value;

    if (const auto* uniformData = get_if<UniformVector2DistributionData>(&distribution.payload))
        return uniformData->maxValue;

    if (const auto* curve = get_if<ConstantCurveVector2DistributionData>(&distribution.payload); curve != nullptr && !curve->keys.empty())
        return curve->keys.front().value;

    return Vec2{ 1.f, 1.f };
}

ConstantCurveFloatDistributionData* CurveEditor_View::Resolve_CurvePayload(FloatDistributionData& distribution)
{
    if (distribution.mode != DistributionMode::ConstantCurve)
        return nullptr;

    return get_if<ConstantCurveFloatDistributionData>(&distribution.payload);
}

ConstantCurveVector2DistributionData* CurveEditor_View::Resolve_CurvePayload(Vector2DistributionData& distribution)
{
    if (distribution.mode != DistributionMode::ConstantCurve)
        return nullptr;

    return get_if<ConstantCurveVector2DistributionData>(&distribution.payload);
}

void CurveEditor_View::Normalize_CurveKeys(ConstantCurveFloatDistributionData& curve)
{
    if (curve.keys.empty())
        curve.keys = get<ConstantCurveFloatDistributionData>(FloatDistributionData::Make_ConstantCurve(0.f).payload).keys;

    ranges::sort(curve.keys, {}, &FloatCurveKeyData::time);
    for (FloatCurveKeyData& key : curve.keys)
        key.time = clamp(key.time, 0.f, 1.f);
}

void CurveEditor_View::Normalize_CurveKeys(ConstantCurveVector2DistributionData& curve)
{
    if (curve.keys.empty())
    {
        curve.keys = get<ConstantCurveVector2DistributionData>(
            Vector2DistributionData::Make_ConstantCurve(Vec2{ 1.f, 1.f }).payload).keys;
    }

    ranges::sort(curve.keys, {}, &Vector2CurveKeyData::time);
    for (Vector2CurveKeyData& key : curve.keys)
        key.time = clamp(key.time, 0.f, 1.f);
}

float CurveEditor_View::Evaluate_Curve(const ConstantCurveFloatDistributionData& curve, float x)
{
    if (curve.keys.empty())
        return 0.f;

    if (x <= curve.keys.front().time)
        return curve.keys.front().value;

    for (size_t index = 0; index + 1 < curve.keys.size(); ++index)
    {
        const FloatCurveKeyData& left = curve.keys[index];
        const FloatCurveKeyData& right = curve.keys[index + 1];
        if (x > right.time)
            continue;

        if (left.interpolationMode == FloatCurveInterpolationMode::Constant)
            return left.value;

        const float span = max(right.time - left.time, 0.0001f);
        const float ratio = clamp((x - left.time) / span, 0.f, 1.f);
        return lerp(left.value, right.value, ratio);
    }

    return curve.keys.back().value;
}

Vec2 CurveEditor_View::Evaluate_Curve(const ConstantCurveVector2DistributionData& curve, float x)
{
    if (curve.keys.empty())
        return Vec2{ 1.f, 1.f };

    if (x <= curve.keys.front().time)
        return curve.keys.front().value;

    for (size_t index = 0; index + 1 < curve.keys.size(); ++index)
    {
        const Vector2CurveKeyData& left = curve.keys[index];
        const Vector2CurveKeyData& right = curve.keys[index + 1];
        if (x > right.time)
            continue;

        if (left.interpolationMode == FloatCurveInterpolationMode::Constant)
            return left.value;

        const float span = max(right.time - left.time, 0.0001f);
        const float ratio = clamp((x - left.time) / span, 0.f, 1.f);
        return Vec2{
            lerp(left.value.x, right.value.x, ratio),
            lerp(left.value.y, right.value.y, ratio)
        };
    }

    return curve.keys.back().value;
}

Shared<Emitter_View> CurveEditor_View::Resolve_EmitterView() const
{
    if (EDITOR == nullptr)
        return nullptr;

    const Shared<Editor_Window> emitterWindow = EDITOR->Get_Window(L"Emitter");
    return dynamic_pointer_cast<Emitter_View>(emitterWindow);
}

vector<CurveEditor_View::CurveTarget> CurveEditor_View::Collect_ModuleTargets(AuthoringEmitter& emitter, AuthoringModule& module) const
{
    vector<CurveTarget> targets{};

    switch (module.type)
    {
    case AuthoringModuleType::Spawn:
        if (auto* data = get_if<SpawnModuleData>(&module.data))
        {
            Add_FloatTarget(targets, emitter, module, data->spawnRate, "spawnRate", "Spawn Rate", IM_COL32(245, 205, 95, 255));
            Add_FloatTarget(targets, emitter, module, data->spawnRateScale, "spawnRateScale", "Spawn Rate Scale", IM_COL32(245, 205, 95, 255));
            Add_FloatTarget(targets, emitter, module, data->burstScale, "burstScale", "Burst Scale", IM_COL32(245, 205, 95, 255));
        }
        break;
    case AuthoringModuleType::Lifetime:
        if (auto* data = get_if<LifetimeModuleData>(&module.data))
            Add_FloatTarget(targets, emitter, module, data->lifeTime, "lifeTime", "Life Time", IM_COL32(245, 205, 95, 255));
        break;
    case AuthoringModuleType::InitialSize:
        if (auto* data = get_if<InitialSizeModuleData>(&module.data))
            Add_Vector2Target(targets, emitter, module, data->size, "size", "Initial Size");
        break;
    case AuthoringModuleType::InitialRadialVelocity:
        if (auto* data = get_if<InitialRadialVelocityModuleData>(&module.data))
            Add_FloatTarget(targets, emitter, module, data->speed, "speed", "Speed", IM_COL32(245, 205, 95, 255));
        break;
    case AuthoringModuleType::Drag:
        if (auto* data = get_if<DragModuleData>(&module.data))
            Add_FloatTarget(targets, emitter, module, data->drag, "drag", "Drag", IM_COL32(245, 205, 95, 255));
        break;
    case AuthoringModuleType::InitialRotation:
        if (auto* data = get_if<InitialRotationModuleData>(&module.data))
            Add_FloatTarget(targets, emitter, module, data->rotationDegrees, "rotationDegrees", "Rotation", IM_COL32(245, 205, 95, 255));
        break;
    case AuthoringModuleType::RotationOverLife:
        if (auto* data = get_if<RotationOverLifeModuleData>(&module.data))
            Add_FloatTarget(targets, emitter, module, data->rotationOverLife, "rotationOverLife", "Rotation Over Life", IM_COL32(245, 205, 95, 255));
        break;
    case AuthoringModuleType::InitialRotationRate:
        if (auto* data = get_if<InitialRotationRateModuleData>(&module.data))
            Add_FloatTarget(targets, emitter, module, data->rotationRateDegrees, "rotationRateDegrees", "Rotation Rate", IM_COL32(245, 205, 95, 255));
        break;
    case AuthoringModuleType::InitialColor:
        if (auto* data = get_if<InitialColorModuleData>(&module.data))
            Add_FloatTarget(targets, emitter, module, data->alpha, "alpha", "Alpha", IM_COL32(220, 190, 255, 255));
        break;
    case AuthoringModuleType::ColorOverLife:
        if (auto* data = get_if<ColorOverLifeModuleData>(&module.data))
            Add_FloatTarget(targets, emitter, module, data->alphaOverLife, "alphaOverLife", "Alpha Over Life", IM_COL32(220, 190, 255, 255));
        break;
    case AuthoringModuleType::SizeByLife:
        if (auto* data = get_if<SizeByLifeModuleData>(&module.data))
            Add_Vector2Target(targets, emitter, module, data->scaleOverLife, "scaleOverLife", "Scale Over Life");
        break;
    case AuthoringModuleType::SpawnPerUnit:
        if (auto* data = get_if<SpawnPerUnitModuleData>(&module.data))
            Add_FloatTarget(targets, emitter, module, data->spawnPerUnit, "spawnPerUnit", "Spawn Per Unit", IM_COL32(245, 205, 95, 255));
        break;
    default:
        break;
    }

    return targets;
}

vector<CurveEditor_View::CurveTarget> CurveEditor_View::Resolve_PinnedTargets()
{
    vector<CurveTarget> targets{};
    targets.reserve(_pinnedTracks.size());

    for (const PinnedTrack& track : _pinnedTracks)
    {
        if (optional<CurveTarget> target = Resolve_Target(track.identity))
            targets.push_back(*target);
    }

    return targets;
}

optional<CurveEditor_View::CurveTarget> CurveEditor_View::Resolve_Target(const CurveTargetIdentity& identity) const
{
    const Shared<Emitter_View> emitterView = Resolve_EmitterView();
    if (!emitterView)
        return nullopt;

    AuthoringEmitter* emitter = emitterView->Find_Emitter(identity.emitterId);
    AuthoringModule* module = emitterView->Find_Module(identity.emitterId, identity.moduleId);
    if (emitter == nullptr || module == nullptr || module->type != identity.moduleType)
        return nullopt;

    vector<CurveTarget> targets = Collect_ModuleTargets(*emitter, *module);
    for (CurveTarget& target : targets)
    {
        if (Is_SameIdentity(target.identity, identity))
            return target;
    }

    return nullopt;
}

optional<size_t> CurveEditor_View::Find_PinnedTrackIndex(const CurveTargetIdentity& identity) const
{
    for (size_t index = 0; index < _pinnedTracks.size(); ++index)
    {
        if (Is_SameIdentity(_pinnedTracks[index].identity, identity))
            return index;
    }

    return nullopt;
}

optional<size_t> CurveEditor_View::Find_ResolvedTargetIndex(const vector<CurveTarget>& targets, const CurveTargetIdentity& identity) const
{
    for (size_t index = 0; index < targets.size(); ++index)
    {
        if (Is_SameIdentity(targets[index].identity, identity))
            return index;
    }

    return nullopt;
}

optional<CurveEditor_View::CurveTarget> CurveEditor_View::Get_ActiveTarget(const vector<CurveTarget>& targets) const
{
    if (_activeTrack.has_value())
    {
        if (optional<size_t> index = Find_ResolvedTargetIndex(targets, *_activeTrack))
            return targets[*index];
    }

    if (!targets.empty())
        return targets.front();

    return nullopt;
}

void CurveEditor_View::Add_FloatTarget(vector<CurveTarget>& targets, AuthoringEmitter& emitter, AuthoringModule& module, FloatDistributionData& distribution, const string& propertyId, const string& label, ImU32 color) const
{
    CurveTargetIdentity identity{};
    identity.emitterId = emitter.id;
    identity.moduleId = module.id;
    identity.moduleType = module.type;
    identity.propertyId = propertyId;
    identity.channel = CurveTargetChannel::Value;

    targets.push_back(
        CurveTarget{
            identity,
            &emitter,
            &module,
            &distribution,
            Build_Label(emitter, module, label, CurveTargetChannel::Value),
            label,
            CurveTargetPayloadKind::Float,
            CurveTargetChannel::Value,
            color
        }
    );
}

void CurveEditor_View::Add_Vector2Target(vector<CurveTarget>& targets, AuthoringEmitter& emitter, AuthoringModule& module, Vector2DistributionData& distribution, const string& propertyId, const string& label) const
{
    for (CurveTargetChannel channel : { CurveTargetChannel::X, CurveTargetChannel::Y })
    {
        CurveTargetIdentity identity{};
        identity.emitterId = emitter.id;
        identity.moduleId = module.id;
        identity.moduleType = module.type;
        identity.propertyId = propertyId;
        identity.channel = channel;

        targets.push_back(
            CurveTarget{
                identity,
                &emitter,
                &module,
                &distribution,
                Build_Label(emitter, module, label, channel),
                label,
                CurveTargetPayloadKind::Vector2,
                channel,
                0
            }
        );
    }
}

void CurveEditor_View::Pin_Targets(const vector<CurveTarget>& targets)
{
    optional<CurveTargetIdentity> firstPinned{};

    for (const CurveTarget& target : targets)
    {
        if (!Find_PinnedTrackIndex(target.identity).has_value())
            _pinnedTracks.push_back(PinnedTrack{ target.identity, true });

        if (!firstPinned.has_value())
            firstPinned = target.identity;
    }

    if (firstPinned.has_value())
    {
        _activeTrack = firstPinned;
        _selectedKeyIndex.reset();
        Set_Open(true);
    }
}

void CurveEditor_View::Remove_StalePinnedTracks(const vector<CurveTarget>& targets)
{
    _pinnedTracks.erase(
        remove_if(
            _pinnedTracks.begin(),
            _pinnedTracks.end(),
            [this, &targets](const PinnedTrack& track)
            {
                return !Find_ResolvedTargetIndex(targets, track.identity).has_value();
            }
        ),
        _pinnedTracks.end()
    );

    if (_activeTrack.has_value() && !Find_ResolvedTargetIndex(targets, *_activeTrack).has_value())
    {
        _activeTrack.reset();
        _selectedKeyIndex.reset();
    }
}

void CurveEditor_View::Ensure_ActiveTrack(const vector<CurveTarget>& targets)
{
    if (targets.empty())
    {
        _activeTrack.reset();
        _selectedKeyIndex.reset();
        return;
    }

    if (!_activeTrack.has_value() || !Find_ResolvedTargetIndex(targets, *_activeTrack).has_value())
    {
        _activeTrack = targets.front().identity;
        _selectedKeyIndex.reset();
    }
}

bool CurveEditor_View::Convert_ActiveTargetToConstantCurve(CurveTarget& target)
{
    if (auto* floatDistribution = get_if<FloatDistributionData*>(&target.distribution))
    {
        **floatDistribution = FloatDistributionData::Make_ConstantCurve(Get_DefaultValue(**floatDistribution));
        target.emitter->previewDirty = true;
        _selectedKeyIndex = 0;
        return true;
    }

    if (auto* vectorDistribution = get_if<Vector2DistributionData*>(&target.distribution))
    {
        **vectorDistribution = Vector2DistributionData::Make_ConstantCurve(Get_DefaultValue(**vectorDistribution));
        target.emitter->previewDirty = true;
        _selectedKeyIndex = 0;
        return true;
    }

    return false;
}

void CurveEditor_View::Draw_EmptyState()
{
    ImGui::TextDisabled("Use a module curve button in the Emitter view to pin editable Float/Vector2 curves.");
}

void CurveEditor_View::Draw_TrackList(const vector<CurveTarget>& targets)
{
    ImGui::BeginChild("##CurveTrackList", ImVec2(_trackListWidth, 0.f), true);
    ImGui::TextDisabled("Pinned Tracks");
    ImGui::Separator();

    for (const CurveTarget& target : targets)
    {
        optional<size_t> pinnedIndex = Find_PinnedTrackIndex(target.identity);
        if (!pinnedIndex.has_value())
            continue;

        PinnedTrack& track = _pinnedTracks[*pinnedIndex];
        const bool selected = _activeTrack.has_value() && Is_SameIdentity(*_activeTrack, target.identity);

        ImGui::PushID(static_cast<int32>(*pinnedIndex));
        ImGui::ColorButton("##Color", ImGui::ColorConvertU32ToFloat4(Get_TargetColor(target)), ImGuiColorEditFlags_NoTooltip, ImVec2{ 12.f, 12.f });
        ImGui::SameLine();
        ImGui::Checkbox("##Visible", &track.visible);
        ImGui::SameLine();

        const float removeButtonWidth = ImGui::GetFrameHeight();
        const float selectableWidth = max(40.f, ImGui::GetContentRegionAvail().x - removeButtonWidth - 4.f);
        if (ImGui::Selectable(target.label.c_str(), selected, 0, ImVec2{ selectableWidth, 0.f }))
        {
            _activeTrack = target.identity;
            _selectedKeyIndex.reset();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_FA_XMARK))
        {
            _pinnedTracks.erase(_pinnedTracks.begin() + static_cast<ptrdiff_t>(*pinnedIndex));
            if (_activeTrack.has_value() && Is_SameIdentity(*_activeTrack, target.identity))
            {
                _activeTrack.reset();
                _selectedKeyIndex.reset();
            }
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }

    ImGui::EndChild();
}

void CurveEditor_View::Draw_TargetHeader(CurveTarget& target)
{
    ImGui::TextUnformatted(target.label.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("active");

    bool hasCurve = false;
    size_t keyCount = 0;
    if (auto* floatDistribution = get_if<FloatDistributionData*>(&target.distribution))
    {
        if (ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution))
        {
            Normalize_CurveKeys(*curve);
            keyCount = curve->keys.size();
            hasCurve = true;
        }
    }
    else if (auto* vectorDistribution = get_if<Vector2DistributionData*>(&target.distribution))
    {
        if (ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vectorDistribution))
        {
            Normalize_CurveKeys(*curve);
            keyCount = curve->keys.size();
            hasCurve = true;
        }
    }

    ImGui::SameLine();
    ImGui::TextDisabled("%zu keys", keyCount);

    if (!hasCurve)
    {
        if (ImGui::Button("Convert to Constant Curve"))
            Convert_ActiveTargetToConstantCurve(target);
        return;
    }

    if (ImGui::Button("Add Key"))
    {
        if (auto* floatDistribution = get_if<FloatDistributionData*>(&target.distribution))
        {
            ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution);
            if (curve != nullptr)
            {
                const float time = _selectedKeyIndex.has_value() && *_selectedKeyIndex < curve->keys.size()
                    ? clamp(curve->keys[*_selectedKeyIndex].time + 0.1f, 0.f, 1.f)
                    : 0.5f;
                curve->keys.push_back(FloatCurveKeyData{ time, Evaluate_Curve(*curve, time), 0.f, 0.f, FloatCurveInterpolationMode::Linear });
                Normalize_CurveKeys(*curve);
                _selectedKeyIndex = curve->keys.empty() ? optional<size_t>{} : optional<size_t>{ curve->keys.size() - 1 };
                target.emitter->previewDirty = true;
            }
        }
        else if (auto* vectorDistribution = get_if<Vector2DistributionData*>(&target.distribution))
        {
            ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vectorDistribution);
            if (curve != nullptr)
            {
                const float time = _selectedKeyIndex.has_value() && *_selectedKeyIndex < curve->keys.size()
                    ? clamp(curve->keys[*_selectedKeyIndex].time + 0.1f, 0.f, 1.f)
                    : 0.5f;
                curve->keys.push_back(Vector2CurveKeyData{ time, Evaluate_Curve(*curve, time), Vec2{}, Vec2{}, FloatCurveInterpolationMode::Linear });
                Normalize_CurveKeys(*curve);
                _selectedKeyIndex = curve->keys.empty() ? optional<size_t>{} : optional<size_t>{ curve->keys.size() - 1 };
                target.emitter->previewDirty = true;
            }
        }
    }

    ImGui::SameLine();
    bool canDelete = false;
    if (auto* floatDistribution = get_if<FloatDistributionData*>(&target.distribution))
    {
        if (ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution))
            canDelete = _selectedKeyIndex.has_value() && *_selectedKeyIndex < curve->keys.size() && curve->keys.size() > 2;
    }
    else if (auto* vectorDistribution = get_if<Vector2DistributionData*>(&target.distribution))
    {
        if (ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vectorDistribution))
            canDelete = _selectedKeyIndex.has_value() && *_selectedKeyIndex < curve->keys.size() && curve->keys.size() > 2;
    }

    if (!canDelete)
        ImGui::BeginDisabled();
    if (ImGui::Button("Delete Key") && canDelete)
    {
        if (auto* floatDistribution = get_if<FloatDistributionData*>(&target.distribution))
        {
            if (ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution))
                curve->keys.erase(curve->keys.begin() + static_cast<ptrdiff_t>(*_selectedKeyIndex));
        }
        else if (auto* vectorDistribution = get_if<Vector2DistributionData*>(&target.distribution))
        {
            if (ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vectorDistribution))
                curve->keys.erase(curve->keys.begin() + static_cast<ptrdiff_t>(*_selectedKeyIndex));
        }
        _selectedKeyIndex.reset();
        target.emitter->previewDirty = true;
    }
    if (!canDelete)
        ImGui::EndDisabled();
}

void CurveEditor_View::Draw_CurveGraph(const vector<CurveTarget>& targets, CurveTarget& activeTarget)
{
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.y = kGraphHeight;
    canvasSize.x = max(canvasSize.x, 260.f);

    ImGui::InvisibleButton("##CurveGraph", canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool isHovered = ImGui::IsItemHovered();
    const bool isActive = ImGui::IsItemActive();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(canvasPos, ImVec2{ canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y }, IM_COL32(24, 26, 32, 255));
    drawList->AddRect(canvasPos, ImVec2{ canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y }, IM_COL32(75, 78, 88, 255));

    const ImVec2 graphOrigin{ canvasPos.x + kGraphPadding, canvasPos.y + 12.f };
    const ImVec2 graphSize{ canvasSize.x - kGraphPadding * 2.f, canvasSize.y - 36.f };

    float minValue = 0.f;
    float maxValue = 1.f;
    bool hasVisibleCurve = false;

    for (const CurveTarget& target : targets)
    {
        optional<size_t> pinnedIndex = Find_PinnedTrackIndex(target.identity);
        if (!pinnedIndex.has_value() || !_pinnedTracks[*pinnedIndex].visible)
            continue;

        if (auto* floatDistribution = get_if<FloatDistributionData*>(&target.distribution))
        {
            ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution);
            if (curve == nullptr)
                continue;

            Normalize_CurveKeys(*curve);
            for (const FloatCurveKeyData& key : curve->keys)
            {
                const float value = Get_TargetValue(target, key);
                minValue = min(minValue, value);
                maxValue = max(maxValue, value);
                hasVisibleCurve = true;
            }
        }
        else if (auto* vectorDistribution = get_if<Vector2DistributionData*>(&target.distribution))
        {
            ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vectorDistribution);
            if (curve == nullptr)
                continue;

            Normalize_CurveKeys(*curve);
            for (const Vector2CurveKeyData& key : curve->keys)
            {
                const float value = Get_TargetValue(target, key);
                minValue = min(minValue, value);
                maxValue = max(maxValue, value);
                hasVisibleCurve = true;
            }
        }
    }

    if (!hasVisibleCurve)
    {
        minValue = 0.f;
        maxValue = 1.f;
    }

    const float margin = max((maxValue - minValue) * 0.1f, 0.1f);
    minValue -= margin;
    maxValue += margin;

    for (int32 line = 0; line <= 4; ++line)
    {
        const float ratio = static_cast<float>(line) / 4.f;
        const float x = graphOrigin.x + graphSize.x * ratio;
        const float y = graphOrigin.y + graphSize.y * ratio;
        drawList->AddLine(ImVec2{ x, graphOrigin.y }, ImVec2{ x, graphOrigin.y + graphSize.y }, IM_COL32(48, 52, 60, 255));
        drawList->AddLine(ImVec2{ graphOrigin.x, y }, ImVec2{ graphOrigin.x + graphSize.x, y }, IM_COL32(48, 52, 60, 255));
    }

    for (const CurveTarget& target : targets)
    {
        optional<size_t> pinnedIndex = Find_PinnedTrackIndex(target.identity);
        if (!pinnedIndex.has_value() || !_pinnedTracks[*pinnedIndex].visible)
            continue;

        const bool isActiveTarget = Is_SameIdentity(target.identity, activeTarget.identity);
        const ImU32 lineColor = Get_TargetColor(target);
        const float lineThickness = isActiveTarget ? 2.25f : 1.25f;

        if (auto* floatDistribution = get_if<FloatDistributionData*>(&target.distribution))
        {
            ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution);
            if (curve == nullptr)
                continue;

            ImVec2 previousPoint = To_GraphPoint(graphOrigin, graphSize, 0.f, Evaluate_Curve(*curve, 0.f), minValue, maxValue);
            for (int32 sample = 1; sample <= 64; ++sample)
            {
                const float time = static_cast<float>(sample) / 64.f;
                const ImVec2 point = To_GraphPoint(graphOrigin, graphSize, time, Evaluate_Curve(*curve, time), minValue, maxValue);
                drawList->AddLine(previousPoint, point, lineColor, lineThickness);
                previousPoint = point;
            }

            for (size_t index = 0; index < curve->keys.size(); ++index)
            {
                const FloatCurveKeyData& key = curve->keys[index];
                const ImVec2 point = To_GraphPoint(graphOrigin, graphSize, key.time, key.value, minValue, maxValue);
                const bool selected = isActiveTarget && _selectedKeyIndex.has_value() && *_selectedKeyIndex == index;
                drawList->AddCircleFilled(point, selected ? 5.f : 3.5f, selected ? IM_COL32(100, 190, 255, 255) : lineColor);
            }
        }
        else if (auto* vectorDistribution = get_if<Vector2DistributionData*>(&target.distribution))
        {
            ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vectorDistribution);
            if (curve == nullptr)
                continue;

            ImVec2 previousPoint = To_GraphPoint(graphOrigin, graphSize, 0.f, Read_Vector2Channel(Evaluate_Curve(*curve, 0.f), target.channel), minValue, maxValue);
            for (int32 sample = 1; sample <= 64; ++sample)
            {
                const float time = static_cast<float>(sample) / 64.f;
                const ImVec2 point = To_GraphPoint(graphOrigin, graphSize, time, Read_Vector2Channel(Evaluate_Curve(*curve, time), target.channel), minValue, maxValue);
                drawList->AddLine(previousPoint, point, lineColor, lineThickness);
                previousPoint = point;
            }

            for (size_t index = 0; index < curve->keys.size(); ++index)
            {
                const Vector2CurveKeyData& key = curve->keys[index];
                const ImVec2 point = To_GraphPoint(graphOrigin, graphSize, key.time, Get_TargetValue(target, key), minValue, maxValue);
                const bool selected = isActiveTarget && _selectedKeyIndex.has_value() && *_selectedKeyIndex == index;
                drawList->AddCircleFilled(point, selected ? 5.f : 3.5f, selected ? IM_COL32(100, 190, 255, 255) : lineColor);
            }
        }
    }

    const ImVec2 mousePos = ImGui::GetIO().MousePos;
    if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        optional<size_t> hitIndex{};
        if (auto* floatDistribution = get_if<FloatDistributionData*>(&activeTarget.distribution))
        {
            if (ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution))
            {
                for (size_t index = 0; index < curve->keys.size(); ++index)
                {
                    const FloatCurveKeyData& key = curve->keys[index];
                    const ImVec2 point = To_GraphPoint(graphOrigin, graphSize, key.time, key.value, minValue, maxValue);
                    const float dx = point.x - mousePos.x;
                    const float dy = point.y - mousePos.y;
                    if (sqrtf(dx * dx + dy * dy) <= kKeyHitRadius)
                    {
                        hitIndex = index;
                        break;
                    }
                }
            }
        }
        else if (auto* vectorDistribution = get_if<Vector2DistributionData*>(&activeTarget.distribution))
        {
            if (ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vectorDistribution))
            {
                for (size_t index = 0; index < curve->keys.size(); ++index)
                {
                    const Vector2CurveKeyData& key = curve->keys[index];
                    const ImVec2 point = To_GraphPoint(graphOrigin, graphSize, key.time, Get_TargetValue(activeTarget, key), minValue, maxValue);
                    const float dx = point.x - mousePos.x;
                    const float dy = point.y - mousePos.y;
                    if (sqrtf(dx * dx + dy * dy) <= kKeyHitRadius)
                    {
                        hitIndex = index;
                        break;
                    }
                }
            }
        }
        _selectedKeyIndex = hitIndex;
    }

    if (isHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        const float time = From_GraphTime(graphOrigin, graphSize, mousePos.x);
        const float value = From_GraphValue(graphOrigin, graphSize, mousePos.y, minValue, maxValue);

        if (auto* floatDistribution = get_if<FloatDistributionData*>(&activeTarget.distribution))
        {
            if (ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution))
            {
                curve->keys.push_back(FloatCurveKeyData{ time, value, 0.f, 0.f, FloatCurveInterpolationMode::Linear });
                Normalize_CurveKeys(*curve);
                activeTarget.emitter->previewDirty = true;
            }
        }
        else if (auto* vectorDistribution = get_if<Vector2DistributionData*>(&activeTarget.distribution))
        {
            if (ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vectorDistribution))
            {
                Vector2CurveKeyData key{ time, Evaluate_Curve(*curve, time), Vec2{}, Vec2{}, FloatCurveInterpolationMode::Linear };
                Set_TargetValue(activeTarget, key, value);
                curve->keys.push_back(key);
                Normalize_CurveKeys(*curve);
                activeTarget.emitter->previewDirty = true;
            }
        }
    }

    if (isActive && _selectedKeyIndex.has_value() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        const size_t index = *_selectedKeyIndex;
        if (auto* floatDistribution = get_if<FloatDistributionData*>(&activeTarget.distribution))
        {
            if (ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution); curve != nullptr && index < curve->keys.size())
            {
                FloatCurveKeyData& key = curve->keys[index];
                key.time = From_GraphTime(graphOrigin, graphSize, mousePos.x);
                Set_TargetValue(activeTarget, key, From_GraphValue(graphOrigin, graphSize, mousePos.y, minValue, maxValue));
                activeTarget.emitter->previewDirty = true;
            }
        }
        else if (auto* vectorDistribution = get_if<Vector2DistributionData*>(&activeTarget.distribution))
        {
            if (ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vectorDistribution); curve != nullptr && index < curve->keys.size())
            {
                Vector2CurveKeyData& key = curve->keys[index];
                key.time = From_GraphTime(graphOrigin, graphSize, mousePos.x);
                Set_TargetValue(activeTarget, key, From_GraphValue(graphOrigin, graphSize, mousePos.y, minValue, maxValue));
                activeTarget.emitter->previewDirty = true;
            }
        }
    }
}

void CurveEditor_View::Draw_SelectedKeyInspector(CurveTarget& target)
{
    if (!_selectedKeyIndex.has_value())
    {
        ImGui::TextDisabled("No key selected.");
        return;
    }

    bool changed = false;

    if (auto* floatDistribution = get_if<FloatDistributionData*>(&target.distribution))
    {
        ConstantCurveFloatDistributionData* curve = Resolve_CurvePayload(**floatDistribution);
        if (curve == nullptr || *_selectedKeyIndex >= curve->keys.size())
        {
            ImGui::TextDisabled("No key selected.");
            return;
        }

        FloatCurveKeyData& key = curve->keys[*_selectedKeyIndex];
        changed |= ImGui::DragFloat("Time", &key.time, 0.005f, 0.f, 1.f, "%.3f");
        changed |= ImGui::DragFloat("Value", &key.value, 0.01f, -1000.f, 1000.f, "%.3f");

        if (ImGui::BeginCombo("Interpolation", Get_InterpolationLabel(key.interpolationMode)))
        {
            const auto draw_item = [&](FloatCurveInterpolationMode mode, bool enabled)
            {
                const bool selected = key.interpolationMode == mode;
                if (!enabled)
                    ImGui::BeginDisabled();
                if (ImGui::Selectable(Get_InterpolationLabel(mode), selected) && enabled)
                {
                    key.interpolationMode = mode;
                    changed = true;
                }
                if (!enabled)
                    ImGui::EndDisabled();
            };

            draw_item(FloatCurveInterpolationMode::Linear, true);
            draw_item(FloatCurveInterpolationMode::Constant, true);
            draw_item(FloatCurveInterpolationMode::CurveAutoClamped, true);
            draw_item(FloatCurveInterpolationMode::CurveAuto, false);
            draw_item(FloatCurveInterpolationMode::CurveUser, false);
            draw_item(FloatCurveInterpolationMode::CurveBreak, false);
            ImGui::EndCombo();
        }

        if (changed)
        {
            key.time = clamp(key.time, 0.f, 1.f);
            Normalize_CurveKeys(*curve);
            target.emitter->previewDirty = true;
        }

        return;
    }

    if (auto* vectorDistribution = get_if<Vector2DistributionData*>(&target.distribution))
    {
        ConstantCurveVector2DistributionData* curve = Resolve_CurvePayload(**vectorDistribution);
        if (curve == nullptr || *_selectedKeyIndex >= curve->keys.size())
        {
            ImGui::TextDisabled("No key selected.");
            return;
        }

        Vector2CurveKeyData& key = curve->keys[*_selectedKeyIndex];
        float activeValue = Get_TargetValue(target, key);
        changed |= ImGui::DragFloat("Time", &key.time, 0.005f, 0.f, 1.f, "%.3f");
        if (ImGui::DragFloat(Get_ChannelLabel(target.channel), &activeValue, 0.01f, -1000.f, 1000.f, "%.3f"))
        {
            Set_TargetValue(target, key, activeValue);
            changed = true;
        }

        const CurveTargetChannel pairedChannel = target.channel == CurveTargetChannel::Y ? CurveTargetChannel::X : CurveTargetChannel::Y;
        float pairedValue = Read_Vector2Channel(key.value, pairedChannel);
        ImGui::BeginDisabled();
        ImGui::DragFloat(Get_ChannelLabel(pairedChannel), &pairedValue, 0.01f, -1000.f, 1000.f, "%.3f");
        ImGui::EndDisabled();

        if (ImGui::BeginCombo("Interpolation", Get_InterpolationLabel(key.interpolationMode)))
        {
            const auto draw_item = [&](FloatCurveInterpolationMode mode, bool enabled)
            {
                const bool selected = key.interpolationMode == mode;
                if (!enabled)
                    ImGui::BeginDisabled();
                if (ImGui::Selectable(Get_InterpolationLabel(mode), selected) && enabled)
                {
                    key.interpolationMode = mode;
                    changed = true;
                }
                if (!enabled)
                    ImGui::EndDisabled();
            };

            draw_item(FloatCurveInterpolationMode::Linear, true);
            draw_item(FloatCurveInterpolationMode::Constant, true);
            draw_item(FloatCurveInterpolationMode::CurveAutoClamped, true);
            draw_item(FloatCurveInterpolationMode::CurveAuto, false);
            draw_item(FloatCurveInterpolationMode::CurveUser, false);
            draw_item(FloatCurveInterpolationMode::CurveBreak, false);
            ImGui::EndCombo();
        }

        if (changed)
        {
            key.time = clamp(key.time, 0.f, 1.f);
            Normalize_CurveKeys(*curve);
            target.emitter->previewDirty = true;
        }
    }
}

Shared<CurveEditor_View> CurveEditor_View::Create()
{
    return make_shared<CurveEditor_View>();
}

NS_END
