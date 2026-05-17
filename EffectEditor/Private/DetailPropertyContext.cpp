#include "DetailPropertyContext.h"

#include <cfloat>
#include <climits>
#include "Helper_Distribution.h"

NS_BEGIN(EffectEditor)

class DetailPropertyContext::BasicDetailPropertyDrawer
{
public:
    bool Begin_PropertyTable(const char* id)
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

    void End_PropertyTable()
    {
        ImGui::EndTable();
    }

    void Draw_PropertyLabel(const char* label)
    {
        ImGui::TableNextRow();
        if (!_rowTintStack.empty())
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, _rowTintStack.back());
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::TableSetColumnIndex(1);
    }

    bool Draw_TreePropertyLabel(const char* label, ImGuiTreeNodeFlags flags = 0)
    {
        ImGui::TableNextRow();
        if (!_rowTintStack.empty())
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, _rowTintStack.back());
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        const bool isOpen = ImGui::TreeNodeEx(label, flags | ImGuiTreeNodeFlags_SpanAvailWidth);
        ImGui::TableSetColumnIndex(1);
        return isOpen;
    }

    void Push_RowTint(ImU32 color)
    {
        _rowTintStack.push_back(color);
    }

    void Pop_RowTint()
    {
        if (!_rowTintStack.empty())
            _rowTintStack.pop_back();
    }

    void Push_LabelTint(ImU32 color)
    {
        _labelTintStack.push_back(color);
    }

    void Pop_LabelTint()
    {
        if (!_labelTintStack.empty())
            _labelTintStack.pop_back();
    }

    bool Draw_ResetButton(bool canReset)
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

    bool Draw_StringProperty(const char* label, string& value, const string& defaultValue)
    {
        char buffer[256]{};
        const size_t copyLength = min(value.size(), sizeof(buffer) - 1);
        memcpy(buffer, value.c_str(), copyLength);

        ImGui::PushID(label);
        Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        const bool inputChanged = ImGui::InputText("##Value", buffer, sizeof(buffer));
        bool changed = inputChanged;
        const bool resetClicked = Draw_ResetButton(!Is_DefaultValue(value, defaultValue));
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

    bool Draw_BoolProperty(const char* label, bool& value, bool defaultValue)
    {
        ImGui::PushID(label);
        Draw_PropertyLabel(label);
        bool changed = ImGui::Checkbox("##Value", &value);
        if (Draw_ResetButton(!Is_DefaultValue(value, defaultValue)))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();
        return changed;
    }

    bool Draw_FloatProperty(const char* label, float& value, float defaultValue, float speed = 0.01f)
    {
        ImGui::PushID(label);
        Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = ImGui::DragFloat("##Value", &value, speed);
        if (Draw_ResetButton(!Is_DefaultValue(value, defaultValue)))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();
        return changed;
    }

    bool Draw_ColorProperty(const char* label, Color& value, const Color& defaultValue)
    {
        ImGui::PushID(label);
        Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = ImGui::ColorEdit4(
            "##Value",
            &value.x,
            ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar
        );
        if (changed)
            Clamp_Color01(value);
        if (Draw_ResetButton(!Is_DefaultValue(value, defaultValue)))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();
        return changed;
    }

    bool Draw_ColorRgbProperty(const char* label, Color& value, const Color& defaultValue)
    {
        ImGui::PushID(label);
        Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        float rgb[3]{ value.R(), value.G(), value.B() };
        bool changed = ImGui::ColorEdit3(
            "##Value",
            rgb,
            ImGuiColorEditFlags_Float
        );
        if (changed)
        {
            value.R(clamp(rgb[0], 0.f, 1.f));
            value.G(clamp(rgb[1], 0.f, 1.f));
            value.B(clamp(rgb[2], 0.f, 1.f));
        }
        if (Draw_ResetButton(!Is_DefaultValue(value.R(), defaultValue.R()) ||
                             !Is_DefaultValue(value.G(), defaultValue.G()) ||
                             !Is_DefaultValue(value.B(), defaultValue.B())))
        {
            value.R(defaultValue.R());
            value.G(defaultValue.G());
            value.B(defaultValue.B());
            changed = true;
        }
        ImGui::PopID();
        return changed;
    }

    bool Draw_UintProperty(const char* label, uint32& value, uint32 defaultValue)
    {
        ImGui::PushID(label);
        Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        int currentValue = static_cast<int>(value);
        bool changed = ImGui::DragInt("##Value", &currentValue, 1.f, 0, INT_MAX);
        if (changed)
            value = static_cast<uint32>(max(0, currentValue));
        if (Draw_ResetButton(!Is_DefaultValue(value, defaultValue)))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();
        return changed;
    }

    bool Draw_Vec2Property(const char* label, Vec2& value, const Vec2& defaultValue, float speed = 0.01f)
    {
        ImGui::PushID(label);
        Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = ImGui::DragFloat2("##Value", &value.x, speed);
        if (Draw_ResetButton(!Is_DefaultValue(value, defaultValue)))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();
        return changed;
    }

    bool Draw_Vec3Property(const char* label, Vec3& value, const Vec3& defaultValue, float speed = 0.01f)
    {
        ImGui::PushID(label);
        Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = ImGui::DragFloat3("##Value", &value.x, speed);
        if (Draw_ResetButton(!Is_DefaultValue(value, defaultValue)))
        {
            value = defaultValue;
            changed = true;
        }
        ImGui::PopID();
        return changed;
    }

    bool Draw_BurstListProperty(
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
        if (!Is_DefaultValue(burstList, defaultValue))
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
                        entryChanged |= Draw_BurstEntryProperty(burstList[index], defaultBurst);
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

    bool Draw_ScreenAlignmentProperty(const char* label, EmitterScreenAlignment& value, EmitterScreenAlignment defaultValue)
    {
        constexpr auto screenAlignmentValues = magic_enum::enum_values<EmitterScreenAlignment>();
        int currentIndex = static_cast<int>(magic_enum::enum_index(value).value_or(0));

        static constexpr const char* kScreenAlignmentLabels[] = {
            "Square",
            "Facing Camera Position",
            "Rectangle",
            "Velocity",
            "Away from Center",
            "Type Specific",
            "Facing Camera Distance Blend",
        };
        static_assert(IM_ARRAYSIZE(kScreenAlignmentLabels) == magic_enum::enum_count<EmitterScreenAlignment>());

        ImGui::PushID(label);
        Draw_PropertyLabel(label);
        ImGui::SetNextItemWidth(-FLT_MIN);
        const bool changed = ImGui::Combo("##Value", &currentIndex, kScreenAlignmentLabels, IM_ARRAYSIZE(kScreenAlignmentLabels));
        const bool resetClicked = Draw_ResetButton(!Is_DefaultValue(value, defaultValue));
        ImGui::PopID();

        if (resetClicked)
        {
            value = defaultValue;
            return true;
        }

        if (!changed)
            return false;

        if (currentIndex < 0 || currentIndex >= static_cast<int>(screenAlignmentValues.size()))
            value = EmitterScreenAlignment::Square;
        else
            value = screenAlignmentValues[currentIndex];

        return true;
    }

    bool Draw_SortModeProperty(const char* label, EmitterSortMode& value, EmitterSortMode defaultValue)
    {
        int currentIndex = 0;
        switch (value)
        {
        case EmitterSortMode::None: currentIndex = 0;
            break;
        case EmitterSortMode::ViewProjDepth: currentIndex = 1;
            break;
        case EmitterSortMode::DistanceToView: currentIndex = 2;
            break;
        case EmitterSortMode::AgeOldestFirst: currentIndex = 3;
            break;
        case EmitterSortMode::AgeNewestFirst: currentIndex = 4;
            break;
        default: currentIndex = 0;
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
        const bool resetClicked = Draw_ResetButton(!Is_DefaultValue(value, defaultValue));
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
        case 0: value = EmitterSortMode::None;
            break;
        case 1: value = EmitterSortMode::ViewProjDepth;
            break;
        case 2: value = EmitterSortMode::DistanceToView;
            break;
        case 3: value = EmitterSortMode::AgeOldestFirst;
            break;
        case 4: value = EmitterSortMode::AgeNewestFirst;
            break;
        default: value = EmitterSortMode::None;
            break;
        }

        return true;
    }

private:
    vector<ImU32> _rowTintStack{};
    vector<ImU32> _labelTintStack{};

private:
    static bool Is_DefaultValue(float value, float defaultValue) { return value == defaultValue; }
    static bool Is_DefaultValue(uint32 value, uint32 defaultValue) { return value == defaultValue; }
    static bool Is_DefaultValue(bool value, bool defaultValue) { return value == defaultValue; }
    static bool Is_DefaultValue(EmitterScreenAlignment value, EmitterScreenAlignment defaultValue) { return value == defaultValue; }
    static bool Is_DefaultValue(EmitterSortMode value, EmitterSortMode defaultValue) { return value == defaultValue; }
    static bool Is_DefaultValue(const string& value, const string& defaultValue) { return value == defaultValue; }

    static bool Is_DefaultValue(const Vec2& value, const Vec2& defaultValue)
    {
        return value.x == defaultValue.x && value.y == defaultValue.y;
    }

    static bool Is_DefaultValue(const Vec3& value, const Vec3& defaultValue)
    {
        return value.x == defaultValue.x && value.y == defaultValue.y && value.z == defaultValue.z;
    }

    static bool Is_DefaultValue(const Color& value, const Color& defaultValue)
    {
        return value.x == defaultValue.x &&
               value.y == defaultValue.y &&
               value.z == defaultValue.z &&
               value.w == defaultValue.w;
    }

    static bool Is_DefaultValue(const SpawnModuleData::ParticleBurstData& value, const SpawnModuleData::ParticleBurstData& defaultValue)
    {
        return Is_DefaultValue(value.time, defaultValue.time) && value.count == defaultValue.count;
    }

    static bool Is_DefaultValue(
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

    bool Draw_BurstEntryProperty(SpawnModuleData::ParticleBurstData& burst, const SpawnModuleData::ParticleBurstData& defaultValue)
    {
        bool changed = false;
        changed |= Draw_FloatProperty("시간", burst.time, defaultValue.time);
        changed |= Draw_UintProperty("개수", burst.count, defaultValue.count);
        return changed;
    }
};

class DetailPropertyContext::DistributionDetailPropertyDrawer
{
public:
    explicit DistributionDetailPropertyDrawer(BasicDetailPropertyDrawer& basicDrawer)
        : _basicDrawer{ basicDrawer }
    {
    }

    bool Draw_FloatDistributionGroup(const char* label, FloatDistributionData& value, const FloatDistributionData& defaultValue, float speed = 0.01f)
    {
        bool changed = false;

        ImGui::PushID(label);
        const bool isOpen = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
        if (isOpen)
        {
            if (_basicDrawer.Begin_PropertyTable("Distribution"))
            {
                changed |= Draw_FloatDistributionProperty("분포 타입", value, defaultValue, speed);
                _basicDrawer.End_PropertyTable();
            }

            ImGui::TreePop();
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_Vector2DistributionGroup(const char* label, Vector2DistributionData& value, const Vector2DistributionData& defaultValue, float speed = 0.01f)
    {
        bool changed = false;

        ImGui::PushID(label);
        const bool isOpen = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
        if (isOpen)
        {
            if (_basicDrawer.Begin_PropertyTable("Distribution"))
            {
                changed |= Draw_Vector2DistributionProperty("분포 타입", value, defaultValue, speed);
                _basicDrawer.End_PropertyTable();
            }

            ImGui::TreePop();
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_Vector3DistributionGroup(const char* label, Vector3DistributionData& value, const Vector3DistributionData& defaultValue, float speed = 0.01f)
    {
        bool changed = false;

        ImGui::PushID(label);
        const bool isOpen = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
        if (isOpen)
        {
            if (_basicDrawer.Begin_PropertyTable("Distribution"))
            {
                changed |= Draw_Vector3DistributionProperty("분포 타입", value, defaultValue, speed);
                _basicDrawer.End_PropertyTable();
            }

            ImGui::TreePop();
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_ColorDistributionGroup(const char* label, ColorDistributionData& value, const ColorDistributionData& defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        const bool isOpen = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
        if (isOpen)
        {
            if (_basicDrawer.Begin_PropertyTable("Distribution"))
            {
                changed |= Draw_ColorDistributionProperty("분포 타입", value, defaultValue);
                _basicDrawer.End_PropertyTable();
            }

            ImGui::TreePop();
        }
        ImGui::PopID();

        return changed;
    }

    bool Draw_ColorRgbDistributionGroup(const char* label, ColorDistributionData& value, const ColorDistributionData& defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        const bool isOpen = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
        if (isOpen)
        {
            if (_basicDrawer.Begin_PropertyTable("Distribution"))
            {
                changed |= Draw_ColorRgbDistributionProperty("분포 타입", value, defaultValue);
                _basicDrawer.End_PropertyTable();
            }

            ImGui::TreePop();
        }
        ImGui::PopID();

        return changed;
    }

private:
    BasicDetailPropertyDrawer& _basicDrawer;

private:
    enum class DistributionValueKind : uint8
    {
        Float,
        Vec2,
        Vec3,
        Color,
    };

    static ImVec4 Lerp_Color(const ImVec4& lhs, const ImVec4& rhs, float t)
    {
        return ImVec4{
            lhs.x + (rhs.x - lhs.x) * t,
            lhs.y + (rhs.y - lhs.y) * t,
            lhs.z + (rhs.z - lhs.z) * t,
            lhs.w + (rhs.w - lhs.w) * t,
        };
    }

    static ImU32 Get_DistributionPayloadRowTint()
    {
        const ImVec4 frameBg = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
        const ImVec4 tint = Lerp_Color(frameBg, ImVec4{ 0.34f, 0.34f, 0.34f, frameBg.w }, 0.45f);
        return ImGui::GetColorU32(tint);
    }

    static ImU32 Get_DistributionKeyHeaderRowTint()
    {
        const ImVec4 frameBg = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
        const ImVec4 tint = Lerp_Color(frameBg, ImVec4{ 0.38f, 0.38f, 0.38f, frameBg.w }, 0.55f);
        return ImGui::GetColorU32(tint);
    }

    static ImU32 Get_DistributionKeyBodyRowTint()
    {
        const ImVec4 frameBg = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
        const ImVec4 tint = Lerp_Color(frameBg, ImVec4{ 0.35f, 0.35f, 0.35f, frameBg.w }, 0.40f);
        return ImGui::GetColorU32(tint);
    }

    static ImU32 Get_DistributionKeyLabelTint()
    {
        const ImVec4 frameBg = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
        const ImVec4 headerActive = ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive);
        const ImVec4 tint = Lerp_Color(frameBg, headerActive, 0.12f);
        return ImGui::GetColorU32(tint);
    }

private:
    static const char* Get_DistributionModeLabel(DistributionValueKind kind, DistributionMode mode)
    {
        switch (kind)
        {
        case DistributionValueKind::Float:
            switch (mode)
            {
            case DistributionMode::Constant: return "float 고정값";
            case DistributionMode::Uniform: return "float 랜덤값";
            case DistributionMode::ConstantCurve: return "float 고정값 커브";
            case DistributionMode::UniformCurve: return "float 랜덤 커브 (예정)";
            case DistributionMode::ParticleParameter: return "float 파티클 파라미터 (예정)";
            default: return "float 고정값";
            }

        case DistributionValueKind::Vec2:
            switch (mode)
            {
            case DistributionMode::Constant: return "Vec2 고정값";
            case DistributionMode::Uniform: return "Vec2 랜덤값";
            case DistributionMode::ConstantCurve: return "Vec2 고정값 커브 (예정)";
            case DistributionMode::UniformCurve: return "Vec2 랜덤 커브 (예정)";
            case DistributionMode::ParticleParameter: return "Vec2 파티클 파라미터 (예정)";
            default: return "Vec2 고정값";
            }

        case DistributionValueKind::Vec3:
            switch (mode)
            {
            case DistributionMode::Constant: return "Vec3 고정값";
            case DistributionMode::Uniform: return "Vec3 랜덤값";
            case DistributionMode::ConstantCurve: return "Vec3 고정값 커브 (예정)";
            case DistributionMode::UniformCurve: return "Vec3 랜덤 커브 (예정)";
            case DistributionMode::ParticleParameter: return "Vec3 파티클 파라미터 (예정)";
            default: return "Vec3 고정값";
            }

        case DistributionValueKind::Color:
            switch (mode)
            {
            case DistributionMode::Constant: return "Color 고정값";
            case DistributionMode::Uniform: return "Color 랜덤값";
            case DistributionMode::ConstantCurve: return "Color 고정값 커브 (예정)";
            case DistributionMode::UniformCurve: return "Color 랜덤 커브 (예정)";
            case DistributionMode::ParticleParameter: return "Color 파티클 파라미터 (예정)";
            default: return "Color 고정값";
            }

        default:
            return "float 고정값";
        }
    }

    static const char* Get_FloatCurveInterpolationLabel(FloatCurveInterpolationMode mode)
    {
        switch (mode)
        {
        case FloatCurveInterpolationMode::Linear: return "Linear";
        case FloatCurveInterpolationMode::Constant: return "Constant";
        case FloatCurveInterpolationMode::CurveAuto: return "Curve Auto (예정)";
        case FloatCurveInterpolationMode::CurveUser: return "Curve User (예정)";
        case FloatCurveInterpolationMode::CurveBreak: return "Curve Break (예정)";
        case FloatCurveInterpolationMode::CurveAutoClamped: return "Curve Auto Clamped";
        default: return "Linear";
        }
    }

    static void Draw_FloatCurveInterpolationSelectable(
        FloatCurveInterpolationMode candidate,
        bool enabled,
        FloatCurveInterpolationMode& current,
        bool& changed)
    {
        if (!enabled)
            ImGui::BeginDisabled();

        const bool isSelected = current == candidate;
        if (ImGui::Selectable(Get_FloatCurveInterpolationLabel(candidate), isSelected) && enabled)
        {
            current = candidate;
            changed = true;
        }

        if (isSelected)
            ImGui::SetItemDefaultFocus();

        if (!enabled)
            ImGui::EndDisabled();
    }

    static float Evaluate_FloatCurveAutoClampedTangent(
        const FloatCurveKeyData* prevKey,
        const FloatCurveKeyData& key,
        const FloatCurveKeyData* nextKey)
    {
        if (prevKey == nullptr || nextKey == nullptr)
            return 0.f;

        const float dt = nextKey->time - prevKey->time;
        if (dt <= 0.f)
            return 0.f;

        return (nextKey->value - prevKey->value) / dt;
    }

    static void Normalize_FloatCurveKeys(ConstantCurveFloatDistributionData& value)
    {
        if (value.keys.empty())
        {
            value.keys = {
                FloatCurveKeyData{ 0.f, 0.f, 0.f, 0.f, FloatCurveInterpolationMode::Linear },
                FloatCurveKeyData{ 1.f, 0.f, 0.f, 0.f, FloatCurveInterpolationMode::Linear }
            };
            return;
        }

        ranges::sort(
            value.keys,
            [](const FloatCurveKeyData& lhs, const FloatCurveKeyData& rhs)
            {
                return lhs.time < rhs.time;
            }
        );

        for (FloatCurveKeyData& key : value.keys)
            key.time = clamp(key.time, 0.f, 1.f);

        for (size_t index = 0; index < value.keys.size(); ++index)
        {
            FloatCurveKeyData& key = value.keys[index];
            const FloatCurveKeyData* prevKey = index > 0 ? &value.keys[index - 1] : nullptr;
            const FloatCurveKeyData* nextKey = index + 1 < value.keys.size() ? &value.keys[index + 1] : nullptr;

            if (key.interpolationMode == FloatCurveInterpolationMode::CurveAutoClamped)
            {
                const float tangent = Evaluate_FloatCurveAutoClampedTangent(prevKey, key, nextKey);
                key.arriveTangent = tangent;
                key.leaveTangent = tangent;
            }
        }
    }

    bool Draw_FloatCurveKeyListProperty(
        ConstantCurveFloatDistributionData& value,
        const ConstantCurveFloatDistributionData& defaultValue,
        float speed = 0.01f)
    {
        bool changed = false;

        ImGui::PushID("PointsRow");
        _basicDrawer.Draw_PropertyLabel("포인트");
        ImGui::AlignTextToFramePadding();
        ImGui::Text("배열 엘리먼트: %zu", value.keys.size());
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_PLUS "##AddFloatCurveKey"))
        {
            const FloatCurveKeyData seed = value.keys.empty() ? FloatCurveKeyData{} : value.keys.back();
            value.keys.push_back(seed);
            changed = true;
        }
        _basicDrawer.Draw_ResetButton(false);
        ImGui::PopID();

        for (size_t index = 0; index < value.keys.size(); ++index)
        {
            ImGui::PushID(static_cast<int>(index));
            FloatCurveKeyData& key = value.keys[index];
            const FloatCurveKeyData defaultKey = index < defaultValue.keys.size() ? defaultValue.keys[index] : key;

            if (index > 0)
                ImGui::Dummy(ImVec2{ 0.f, 4.f });

            const string rowId = "인덱스 [" + to_string(index) + "]";
            _basicDrawer.Push_RowTint(Get_DistributionKeyHeaderRowTint());
            _basicDrawer.Push_LabelTint(Get_DistributionKeyLabelTint());
            const bool isOpen = _basicDrawer.Draw_TreePropertyLabel(rowId.c_str());
            _basicDrawer.Draw_ResetButton(false);
            _basicDrawer.Pop_LabelTint();
            if (isOpen)
            {
                _basicDrawer.Push_RowTint(Get_DistributionKeyBodyRowTint());
                changed |= _basicDrawer.Draw_FloatProperty("In 값", key.time, defaultKey.time, speed);
                changed |= _basicDrawer.Draw_FloatProperty("Out 값", key.value, defaultKey.value, speed);

                _basicDrawer.Draw_PropertyLabel("보간 모드");
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::BeginCombo("##Interpolation", Get_FloatCurveInterpolationLabel(key.interpolationMode)))
                {
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::Linear, true, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::Constant, true, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::CurveAutoClamped, true, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::CurveAuto, false, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::CurveUser, false, key.interpolationMode, changed);
                    Draw_FloatCurveInterpolationSelectable(FloatCurveInterpolationMode::CurveBreak, false, key.interpolationMode, changed);
                    ImGui::EndCombo();
                }
                _basicDrawer.Draw_ResetButton(false);

                const bool showTangents =
                    key.interpolationMode == FloatCurveInterpolationMode::CurveAutoClamped &&
                    ImGui::TreeNode("고급");
                if (showTangents)
                {
                    changed |= _basicDrawer.Draw_FloatProperty("도착 탄젠트", key.arriveTangent, defaultKey.arriveTangent, speed);
                    changed |= _basicDrawer.Draw_FloatProperty("출발 탄젠트", key.leaveTangent, defaultKey.leaveTangent, speed);
                    ImGui::TreePop();
                }

                _basicDrawer.Pop_RowTint();
                ImGui::TreePop();
            }
            _basicDrawer.Pop_RowTint();
            ImGui::PopID();
        }

        return changed;
    }

    bool Draw_FloatDistributionProperty(
        const char* label,
        FloatDistributionData& value,
        const FloatDistributionData& defaultValue,
        float speed = 0.01f)
    {
        bool changed = false;

        ImGui::PushID(label);
        DistributionMode nextMode = value.mode;
        ImGui::PushID("DistributionModeRow");
        _basicDrawer.Draw_PropertyLabel(label);
        bool modeChanged = false;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_DistributionModeLabel(DistributionValueKind::Float, nextMode)))
        {
            auto draw_selectable = [&](DistributionMode mode, bool enabled)
            {
                if (!enabled)
                    ImGui::BeginDisabled();

                const bool isSelected = nextMode == mode;
                if (ImGui::Selectable(Get_DistributionModeLabel(DistributionValueKind::Float, mode), isSelected) && enabled)
                {
                    nextMode = mode;
                    modeChanged = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();

                if (!enabled)
                    ImGui::EndDisabled();
            };

            draw_selectable(DistributionMode::Constant, true);
            draw_selectable(DistributionMode::Uniform, true);
            draw_selectable(DistributionMode::ConstantCurve, true);
            draw_selectable(DistributionMode::UniformCurve, false);
            draw_selectable(DistributionMode::ParticleParameter, false);
            ImGui::EndCombo();
        }

        const bool resetDistribution = _basicDrawer.Draw_ResetButton(!Is_DefaultValue(value, defaultValue));
        if (resetDistribution)
        {
            value = defaultValue;
            changed = true;
        }
        else if (modeChanged)
        {
            Transition_FloatDistributionMode(value, nextMode);
            changed = true;
        }
        ImGui::PopID();

        _basicDrawer.Push_RowTint(Get_DistributionPayloadRowTint());
        switch (value.mode)
        {
        case DistributionMode::Constant:
            if (auto* constant = get_if<ConstantFloatDistributionData>(&value.payload))
            {
                const auto* defaultConstant = get_if<ConstantFloatDistributionData>(&defaultValue.payload);
                const float defaultField = defaultConstant != nullptr ? defaultConstant->value : constant->value;
                changed |= _basicDrawer.Draw_FloatProperty("상수", constant->value, defaultField, speed);
            }
            break;

        case DistributionMode::Uniform:
            if (auto* uniform = get_if<UniformFloatDistributionData>(&value.payload))
            {
                const auto* defaultUniform = get_if<UniformFloatDistributionData>(&defaultValue.payload);
                const float defaultMin = defaultUniform != nullptr ? defaultUniform->minValue : uniform->minValue;
                const float defaultMax = defaultUniform != nullptr ? defaultUniform->maxValue : uniform->maxValue;
                changed |= _basicDrawer.Draw_FloatProperty("최소", uniform->minValue, defaultMin, speed);
                changed |= _basicDrawer.Draw_FloatProperty("최대", uniform->maxValue, defaultMax, speed);
                Normalize_FloatUniform(*uniform);
            }
            break;

        case DistributionMode::ConstantCurve:
            if (auto* curve = get_if<ConstantCurveFloatDistributionData>(&value.payload))
            {
                const auto* defaultCurve = get_if<ConstantCurveFloatDistributionData>(&defaultValue.payload);
                const FloatDistributionData fallbackCurveData = FloatDistributionData::Make_ConstantCurve(0.f);
                const auto* fallbackDefault = get_if<ConstantCurveFloatDistributionData>(&fallbackCurveData.payload);
                changed |= Draw_FloatCurveKeyListProperty(*curve, defaultCurve != nullptr ? *defaultCurve : *fallbackDefault, speed);
                Normalize_FloatCurveKeys(*curve);
            }
            break;

        case DistributionMode::UniformCurve:
        case DistributionMode::ParticleParameter:
        default:
            break;
        }
        _basicDrawer.Pop_RowTint();

        ImGui::PopID();
        return changed;
    }

    bool Draw_Vector2DistributionProperty(
        const char* label,
        Vector2DistributionData& value,
        const Vector2DistributionData& defaultValue,
        float speed = 0.01f)
    {
        bool changed = false;

        ImGui::PushID(label);
        DistributionMode nextMode = value.mode;
        ImGui::PushID("DistributionModeRow");
        _basicDrawer.Draw_PropertyLabel(label);
        bool modeChanged = false;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_DistributionModeLabel(DistributionValueKind::Vec2, nextMode)))
        {
            auto draw_selectable = [&](DistributionMode mode, bool enabled)
            {
                if (!enabled)
                    ImGui::BeginDisabled();
                const bool isSelected = nextMode == mode;
                if (ImGui::Selectable(Get_DistributionModeLabel(DistributionValueKind::Vec2, mode), isSelected) && enabled)
                {
                    nextMode = mode;
                    modeChanged = true;
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
                if (!enabled)
                    ImGui::EndDisabled();
            };

            draw_selectable(DistributionMode::Constant, true);
            draw_selectable(DistributionMode::Uniform, true);
            draw_selectable(DistributionMode::ConstantCurve, false);
            draw_selectable(DistributionMode::UniformCurve, false);
            draw_selectable(DistributionMode::ParticleParameter, false);
            ImGui::EndCombo();
        }

        const bool resetDistribution = _basicDrawer.Draw_ResetButton(!Is_DefaultValue(value, defaultValue));
        if (resetDistribution)
        {
            value = defaultValue;
            changed = true;
        }
        else if (modeChanged)
        {
            Transition_Vector2DistributionMode(value, nextMode);
            changed = true;
        }
        ImGui::PopID();

        _basicDrawer.Push_RowTint(Get_DistributionPayloadRowTint());
        switch (value.mode)
        {
        case DistributionMode::Constant:
            if (auto* constant = get_if<ConstantVector2DistributionData>(&value.payload))
            {
                const auto* defaultConstant = get_if<ConstantVector2DistributionData>(&defaultValue.payload);
                const Vec2 defaultField = defaultConstant != nullptr ? defaultConstant->value : constant->value;
                changed |= _basicDrawer.Draw_Vec2Property("상수", constant->value, defaultField, speed);
            }
            break;
        case DistributionMode::Uniform:
            if (auto* uniform = get_if<UniformVector2DistributionData>(&value.payload))
            {
                const auto* defaultUniform = get_if<UniformVector2DistributionData>(&defaultValue.payload);
                const Vec2 defaultMin = defaultUniform != nullptr ? defaultUniform->minValue : uniform->minValue;
                const Vec2 defaultMax = defaultUniform != nullptr ? defaultUniform->maxValue : uniform->maxValue;
                changed |= _basicDrawer.Draw_Vec2Property("최소", uniform->minValue, defaultMin, speed);
                changed |= _basicDrawer.Draw_Vec2Property("최대", uniform->maxValue, defaultMax, speed);
                Normalize_Vector2Uniform(*uniform);
            }
            break;
        default:
            break;
        }
        _basicDrawer.Pop_RowTint();

        ImGui::PopID();
        return changed;
    }

    bool Draw_Vector3DistributionProperty(
        const char* label,
        Vector3DistributionData& value,
        const Vector3DistributionData& defaultValue,
        float speed = 0.01f)
    {
        bool changed = false;

        ImGui::PushID(label);
        DistributionMode nextMode = value.mode;
        ImGui::PushID("DistributionModeRow");
        _basicDrawer.Draw_PropertyLabel(label);
        bool modeChanged = false;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_DistributionModeLabel(DistributionValueKind::Vec3, nextMode)))
        {
            auto draw_selectable = [&](DistributionMode mode, bool enabled)
            {
                if (!enabled)
                    ImGui::BeginDisabled();
                const bool isSelected = nextMode == mode;
                if (ImGui::Selectable(Get_DistributionModeLabel(DistributionValueKind::Vec3, mode), isSelected) && enabled)
                {
                    nextMode = mode;
                    modeChanged = true;
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
                if (!enabled)
                    ImGui::EndDisabled();
            };

            draw_selectable(DistributionMode::Constant, true);
            draw_selectable(DistributionMode::Uniform, true);
            draw_selectable(DistributionMode::ConstantCurve, false);
            draw_selectable(DistributionMode::UniformCurve, false);
            draw_selectable(DistributionMode::ParticleParameter, false);
            ImGui::EndCombo();
        }

        const bool resetDistribution = _basicDrawer.Draw_ResetButton(!Is_DefaultValue(value, defaultValue));
        if (resetDistribution)
        {
            value = defaultValue;
            changed = true;
        }
        else if (modeChanged)
        {
            Transition_Vector3DistributionMode(value, nextMode);
            changed = true;
        }
        ImGui::PopID();

        _basicDrawer.Push_RowTint(Get_DistributionPayloadRowTint());
        switch (value.mode)
        {
        case DistributionMode::Constant:
            if (auto* constant = get_if<ConstantVector3DistributionData>(&value.payload))
            {
                const auto* defaultConstant = get_if<ConstantVector3DistributionData>(&defaultValue.payload);
                const Vec3 defaultField = defaultConstant != nullptr ? defaultConstant->value : constant->value;
                changed |= _basicDrawer.Draw_Vec3Property("상수", constant->value, defaultField, speed);
            }
            break;
        case DistributionMode::Uniform:
            if (auto* uniform = get_if<UniformVector3DistributionData>(&value.payload))
            {
                const auto* defaultUniform = get_if<UniformVector3DistributionData>(&defaultValue.payload);
                const Vec3 defaultMin = defaultUniform != nullptr ? defaultUniform->minValue : uniform->minValue;
                const Vec3 defaultMax = defaultUniform != nullptr ? defaultUniform->maxValue : uniform->maxValue;
                changed |= _basicDrawer.Draw_Vec3Property("최소", uniform->minValue, defaultMin, speed);
                changed |= _basicDrawer.Draw_Vec3Property("최대", uniform->maxValue, defaultMax, speed);
                Normalize_Vector3Uniform(*uniform);
            }
            break;
        default:
            break;
        }
        _basicDrawer.Pop_RowTint();

        ImGui::PopID();
        return changed;
    }

    bool Draw_ColorDistributionProperty(
        const char* label,
        ColorDistributionData& value,
        const ColorDistributionData& defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        DistributionMode nextMode = value.mode;
        ImGui::PushID("DistributionModeRow");
        _basicDrawer.Draw_PropertyLabel(label);
        bool modeChanged = false;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_DistributionModeLabel(DistributionValueKind::Color, nextMode)))
        {
            auto draw_selectable = [&](DistributionMode mode, bool enabled)
            {
                if (!enabled)
                    ImGui::BeginDisabled();
                const bool isSelected = nextMode == mode;
                if (ImGui::Selectable(Get_DistributionModeLabel(DistributionValueKind::Color, mode), isSelected) && enabled)
                {
                    nextMode = mode;
                    modeChanged = true;
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
                if (!enabled)
                    ImGui::EndDisabled();
            };

            draw_selectable(DistributionMode::Constant, true);
            draw_selectable(DistributionMode::Uniform, true);
            draw_selectable(DistributionMode::ConstantCurve, false);
            draw_selectable(DistributionMode::UniformCurve, false);
            draw_selectable(DistributionMode::ParticleParameter, false);
            ImGui::EndCombo();
        }

        const bool resetDistribution = _basicDrawer.Draw_ResetButton(!Is_DefaultValue(value, defaultValue));
        if (resetDistribution)
        {
            value = defaultValue;
            changed = true;
        }
        else if (modeChanged)
        {
            Transition_ColorDistributionMode(value, nextMode);
            changed = true;
        }
        ImGui::PopID();

        _basicDrawer.Push_RowTint(Get_DistributionPayloadRowTint());
        switch (value.mode)
        {
        case DistributionMode::Constant:
            if (auto* constant = get_if<ConstantColorDistributionData>(&value.payload))
            {
                const auto* defaultConstant = get_if<ConstantColorDistributionData>(&defaultValue.payload);
                const Color defaultField = defaultConstant != nullptr ? defaultConstant->value : constant->value;
                changed |= _basicDrawer.Draw_ColorProperty("상수", constant->value, defaultField);
            }
            break;
        case DistributionMode::Uniform:
            if (auto* uniform = get_if<UniformColorDistributionData>(&value.payload))
            {
                const auto* defaultUniform = get_if<UniformColorDistributionData>(&defaultValue.payload);
                const Color defaultMin = defaultUniform != nullptr ? defaultUniform->minValue : uniform->minValue;
                const Color defaultMax = defaultUniform != nullptr ? defaultUniform->maxValue : uniform->maxValue;
                changed |= _basicDrawer.Draw_ColorProperty("최소", uniform->minValue, defaultMin);
                changed |= _basicDrawer.Draw_ColorProperty("최대", uniform->maxValue, defaultMax);
                Normalize_ColorUniform(*uniform);
            }
            break;
        default:
            break;
        }
        _basicDrawer.Pop_RowTint();

        ImGui::PopID();
        return changed;
    }

    bool Draw_ColorRgbDistributionProperty(
        const char* label,
        ColorDistributionData& value,
        const ColorDistributionData& defaultValue)
    {
        bool changed = false;

        ImGui::PushID(label);
        DistributionMode nextMode = value.mode;
        ImGui::PushID("DistributionModeRow");
        _basicDrawer.Draw_PropertyLabel(label);
        bool modeChanged = false;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##Value", Get_DistributionModeLabel(DistributionValueKind::Color, nextMode)))
        {
            auto draw_selectable = [&](DistributionMode mode, bool enabled)
            {
                if (!enabled)
                    ImGui::BeginDisabled();
                const bool isSelected = nextMode == mode;
                if (ImGui::Selectable(Get_DistributionModeLabel(DistributionValueKind::Color, mode), isSelected) && enabled)
                {
                    nextMode = mode;
                    modeChanged = true;
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
                if (!enabled)
                    ImGui::EndDisabled();
            };

            draw_selectable(DistributionMode::Constant, true);
            draw_selectable(DistributionMode::Uniform, true);
            draw_selectable(DistributionMode::ConstantCurve, false);
            draw_selectable(DistributionMode::UniformCurve, false);
            draw_selectable(DistributionMode::ParticleParameter, false);
            ImGui::EndCombo();
        }

        const bool resetDistribution = _basicDrawer.Draw_ResetButton(!Is_DefaultValue(value, defaultValue));
        if (resetDistribution)
        {
            value = defaultValue;
            changed = true;
        }
        else if (modeChanged)
        {
            Transition_ColorDistributionMode(value, nextMode);
            changed = true;
        }
        ImGui::PopID();

        _basicDrawer.Push_RowTint(Get_DistributionPayloadRowTint());
        switch (value.mode)
        {
        case DistributionMode::Constant:
            if (auto* constant = get_if<ConstantColorDistributionData>(&value.payload))
            {
                const auto* defaultConstant = get_if<ConstantColorDistributionData>(&defaultValue.payload);
                const Color defaultField = defaultConstant != nullptr ? defaultConstant->value : constant->value;
                changed |= _basicDrawer.Draw_ColorRgbProperty("상수 RGB", constant->value, defaultField);
            }
            break;
        case DistributionMode::Uniform:
            if (auto* uniform = get_if<UniformColorDistributionData>(&value.payload))
            {
                const auto* defaultUniform = get_if<UniformColorDistributionData>(&defaultValue.payload);
                const Color defaultMin = defaultUniform != nullptr ? defaultUniform->minValue : uniform->minValue;
                const Color defaultMax = defaultUniform != nullptr ? defaultUniform->maxValue : uniform->maxValue;
                changed |= _basicDrawer.Draw_ColorRgbProperty("최소 RGB", uniform->minValue, defaultMin);
                changed |= _basicDrawer.Draw_ColorRgbProperty("최대 RGB", uniform->maxValue, defaultMax);
                Normalize_ColorUniform(*uniform);
            }
            break;
        default:
            break;
        }
        _basicDrawer.Pop_RowTint();

        ImGui::PopID();
        return changed;
    }

    static bool Is_DefaultValue(float value, float defaultValue) { return value == defaultValue; }
    static bool Is_DefaultValue(const Vec2& value, const Vec2& defaultValue) { return value.x == defaultValue.x && value.y == defaultValue.y; }

    static bool Is_DefaultValue(const Vec3& value, const Vec3& defaultValue)
    {
        return value.x == defaultValue.x && value.y == defaultValue.y && value.z == defaultValue.z;
    }

    static bool Is_DefaultValue(const Color& value, const Color& defaultValue)
    {
        return value.x == defaultValue.x && value.y == defaultValue.y && value.z == defaultValue.z && value.w == defaultValue.w;
    }

    static bool Is_DefaultValue(const ConstantFloatDistributionData& value, const ConstantFloatDistributionData& defaultValue)
    {
        return Is_DefaultValue(value.value, defaultValue.value);
    }

    static bool Is_DefaultValue(const UniformFloatDistributionData& value, const UniformFloatDistributionData& defaultValue)
    {
        return Is_DefaultValue(value.minValue, defaultValue.minValue) && Is_DefaultValue(value.maxValue, defaultValue.maxValue);
    }

    static bool Is_DefaultValue(const FloatCurveKeyData& value, const FloatCurveKeyData& defaultValue)
    {
        return Is_DefaultValue(value.time, defaultValue.time) &&
               Is_DefaultValue(value.value, defaultValue.value) &&
               Is_DefaultValue(value.arriveTangent, defaultValue.arriveTangent) &&
               Is_DefaultValue(value.leaveTangent, defaultValue.leaveTangent) &&
               value.interpolationMode == defaultValue.interpolationMode;
    }

    static bool Is_DefaultValue(const ConstantCurveFloatDistributionData& value, const ConstantCurveFloatDistributionData& defaultValue)
    {
        if (value.keys.size() != defaultValue.keys.size())
            return false;

        for (size_t index = 0; index < value.keys.size(); ++index)
        {
            if (!Is_DefaultValue(value.keys[index], defaultValue.keys[index]))
                return false;
        }

        return true;
    }

    static bool Is_DefaultValue(const FloatDistributionData& value, const FloatDistributionData& defaultValue)
    {
        if (value.mode != defaultValue.mode || value.payload.index() != defaultValue.payload.index())
            return false;

        if (const auto* current = get_if<ConstantFloatDistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<ConstantFloatDistributionData>(defaultValue.payload));
        if (const auto* current = get_if<UniformFloatDistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<UniformFloatDistributionData>(defaultValue.payload));
        if (const auto* current = get_if<ConstantCurveFloatDistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<ConstantCurveFloatDistributionData>(defaultValue.payload));
        return false;
    }

    static bool Is_DefaultValue(const ConstantVector2DistributionData& value, const ConstantVector2DistributionData& defaultValue)
    {
        return Is_DefaultValue(value.value, defaultValue.value);
    }

    static bool Is_DefaultValue(const UniformVector2DistributionData& value, const UniformVector2DistributionData& defaultValue)
    {
        return Is_DefaultValue(value.minValue, defaultValue.minValue) && Is_DefaultValue(value.maxValue, defaultValue.maxValue);
    }

    static bool Is_DefaultValue(const Vector2DistributionData& value, const Vector2DistributionData& defaultValue)
    {
        if (value.mode != defaultValue.mode || value.payload.index() != defaultValue.payload.index())
            return false;
        if (const auto* current = get_if<ConstantVector2DistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<ConstantVector2DistributionData>(defaultValue.payload));
        if (const auto* current = get_if<UniformVector2DistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<UniformVector2DistributionData>(defaultValue.payload));
        return false;
    }

    static bool Is_DefaultValue(const ConstantVector3DistributionData& value, const ConstantVector3DistributionData& defaultValue)
    {
        return Is_DefaultValue(value.value, defaultValue.value);
    }

    static bool Is_DefaultValue(const UniformVector3DistributionData& value, const UniformVector3DistributionData& defaultValue)
    {
        return Is_DefaultValue(value.minValue, defaultValue.minValue) && Is_DefaultValue(value.maxValue, defaultValue.maxValue);
    }

    static bool Is_DefaultValue(const Vector3DistributionData& value, const Vector3DistributionData& defaultValue)
    {
        if (value.mode != defaultValue.mode || value.payload.index() != defaultValue.payload.index())
            return false;
        if (const auto* current = get_if<ConstantVector3DistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<ConstantVector3DistributionData>(defaultValue.payload));
        if (const auto* current = get_if<UniformVector3DistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<UniformVector3DistributionData>(defaultValue.payload));
        return false;
    }

    static bool Is_DefaultValue(const ConstantColorDistributionData& value, const ConstantColorDistributionData& defaultValue)
    {
        return Is_DefaultValue(value.value, defaultValue.value);
    }

    static bool Is_DefaultValue(const UniformColorDistributionData& value, const UniformColorDistributionData& defaultValue)
    {
        return Is_DefaultValue(value.minValue, defaultValue.minValue) && Is_DefaultValue(value.maxValue, defaultValue.maxValue);
    }

    static bool Is_DefaultValue(const ColorDistributionData& value, const ColorDistributionData& defaultValue)
    {
        if (value.mode != defaultValue.mode || value.payload.index() != defaultValue.payload.index())
            return false;
        if (const auto* current = get_if<ConstantColorDistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<ConstantColorDistributionData>(defaultValue.payload));
        if (const auto* current = get_if<UniformColorDistributionData>(&value.payload))
            return Is_DefaultValue(*current, get<UniformColorDistributionData>(defaultValue.payload));
        return false;
    }

    static void Transition_FloatDistributionMode(FloatDistributionData& value, DistributionMode nextMode)
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
            else if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&value.payload))
                value.payload = ConstantFloatDistributionData{ curve->keys.empty() ? 0.f : curve->keys.front().value };
            else
                value.payload = ConstantFloatDistributionData{};
            break;

        case DistributionMode::Uniform:
            if (const auto* constant = get_if<ConstantFloatDistributionData>(&value.payload))
                value.payload = UniformFloatDistributionData{ constant->value, constant->value };
            else if (const auto* uniform = get_if<UniformFloatDistributionData>(&value.payload))
                value.payload = *uniform;
            else if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&value.payload))
            {
                const float minValue = curve->keys.empty() ? 0.f : curve->keys.front().value;
                const float maxValue = curve->keys.size() > 1 ? curve->keys.back().value : minValue;
                value.payload = UniformFloatDistributionData{ minValue, maxValue };
            }
            else
                value.payload = UniformFloatDistributionData{};
            break;

        case DistributionMode::ConstantCurve:
            if (const auto* constant = get_if<ConstantFloatDistributionData>(&value.payload))
                value.payload = FloatDistributionData::Make_ConstantCurve(constant->value).payload;
            else if (const auto* uniform = get_if<UniformFloatDistributionData>(&value.payload))
                value.payload = FloatDistributionData::Make_ConstantCurve(uniform->minValue).payload;
            else if (const auto* curve = get_if<ConstantCurveFloatDistributionData>(&value.payload))
                value.payload = *curve;
            else
                value.payload = FloatDistributionData::Make_ConstantCurve(0.f).payload;
            break;

        default:
            return;
        }

        value.mode = nextMode;
    }

    static void Transition_Vector2DistributionMode(Vector2DistributionData& value, DistributionMode nextMode)
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

    static void Transition_Vector3DistributionMode(Vector3DistributionData& value, DistributionMode nextMode)
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

    static void Transition_ColorDistributionMode(ColorDistributionData& value, DistributionMode nextMode)
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
};

DetailPropertyContext::DetailPropertyContext()
    : _basicDrawer{ make_unique<BasicDetailPropertyDrawer>() }
    , _distributionDrawer{ make_unique<DistributionDetailPropertyDrawer>(*_basicDrawer) }
{
}

DetailPropertyContext::~DetailPropertyContext() = default;

bool DetailPropertyContext::Begin_PropertyTable(const char* id)
{
    return _basicDrawer->Begin_PropertyTable(id);
}

void DetailPropertyContext::End_PropertyTable()
{
    _basicDrawer->End_PropertyTable();
}

void DetailPropertyContext::Draw_PropertyLabel(const char* label)
{
    _basicDrawer->Draw_PropertyLabel(label);
}

bool DetailPropertyContext::Draw_ResetButton(bool canReset)
{
    return _basicDrawer->Draw_ResetButton(canReset);
}

bool DetailPropertyContext::Draw_StringProperty(const char* label, string& value, const string& defaultValue)
{
    return _basicDrawer->Draw_StringProperty(label, value, defaultValue);
}

bool DetailPropertyContext::Draw_BoolProperty(const char* label, bool& value, bool defaultValue)
{
    return _basicDrawer->Draw_BoolProperty(label, value, defaultValue);
}

bool DetailPropertyContext::Draw_FloatProperty(const char* label, float& value, float defaultValue, float speed)
{
    return _basicDrawer->Draw_FloatProperty(label, value, defaultValue, speed);
}

bool DetailPropertyContext::Draw_ColorProperty(const char* label, Color& value, const Color& defaultValue)
{
    return _basicDrawer->Draw_ColorProperty(label, value, defaultValue);
}

bool DetailPropertyContext::Draw_FloatDistributionGroup(const char* label, FloatDistributionData& value, const FloatDistributionData& defaultValue, float speed)
{
    return _distributionDrawer->Draw_FloatDistributionGroup(label, value, defaultValue, speed);
}

bool DetailPropertyContext::Draw_Vector2DistributionGroup(
    const char* label,
    Vector2DistributionData& value,
    const Vector2DistributionData& defaultValue,
    float speed)
{
    return _distributionDrawer->Draw_Vector2DistributionGroup(label, value, defaultValue, speed);
}

bool DetailPropertyContext::Draw_Vector3DistributionGroup(
    const char* label,
    Vector3DistributionData& value,
    const Vector3DistributionData& defaultValue,
    float speed)
{
    return _distributionDrawer->Draw_Vector3DistributionGroup(label, value, defaultValue, speed);
}

bool DetailPropertyContext::Draw_ColorDistributionGroup(const char* label, ColorDistributionData& value, const ColorDistributionData& defaultValue)
{
    return _distributionDrawer->Draw_ColorDistributionGroup(label, value, defaultValue);
}

bool DetailPropertyContext::Draw_ColorRgbDistributionGroup(const char* label, ColorDistributionData& value, const ColorDistributionData& defaultValue)
{
    return _distributionDrawer->Draw_ColorRgbDistributionGroup(label, value, defaultValue);
}

bool DetailPropertyContext::Draw_BurstListProperty(
    vector<SpawnModuleData::ParticleBurstData>& burstList,
    const vector<SpawnModuleData::ParticleBurstData>& defaultValue)
{
    return _basicDrawer->Draw_BurstListProperty(burstList, defaultValue);
}

bool DetailPropertyContext::Draw_ScreenAlignmentProperty(const char* label, EmitterScreenAlignment& value, EmitterScreenAlignment defaultValue)
{
    return _basicDrawer->Draw_ScreenAlignmentProperty(label, value, defaultValue);
}

bool DetailPropertyContext::Draw_SortModeProperty(const char* label, EmitterSortMode& value, EmitterSortMode defaultValue)
{
    return _basicDrawer->Draw_SortModeProperty(label, value, defaultValue);
}

bool DetailPropertyContext::Draw_UintProperty(const char* label, uint32& value, uint32 defaultValue)
{
    return _basicDrawer->Draw_UintProperty(label, value, defaultValue);
}

bool DetailPropertyContext::Draw_Vec3Property(const char* label, Vec3& value, const Vec3& defaultValue, float speed)
{
    return _basicDrawer->Draw_Vec3Property(label, value, defaultValue, speed);
}

NS_END
