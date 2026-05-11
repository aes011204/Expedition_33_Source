#include "Helper_DetailProperty.h"

#include <cfloat>
#include "Helper_Distribution.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    bool Draw_FloatDistributionProperty(
        const char* label,
        FloatDistributionData& value,
        const FloatDistributionData& defaultValue,
        float speed = 0.01f);
    bool Draw_Vector2DistributionProperty(
        const char* label,
        Vector2DistributionData& value,
        const Vector2DistributionData& defaultValue,
        float speed = 0.01f);
    bool Draw_Vector3DistributionProperty(
        const char* label,
        Vector3DistributionData& value,
        const Vector3DistributionData& defaultValue,
        float speed = 0.01f);
    bool Draw_ColorDistributionProperty(
        const char* label,
        ColorDistributionData& value,
        const ColorDistributionData& defaultValue);
    bool Draw_BurstEntryProperty(
        SpawnModuleData::ParticleBurstData& burst,
        const SpawnModuleData::ParticleBurstData& defaultValue);
    bool Draw_Vec2Property(const char* label, Vec2& value, const Vec2& defaultValue, float speed = 0.01f);
    bool Draw_ColorProperty(const char* label, Color& value, const Color& defaultValue);

    void Transition_FloatDistributionMode(FloatDistributionData& value, DistributionMode nextMode);
    void Transition_Vector2DistributionMode(Vector2DistributionData& value, DistributionMode nextMode);
    void Transition_Vector3DistributionMode(Vector3DistributionData& value, DistributionMode nextMode);
    void Transition_ColorDistributionMode(ColorDistributionData& value, DistributionMode nextMode);

    bool Is_DefaultValue(float value, float defaultValue)
    {
        return value == defaultValue;
    }

    bool Is_DefaultValue(const Vec2& value, const Vec2& defaultValue)
    {
        return value.x == defaultValue.x && value.y == defaultValue.y;
    }

    bool Is_DefaultValue(const Vec3& value, const Vec3& defaultValue)
    {
        return value.x == defaultValue.x && value.y == defaultValue.y && value.z == defaultValue.z;
    }

    bool Is_DefaultValue(const Color& value, const Color& defaultValue)
    {
        return value.x == defaultValue.x &&
               value.y == defaultValue.y &&
               value.z == defaultValue.z &&
               value.w == defaultValue.w;
    }

    bool Is_DefaultValue(const ConstantFloatDistributionData& value, const ConstantFloatDistributionData& defaultValue)
    {
        return Is_DefaultValue(value.value, defaultValue.value);
    }

    bool Is_DefaultValue(const UniformFloatDistributionData& value, const UniformFloatDistributionData& defaultValue)
    {
        return Is_DefaultValue(value.minValue, defaultValue.minValue) &&
               Is_DefaultValue(value.maxValue, defaultValue.maxValue);
    }

    bool Is_DefaultValue(const ConstantVector2DistributionData& value, const ConstantVector2DistributionData& defaultValue)
    {
        return Is_DefaultValue(value.value, defaultValue.value);
    }

    bool Is_DefaultValue(const UniformVector2DistributionData& value, const UniformVector2DistributionData& defaultValue)
    {
        return Is_DefaultValue(value.minValue, defaultValue.minValue) &&
               Is_DefaultValue(value.maxValue, defaultValue.maxValue);
    }

    bool Is_DefaultValue(const ConstantVector3DistributionData& value, const ConstantVector3DistributionData& defaultValue)
    {
        return Is_DefaultValue(value.value, defaultValue.value);
    }

    bool Is_DefaultValue(const UniformVector3DistributionData& value, const UniformVector3DistributionData& defaultValue)
    {
        return Is_DefaultValue(value.minValue, defaultValue.minValue) &&
               Is_DefaultValue(value.maxValue, defaultValue.maxValue);
    }

    bool Is_DefaultValue(const ConstantColorDistributionData& value, const ConstantColorDistributionData& defaultValue)
    {
        return Is_DefaultValue(value.value, defaultValue.value);
    }

    bool Is_DefaultValue(const UniformColorDistributionData& value, const UniformColorDistributionData& defaultValue)
    {
        return Is_DefaultValue(value.minValue, defaultValue.minValue) &&
               Is_DefaultValue(value.maxValue, defaultValue.maxValue);
    }

    bool Is_DefaultValue(const FloatDistributionData& value, const FloatDistributionData& defaultValue)
    {
        if (value.mode != defaultValue.mode || value.payload.index() != defaultValue.payload.index())
            return false;

        if (const auto* current = get_if<ConstantFloatDistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<ConstantFloatDistributionData>(defaultValue.payload));

        if (const auto* current = get_if<UniformFloatDistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<UniformFloatDistributionData>(defaultValue.payload));

        return false;
    }

    bool Is_DefaultValue(const Vector2DistributionData& value, const Vector2DistributionData& defaultValue)
    {
        if (value.mode != defaultValue.mode || value.payload.index() != defaultValue.payload.index())
            return false;

        if (const auto* current = get_if<ConstantVector2DistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<ConstantVector2DistributionData>(defaultValue.payload));

        if (const auto* current = get_if<UniformVector2DistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<UniformVector2DistributionData>(defaultValue.payload));

        return false;
    }

    bool Is_DefaultValue(const Vector3DistributionData& value, const Vector3DistributionData& defaultValue)
    {
        if (value.mode != defaultValue.mode || value.payload.index() != defaultValue.payload.index())
            return false;

        if (const auto* current = get_if<ConstantVector3DistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<ConstantVector3DistributionData>(defaultValue.payload));

        if (const auto* current = get_if<UniformVector3DistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<UniformVector3DistributionData>(defaultValue.payload));

        return false;
    }

    bool Is_DefaultValue(const ColorDistributionData& value, const ColorDistributionData& defaultValue)
    {
        if (value.mode != defaultValue.mode || value.payload.index() != defaultValue.payload.index())
            return false;

        if (const auto* current = get_if<ConstantColorDistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<ConstantColorDistributionData>(defaultValue.payload));

        if (const auto* current = get_if<UniformColorDistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<UniformColorDistributionData>(defaultValue.payload));

        return false;
    }

    bool Is_DefaultValue(const SpawnModuleData::ParticleBurstData& value, const SpawnModuleData::ParticleBurstData& defaultValue)
    {
        return Is_DefaultValue(value.time, defaultValue.time) && value.count == defaultValue.count;
    }

    bool Is_DefaultValue(
        const vector<SpawnModuleData::ParticleBurstData>& value,
        const vector<SpawnModuleData::ParticleBurstData>& defaultValue)
    {
        if (value.size() != defaultValue.size())
            return false;

        for (size_t index = 0; index < value.size(); ++index)
        {
            if (!Is_DefaultValue(value[index], defaultValue[index]))
                return false;
        }

        return true;
    }

    template <typename T>
    bool Is_DefaultValue(const T& value, const T& defaultValue)
    {
        return value == defaultValue;
    }

    const char* Get_DistributionModeLabel(DistributionMode mode)
    {
        switch (mode)
        {
        case DistributionMode::Constant:
            return "상수";
        case DistributionMode::Uniform:
            return "유니폼 범위";
        case DistributionMode::ConstantCurve:
            return "상수 커브 (예정)";
        case DistributionMode::UniformCurve:
            return "유니폼 커브 (예정)";
        case DistributionMode::ParticleParameter:
            return "파티클 파라미터 (예정)";
        default:
            return "상수";
        }
    }
}

bool DetailProperty::Begin_PropertyTable(const char* id)
{
    constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
    constexpr float resetColumnWidth{ 40.f };
    if (!ImGui::BeginTable(id, 3, tableFlags))
        return false;

    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 0.4f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.55f);
    ImGui::TableSetupColumn("Reset", ImGuiTableColumnFlags_WidthFixed, resetColumnWidth);
    return true;
}

void DetailProperty::End_PropertyTable()
{
    ImGui::EndTable();
}

void DetailProperty::Draw_PropertyLabel(const char* label)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
}

bool DetailProperty::Draw_ResetButton(bool canReset)
{
    constexpr float resetButtonWidth{ 28.f };

    ImGui::TableSetColumnIndex(2);
    if (!canReset)
        ImGui::BeginDisabled();

    const bool clicked = ImGui::Button(ICON_FA_ARROW_ROTATE_LEFT "##Reset", ImVec2{ resetButtonWidth, 0.f });
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("기본값으로 리셋");

    if (!canReset)
        ImGui::EndDisabled();

    return canReset && clicked;
}

bool DetailProperty::Draw_StringProperty(const char* label, string& value, const string& defaultValue)
{
    char buffer[256]{};
    const size_t copyLength = min(value.size(), sizeof(buffer) - 1);
    memcpy(buffer, value.c_str(), copyLength);

    ImGui::PushID(label);
    DetailProperty::Draw_PropertyLabel(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool inputChanged = ImGui::InputText("##Value", buffer, sizeof(buffer));
    bool changed = inputChanged;
    const bool resetClicked = Draw_ResetButton(!Internal::Is_DefaultValue(value, defaultValue));
    if (resetClicked)
    {
        value = defaultValue;
        changed = true;
    }
    ImGui::PopID();

    if (inputChanged && !resetClicked)
        value = buffer;

    return changed;
}

bool DetailProperty::Draw_BoolProperty(const char* label, bool& value, bool defaultValue)
{
    ImGui::PushID(label);
    DetailProperty::Draw_PropertyLabel(label);
    bool changed = ImGui::Checkbox("##Value", &value);
    if (Draw_ResetButton(!Internal::Is_DefaultValue(value, defaultValue)))
    {
        value = defaultValue;
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

bool DetailProperty::Draw_FloatProperty(const char* label, float& value, float defaultValue, float speed)
{
    ImGui::PushID(label);
    Draw_PropertyLabel(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    bool changed = ImGui::DragFloat("##Value", &value, speed);
    if (Draw_ResetButton(!Internal::Is_DefaultValue(value, defaultValue)))
    {
        value = defaultValue;
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

bool DetailProperty::Draw_FloatDistributionGroup(const char* label, FloatDistributionData& value, const FloatDistributionData& defaultValue, float speed)
{
    bool changed = false;

    ImGui::PushID(label);
    const bool isOpen = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
    if (isOpen)
    {
        if (Begin_PropertyTable("Distribution"))
        {
            changed |= Internal::Draw_FloatDistributionProperty("분포 타입", value, defaultValue, speed);
            End_PropertyTable();
        }

        ImGui::TreePop();
    }
    ImGui::PopID();

    return changed;
}

bool DetailProperty::Draw_Vector2DistributionGroup(
    const char* label,
    Vector2DistributionData& value,
    const Vector2DistributionData& defaultValue,
    float speed)
{
    bool changed = false;

    ImGui::PushID(label);
    const bool isOpen = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
    if (isOpen)
    {
        if (Begin_PropertyTable("Distribution"))
        {
            changed |= Internal::Draw_Vector2DistributionProperty("분포 타입", value, defaultValue, speed);
            End_PropertyTable();
        }

        ImGui::TreePop();
    }
    ImGui::PopID();

    return changed;
}

bool DetailProperty::Draw_Vector3DistributionGroup(
    const char* label,
    Vector3DistributionData& value,
    const Vector3DistributionData& defaultValue,
    float speed)
{
    bool changed = false;

    ImGui::PushID(label);
    const bool isOpen = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
    if (isOpen)
    {
        if (Begin_PropertyTable("Distribution"))
        {
            changed |= Internal::Draw_Vector3DistributionProperty("분포 타입", value, defaultValue, speed);
            End_PropertyTable();
        }

        ImGui::TreePop();
    }
    ImGui::PopID();

    return changed;
}

bool DetailProperty::Draw_ColorDistributionGroup(
    const char* label,
    ColorDistributionData& value,
    const ColorDistributionData& defaultValue)
{
    bool changed = false;

    ImGui::PushID(label);
    const bool isOpen = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
    if (isOpen)
    {
        if (Begin_PropertyTable("Distribution"))
        {
            changed |= Internal::Draw_ColorDistributionProperty("분포 타입", value, defaultValue);
            End_PropertyTable();
        }

        ImGui::TreePop();
    }
    ImGui::PopID();

    return changed;
}

bool DetailProperty::Draw_BurstListProperty(
    vector<SpawnModuleData::ParticleBurstData>& burstList,
    const vector<SpawnModuleData::ParticleBurstData>& defaultValue)
{
    bool changed = false;

    ImGui::TextUnformatted("버스트 목록");
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_PLUS "##AddBurst"))
    {
        burstList.push_back(SpawnModuleData::ParticleBurstData{});
        changed = true;
    }
    ImGui::SameLine();
    if (!Internal::Is_DefaultValue(burstList, defaultValue))
    {
        if (ImGui::SmallButton(ICON_FA_ARROW_ROTATE_LEFT "##ResetBurstList"))
        {
            burstList = defaultValue;
            changed = true;
        }
    }
    else
    {
        ImGui::BeginDisabled();
        ImGui::SmallButton(ICON_FA_ARROW_ROTATE_LEFT "##ResetBurstList");
        ImGui::EndDisabled();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("기본값으로 리셋");

    if (burstList.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("비어 있음");
        return changed;
    }

    for (size_t index = 0; index < burstList.size();)
    {
        ImGui::PushID(static_cast<int>(index));

        string label = "버스트 " + to_string(index);
        const bool isOpen = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        ImGui::SameLine();
        const bool remove = ImGui::Button(ICON_FA_TRASH "##RemoveBurst");

        bool entryChanged = false;
        if (isOpen)
        {
            if (!remove)
            {
                if (Begin_PropertyTable("BurstEntry"))
                {
                    const SpawnModuleData::ParticleBurstData defaultBurst =
                        index < defaultValue.size() ? defaultValue[index] : SpawnModuleData::ParticleBurstData{};
                    entryChanged |= Internal::Draw_BurstEntryProperty(burstList[index], defaultBurst);
                    End_PropertyTable();
                }
            }
            ImGui::TreePop();
        }

        ImGui::PopID();

        if (remove)
        {
            burstList.erase(burstList.begin() + static_cast<vector<SpawnModuleData::ParticleBurstData>::difference_type>(index));
            changed = true;
            continue;
        }

        changed |= entryChanged;
        ++index;
    }

    return changed;
}

bool DetailProperty::Draw_ScreenAlignmentProperty(const char* label, EmitterScreenAlignment& value, EmitterScreenAlignment defaultValue)
{
    int currentIndex = 0;
    switch (value)
    {
    case EmitterScreenAlignment::Square:
        currentIndex = 0;
        break;
    case EmitterScreenAlignment::FacingCameraPosition:
        currentIndex = 1;
        break;
    case EmitterScreenAlignment::Rectangle:
        currentIndex = 2;
        break;
    case EmitterScreenAlignment::Velocity:
        currentIndex = 3;
        break;
    case EmitterScreenAlignment::AwayFromCenter:
        currentIndex = 4;
        break;
    case EmitterScreenAlignment::TypeSpecific:
        currentIndex = 5;
        break;
    case EmitterScreenAlignment::FacingCameraDistanceBlend:
        currentIndex = 6;
        break;
    default:
        currentIndex = 0;
        break;
    }

    static constexpr const char* kScreenAlignmentLabels[] = {
        "Square",
        "Facing Camera Position",
        "Rectangle",
        "Velocity",
        "Away from Center",
        "Type Specific",
        "Facing Camera Distance Blend",
    };

    ImGui::PushID(label);
    Draw_PropertyLabel(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool changed = ImGui::Combo("##Value", &currentIndex, kScreenAlignmentLabels, IM_ARRAYSIZE(kScreenAlignmentLabels));
    const bool resetClicked = Draw_ResetButton(!Internal::Is_DefaultValue(value, defaultValue));
    ImGui::PopID();

    if (resetClicked)
    {
        value = defaultValue;
        return true;
    }

    if (!changed)
        return false;

    switch (currentIndex)
    {
    case 0:
        value = EmitterScreenAlignment::Square;
        break;
    case 1:
        value = EmitterScreenAlignment::FacingCameraPosition;
        break;
    case 2:
        value = EmitterScreenAlignment::Rectangle;
        break;
    case 3:
        value = EmitterScreenAlignment::Velocity;
        break;
    case 4:
        value = EmitterScreenAlignment::AwayFromCenter;
        break;
    case 5:
        value = EmitterScreenAlignment::TypeSpecific;
        break;
    case 6:
        value = EmitterScreenAlignment::FacingCameraDistanceBlend;
        break;
    default:
        value = EmitterScreenAlignment::Square;
        break;
    }

    return true;
}

bool DetailProperty::Draw_SortModeProperty(const char* label, EmitterSortMode& value, EmitterSortMode defaultValue)
{
    int currentIndex = 0;
    switch (value)
    {
    case EmitterSortMode::None:
        currentIndex = 0;
        break;
    case EmitterSortMode::ViewProjDepth:
        currentIndex = 1;
        break;
    case EmitterSortMode::DistanceToView:
        currentIndex = 2;
        break;
    case EmitterSortMode::AgeOldestFirst:
        currentIndex = 3;
        break;
    case EmitterSortMode::AgeNewestFirst:
        currentIndex = 4;
        break;
    default:
        currentIndex = 0;
        break;
    }

    static constexpr const char* kSortModeLabels[] = {
        "None",
        "View Proj Depth",
        "Distance to View",
        "Age Oldest First",
        "Age Newest First",
    };

    ImGui::PushID(label);
    Draw_PropertyLabel(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool changed = ImGui::Combo("##Value", &currentIndex, kSortModeLabels, IM_ARRAYSIZE(kSortModeLabels));
    const bool resetClicked = Draw_ResetButton(!Internal::Is_DefaultValue(value, defaultValue));
    ImGui::PopID();

    if (resetClicked)
    {
        value = defaultValue;
        return true;
    }

    if (!changed)
        return false;

    switch (currentIndex)
    {
    case 0:
        value = EmitterSortMode::None;
        break;
    case 1:
        value = EmitterSortMode::ViewProjDepth;
        break;
    case 2:
        value = EmitterSortMode::DistanceToView;
        break;
    case 3:
        value = EmitterSortMode::AgeOldestFirst;
        break;
    case 4:
        value = EmitterSortMode::AgeNewestFirst;
        break;
    default:
        value = EmitterSortMode::None;
        break;
    }

    return true;
}

bool DetailProperty::Draw_UintProperty(const char* label, uint32& value, uint32 defaultValue)
{
    int intValue = static_cast<int>(value);

    ImGui::PushID(label);
    Draw_PropertyLabel(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool changed = ImGui::DragInt("##Value", &intValue, 1.f, 0, 100000);
    const bool resetClicked = Draw_ResetButton(!Internal::Is_DefaultValue(value, defaultValue));
    ImGui::PopID();

    if (resetClicked)
    {
        value = defaultValue;
        return true;
    }

    if (changed)
        value = static_cast<uint32>(max(0, intValue));

    return changed;
}

bool DetailProperty::Draw_Vec3Property(const char* label, Vec3& value, const Vec3& defaultValue, float speed)
{
    ImGui::PushID(label);
    Draw_PropertyLabel(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    bool changed = ImGui::DragFloat3("##Value", &value.x, speed);
    if (Draw_ResetButton(!Internal::Is_DefaultValue(value, defaultValue)))
    {
        value = defaultValue;
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

bool Internal::Draw_FloatDistributionProperty(
    const char* label,
    FloatDistributionData& value,
    const FloatDistributionData& defaultValue,
    float speed)
{
    bool changed = false;

    ImGui::PushID(label);
    DistributionMode nextMode = value.mode;
    DetailProperty::Draw_PropertyLabel(label);
    bool modeChanged = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##Value", Internal::Get_DistributionModeLabel(nextMode)))
    {
        auto drawSelectable = [&](DistributionMode mode, bool enabled)
        {
            if (!enabled)
                ImGui::BeginDisabled();

            const bool isSelected = nextMode == mode;
            if (ImGui::Selectable(Internal::Get_DistributionModeLabel(mode), isSelected) && enabled)
            {
                nextMode = mode;
                modeChanged = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();

            if (!enabled)
                ImGui::EndDisabled();
        };

        drawSelectable(DistributionMode::Constant, true);
        drawSelectable(DistributionMode::Uniform, true);
        drawSelectable(DistributionMode::ConstantCurve, false);
        drawSelectable(DistributionMode::UniformCurve, false);
        drawSelectable(DistributionMode::ParticleParameter, false);
        ImGui::EndCombo();
    }

    const bool resetDistribution = DetailProperty::Draw_ResetButton(!Internal::Is_DefaultValue(value, defaultValue));
    if (resetDistribution)
    {
        value = defaultValue;
        changed = true;
    }
    else if (modeChanged)
    {
        Internal::Transition_FloatDistributionMode(value, nextMode);
        changed = true;
    }

    switch (value.mode)
    {
    case DistributionMode::Constant:
        if (auto* constant = get_if<ConstantFloatDistributionData>(&value.payload))
        {
            const auto* defaultConstant = get_if<ConstantFloatDistributionData>(&defaultValue.payload);
            const float defaultField = defaultConstant != nullptr ? defaultConstant->value : constant->value;
            changed |= DetailProperty::Draw_FloatProperty("상수", constant->value, defaultField, speed);
        }
        break;

    case DistributionMode::Uniform:
        if (auto* uniform = get_if<UniformFloatDistributionData>(&value.payload))
        {
            const auto* defaultUniform = get_if<UniformFloatDistributionData>(&defaultValue.payload);
            const float defaultMin = defaultUniform != nullptr ? defaultUniform->minValue : uniform->minValue;
            const float defaultMax = defaultUniform != nullptr ? defaultUniform->maxValue : uniform->maxValue;
            changed |= DetailProperty::Draw_FloatProperty("최소", uniform->minValue, defaultMin, speed);
            changed |= DetailProperty::Draw_FloatProperty("최대", uniform->maxValue, defaultMax, speed);
            Normalize_FloatUniform(*uniform);
        }
        break;

    case DistributionMode::ConstantCurve:
    case DistributionMode::UniformCurve:
    case DistributionMode::ParticleParameter:
        break;

    default:
        break;
    }

    ImGui::PopID();
    return changed;
}

bool Internal::Draw_Vector2DistributionProperty(
    const char* label,
    Vector2DistributionData& value,
    const Vector2DistributionData& defaultValue,
    float speed)
{
    bool changed = false;

    ImGui::PushID(label);
    DistributionMode nextMode = value.mode;
    DetailProperty::Draw_PropertyLabel(label);
    bool modeChanged = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##Value", Internal::Get_DistributionModeLabel(nextMode)))
    {
        auto drawSelectable = [&](DistributionMode mode, bool enabled)
        {
            if (!enabled)
                ImGui::BeginDisabled();

            const bool isSelected = nextMode == mode;
            if (ImGui::Selectable(Internal::Get_DistributionModeLabel(mode), isSelected) && enabled)
            {
                nextMode = mode;
                modeChanged = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();

            if (!enabled)
                ImGui::EndDisabled();
        };

        drawSelectable(DistributionMode::Constant, true);
        drawSelectable(DistributionMode::Uniform, true);
        drawSelectable(DistributionMode::ConstantCurve, false);
        drawSelectable(DistributionMode::UniformCurve, false);
        drawSelectable(DistributionMode::ParticleParameter, false);
        ImGui::EndCombo();
    }

    const bool resetDistribution = DetailProperty::Draw_ResetButton(!Internal::Is_DefaultValue(value, defaultValue));
    if (resetDistribution)
    {
        value = defaultValue;
        changed = true;
    }
    else if (modeChanged)
    {
        Internal::Transition_Vector2DistributionMode(value, nextMode);
        changed = true;
    }

    switch (value.mode)
    {
    case DistributionMode::Constant:
        if (auto* constant = get_if<ConstantVector2DistributionData>(&value.payload))
        {
            const auto* defaultConstant = get_if<ConstantVector2DistributionData>(&defaultValue.payload);
            const Vec2 defaultField = defaultConstant != nullptr ? defaultConstant->value : constant->value;
            changed |= Internal::Draw_Vec2Property("상수", constant->value, defaultField, speed);
        }
        break;

    case DistributionMode::Uniform:
        if (auto* uniform = get_if<UniformVector2DistributionData>(&value.payload))
        {
            const auto* defaultUniform = get_if<UniformVector2DistributionData>(&defaultValue.payload);
            const Vec2 defaultMin = defaultUniform != nullptr ? defaultUniform->minValue : uniform->minValue;
            const Vec2 defaultMax = defaultUniform != nullptr ? defaultUniform->maxValue : uniform->maxValue;
            changed |= Internal::Draw_Vec2Property("최소", uniform->minValue, defaultMin, speed);
            changed |= Internal::Draw_Vec2Property("최대", uniform->maxValue, defaultMax, speed);
            Normalize_Vector2Uniform(*uniform);
        }
        break;

    case DistributionMode::ConstantCurve:
    case DistributionMode::UniformCurve:
    case DistributionMode::ParticleParameter:
        break;

    default:
        break;
    }

    ImGui::PopID();
    return changed;
}

bool Internal::Draw_Vector3DistributionProperty(
    const char* label,
    Vector3DistributionData& value,
    const Vector3DistributionData& defaultValue,
    float speed)
{
    bool changed = false;

    ImGui::PushID(label);
    DistributionMode nextMode = value.mode;
    DetailProperty::Draw_PropertyLabel(label);
    bool modeChanged = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##Value", Internal::Get_DistributionModeLabel(nextMode)))
    {
        auto drawSelectable = [&](DistributionMode mode, bool enabled)
        {
            if (!enabled)
                ImGui::BeginDisabled();

            const bool isSelected = nextMode == mode;
            if (ImGui::Selectable(Internal::Get_DistributionModeLabel(mode), isSelected) && enabled)
            {
                nextMode = mode;
                modeChanged = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();

            if (!enabled)
                ImGui::EndDisabled();
        };

        drawSelectable(DistributionMode::Constant, true);
        drawSelectable(DistributionMode::Uniform, true);
        drawSelectable(DistributionMode::ConstantCurve, false);
        drawSelectable(DistributionMode::UniformCurve, false);
        drawSelectable(DistributionMode::ParticleParameter, false);
        ImGui::EndCombo();
    }

    const bool resetDistribution = DetailProperty::Draw_ResetButton(!Internal::Is_DefaultValue(value, defaultValue));
    if (resetDistribution)
    {
        value = defaultValue;
        changed = true;
    }
    else if (modeChanged)
    {
        Internal::Transition_Vector3DistributionMode(value, nextMode);
        changed = true;
    }

    switch (value.mode)
    {
    case DistributionMode::Constant:
        if (auto* constant = get_if<ConstantVector3DistributionData>(&value.payload))
        {
            const auto* defaultConstant = get_if<ConstantVector3DistributionData>(&defaultValue.payload);
            const Vec3 defaultField = defaultConstant != nullptr ? defaultConstant->value : constant->value;
            changed |= DetailProperty::Draw_Vec3Property("상수", constant->value, defaultField, speed);
        }
        break;

    case DistributionMode::Uniform:
        if (auto* uniform = get_if<UniformVector3DistributionData>(&value.payload))
        {
            const auto* defaultUniform = get_if<UniformVector3DistributionData>(&defaultValue.payload);
            const Vec3 defaultMin = defaultUniform != nullptr ? defaultUniform->minValue : uniform->minValue;
            const Vec3 defaultMax = defaultUniform != nullptr ? defaultUniform->maxValue : uniform->maxValue;
            changed |= DetailProperty::Draw_Vec3Property("최소", uniform->minValue, defaultMin, speed);
            changed |= DetailProperty::Draw_Vec3Property("최대", uniform->maxValue, defaultMax, speed);
            Normalize_Vector3Uniform(*uniform);
        }
        break;

    case DistributionMode::ConstantCurve:
    case DistributionMode::UniformCurve:
    case DistributionMode::ParticleParameter:
        break;

    default:
        break;
    }

    ImGui::PopID();
    return changed;
}

bool Internal::Draw_ColorDistributionProperty(
    const char* label,
    ColorDistributionData& value,
    const ColorDistributionData& defaultValue)
{
    bool changed = false;

    ImGui::PushID(label);
    DistributionMode nextMode = value.mode;
    DetailProperty::Draw_PropertyLabel(label);
    bool modeChanged = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##Value", Internal::Get_DistributionModeLabel(nextMode)))
    {
        auto drawSelectable = [&](DistributionMode mode, bool enabled)
        {
            if (!enabled)
                ImGui::BeginDisabled();

            const bool isSelected = nextMode == mode;
            if (ImGui::Selectable(Internal::Get_DistributionModeLabel(mode), isSelected) && enabled)
            {
                nextMode = mode;
                modeChanged = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();

            if (!enabled)
                ImGui::EndDisabled();
        };

        drawSelectable(DistributionMode::Constant, true);
        drawSelectable(DistributionMode::Uniform, true);
        drawSelectable(DistributionMode::ConstantCurve, false);
        drawSelectable(DistributionMode::UniformCurve, false);
        drawSelectable(DistributionMode::ParticleParameter, false);
        ImGui::EndCombo();
    }

    const bool resetDistribution = DetailProperty::Draw_ResetButton(!Internal::Is_DefaultValue(value, defaultValue));
    if (resetDistribution)
    {
        value = defaultValue;
        changed = true;
    }
    else if (modeChanged)
    {
        Internal::Transition_ColorDistributionMode(value, nextMode);
        changed = true;
    }

    switch (value.mode)
    {
    case DistributionMode::Constant:
        if (auto* constant = get_if<ConstantColorDistributionData>(&value.payload))
        {
            const auto* defaultConstant = get_if<ConstantColorDistributionData>(&defaultValue.payload);
            const Color defaultField = defaultConstant != nullptr ? defaultConstant->value : constant->value;
            changed |= Internal::Draw_ColorProperty("상수", constant->value, defaultField);
        }
        break;

    case DistributionMode::Uniform:
        if (auto* uniform = get_if<UniformColorDistributionData>(&value.payload))
        {
            const auto* defaultUniform = get_if<UniformColorDistributionData>(&defaultValue.payload);
            const Color defaultMin = defaultUniform != nullptr ? defaultUniform->minValue : uniform->minValue;
            const Color defaultMax = defaultUniform != nullptr ? defaultUniform->maxValue : uniform->maxValue;
            changed |= Internal::Draw_ColorProperty("최소", uniform->minValue, defaultMin);
            changed |= Internal::Draw_ColorProperty("최대", uniform->maxValue, defaultMax);
            Normalize_ColorUniform(*uniform);
        }
        break;

    case DistributionMode::ConstantCurve:
    case DistributionMode::UniformCurve:
    case DistributionMode::ParticleParameter:
        break;

    default:
        break;
    }

    ImGui::PopID();
    return changed;
}

bool Internal::Draw_BurstEntryProperty(SpawnModuleData::ParticleBurstData& burst, const SpawnModuleData::ParticleBurstData& defaultValue)
{
    bool changed = false;
    changed |= DetailProperty::Draw_FloatProperty("시간", burst.time, defaultValue.time);
    changed |= DetailProperty::Draw_UintProperty("개수", burst.count, defaultValue.count);
    return changed;
}

bool Internal::Draw_Vec2Property(const char* label, Vec2& value, const Vec2& defaultValue, float speed)
{
    ImGui::PushID(label);
    DetailProperty::Draw_PropertyLabel(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    bool changed = ImGui::DragFloat2("##Value", &value.x, speed);
    if (DetailProperty::Draw_ResetButton(!Internal::Is_DefaultValue(value, defaultValue)))
    {
        value = defaultValue;
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

bool Internal::Draw_ColorProperty(const char* label, Color& value, const Color& defaultValue)
{
    ImGui::PushID(label);
    DetailProperty::Draw_PropertyLabel(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    bool changed = ImGui::ColorEdit4(
        "##Value",
        &value.x,
        ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar
    );
    if (changed)
        Clamp_Color01(value);
    if (DetailProperty::Draw_ResetButton(!Internal::Is_DefaultValue(value, defaultValue)))
    {
        value = defaultValue;
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

void Internal::Transition_FloatDistributionMode(FloatDistributionData& value, DistributionMode nextMode)
{
    if (value.mode == nextMode || !Is_DistributionModeEditable(nextMode))
        return;

    switch (nextMode)
    {
    case DistributionMode::Constant:
        if (const auto* uniform = get_if<UniformFloatDistributionData>(&value.payload))
            value.payload = ConstantFloatDistributionData{ uniform->minValue };
        else if (const auto* constant = get_if<ConstantFloatDistributionData>(&value.payload))
            value.payload = ConstantFloatDistributionData{ constant->value };
        else
            value.payload = ConstantFloatDistributionData{};
        break;

    case DistributionMode::Uniform:
        if (const auto* constant = get_if<ConstantFloatDistributionData>(&value.payload))
            value.payload = UniformFloatDistributionData{ constant->value, constant->value };
        else if (const auto* uniform = get_if<UniformFloatDistributionData>(&value.payload))
            value.payload = *uniform;
        else
            value.payload = UniformFloatDistributionData{};
        break;

    default:
        return;
    }

    value.mode = nextMode;
}

void Internal::Transition_Vector2DistributionMode(Vector2DistributionData& value, DistributionMode nextMode)
{
    if (value.mode == nextMode || !Is_DistributionModeEditable(nextMode))
        return;

    switch (nextMode)
    {
    case DistributionMode::Constant:
        if (const auto* uniform = get_if<UniformVector2DistributionData>(&value.payload))
            value.payload = ConstantVector2DistributionData{ uniform->minValue };
        else if (const auto* constant = get_if<ConstantVector2DistributionData>(&value.payload))
            value.payload = ConstantVector2DistributionData{ constant->value };
        else
            value.payload = ConstantVector2DistributionData{};
        break;

    case DistributionMode::Uniform:
        if (const auto* constant = get_if<ConstantVector2DistributionData>(&value.payload))
            value.payload = UniformVector2DistributionData{ constant->value, constant->value };
        else if (const auto* uniform = get_if<UniformVector2DistributionData>(&value.payload))
            value.payload = *uniform;
        else
            value.payload = UniformVector2DistributionData{};
        break;

    default:
        return;
    }

    value.mode = nextMode;
}

void Internal::Transition_Vector3DistributionMode(Vector3DistributionData& value, DistributionMode nextMode)
{
    if (value.mode == nextMode || !Is_DistributionModeEditable(nextMode))
        return;

    switch (nextMode)
    {
    case DistributionMode::Constant:
        if (const auto* uniform = get_if<UniformVector3DistributionData>(&value.payload))
            value.payload = ConstantVector3DistributionData{ uniform->minValue };
        else if (const auto* constant = get_if<ConstantVector3DistributionData>(&value.payload))
            value.payload = ConstantVector3DistributionData{ constant->value };
        else
            value.payload = ConstantVector3DistributionData{};
        break;

    case DistributionMode::Uniform:
        if (const auto* constant = get_if<ConstantVector3DistributionData>(&value.payload))
            value.payload = UniformVector3DistributionData{ constant->value, constant->value };
        else if (const auto* uniform = get_if<UniformVector3DistributionData>(&value.payload))
            value.payload = *uniform;
        else
            value.payload = UniformVector3DistributionData{};
        break;

    default:
        return;
    }

    value.mode = nextMode;
}

void Internal::Transition_ColorDistributionMode(ColorDistributionData& value, DistributionMode nextMode)
{
    if (value.mode == nextMode || !Is_DistributionModeEditable(nextMode))
        return;

    switch (nextMode)
    {
    case DistributionMode::Constant:
        if (const auto* uniform = get_if<UniformColorDistributionData>(&value.payload))
            value.payload = ConstantColorDistributionData{ uniform->minValue };
        else if (const auto* constant = get_if<ConstantColorDistributionData>(&value.payload))
            value.payload = ConstantColorDistributionData{ constant->value };
        else
            value.payload = ConstantColorDistributionData{};
        break;

    case DistributionMode::Uniform:
        if (const auto* constant = get_if<ConstantColorDistributionData>(&value.payload))
            value.payload = UniformColorDistributionData{ constant->value, constant->value };
        else if (const auto* uniform = get_if<UniformColorDistributionData>(&value.payload))
            value.payload = *uniform;
        else
            value.payload = UniformColorDistributionData{};
        break;

    default:
        return;
    }

    value.mode = nextMode;
}

NS_END
