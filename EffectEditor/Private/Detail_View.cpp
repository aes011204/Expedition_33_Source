#include "Detail_View.h"

#include <cfloat>
#include <cstring>

#include "Editor_Context.h"
#include "EffectEditorInstance.h"
#include "Emitter_View.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    RequiredModuleData* Find_RequiredModuleData(AuthoringEmitter& emitter)
    {
        for (AuthoringModule& module : emitter.modules)
        {
            if (module.type != AuthoringModuleType::Required)
                continue;

            return get_if<RequiredModuleData>(&module.data);
        }

        return nullptr;
    }

    void Normalize_FloatUniform(UniformFloatDistributionData& data)
    {
        if (data.minValue <= data.maxValue)
            return;

        const float previousMin = data.minValue;
        data.minValue = data.maxValue;
        data.maxValue = previousMin;
    }

    void Normalize_Vector2Uniform(UniformVector2DistributionData& data)
    {
        if (data.minValue.x > data.maxValue.x)
        {
            const float previousMin = data.minValue.x;
            data.minValue.x = data.maxValue.x;
            data.maxValue.x = previousMin;
        }

        if (data.minValue.y > data.maxValue.y)
        {
            const float previousMin = data.minValue.y;
            data.minValue.y = data.maxValue.y;
            data.maxValue.y = previousMin;
        }
    }

    void Normalize_Vector3Uniform(UniformVector3DistributionData& data)
    {
        if (data.minValue.x > data.maxValue.x)
        {
            const float previousMin = data.minValue.x;
            data.minValue.x = data.maxValue.x;
            data.maxValue.x = previousMin;
        }

        if (data.minValue.y > data.maxValue.y)
        {
            const float previousMin = data.minValue.y;
            data.minValue.y = data.maxValue.y;
            data.maxValue.y = previousMin;
        }

        if (data.minValue.z > data.maxValue.z)
        {
            const float previousMin = data.minValue.z;
            data.minValue.z = data.maxValue.z;
            data.maxValue.z = previousMin;
        }
    }

    void Transition_FloatDistributionMode(FloatDistributionData& value, DistributionMode nextMode)
    {
        if (value.mode == nextMode || nextMode == DistributionMode::ConstantCurveReserved)
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

    void Transition_Vector2DistributionMode(Vector2DistributionData& value, DistributionMode nextMode)
    {
        if (value.mode == nextMode || nextMode == DistributionMode::ConstantCurveReserved)
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

    void Transition_Vector3DistributionMode(Vector3DistributionData& value, DistributionMode nextMode)
    {
        if (value.mode == nextMode || nextMode == DistributionMode::ConstantCurveReserved)
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
}

Detail_View::Detail_View()
    : Editor_Window{ L"Detail", ICON_FA_PEN_TO_SQUARE }
{
}

Detail_View::~Detail_View()
{
    Free();
}

void Detail_View::Update(float)
{
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

    if (selection.kind == EffectAuthoringSelectionKind::None)
    {
        Render_ParticleSystemDetail(emitterView->Get_ParticleSystemData());
        return;
    }

    AuthoringEmitter* emitter = emitterView->Find_Emitter(selection.emitterId);
    if (nullptr == emitter)
    {
        ImGui::TextDisabled("Selected emitter is not available.");
        return;
    }

    if (selection.kind == EffectAuthoringSelectionKind::Emitter)
    {
        Render_EmitterDetail(*emitter);
        return;
    }

    AuthoringModule* module = emitterView->Find_Module(selection.emitterId, selection.moduleId);
    if (nullptr == module)
    {
        ImGui::TextDisabled("Selected module is not available.");
        return;
    }

    Render_ModuleDetail(*emitter, *module);
}

void Detail_View::Render_ParticleSystemDetail(ParticleSystemAuthoringData& data)
{
    ImGui::TextUnformatted("파티클 시스템");
    ImGui::Separator();

    bool changed = false;
    if (Begin_PropertyTable("ParticleSystem"))
    {
        changed |= Draw_BoolProperty("자동 비활성화", data.autoDeactivate);
        End_PropertyTable();
    }
    if (changed)
        MarkDirty();
}

void Detail_View::Render_EmitterDetail(AuthoringEmitter& emitter)
{
    ImGui::TextUnformatted("파티클");
    ImGui::Separator();

    const RequiredModuleData* requiredData = Internal::Find_RequiredModuleData(emitter);
    bool changed = false;

    if (Begin_PropertyTable("Emitter"))
    {
        changed |= Draw_StringProperty("이미터 이름", emitter.name);

        ImGui::PushID("RendererType");
        Draw_PropertyLabel("렌더러 타입");
        ImGui::TextDisabled("%s", requiredData != nullptr ? requiredData->rendererType.c_str() : emitter.rendererType.c_str());
        ImGui::PopID();

        End_PropertyTable();
    }

    Mark_Changed(emitter, changed);
}

void Detail_View::Render_ModuleDetail(AuthoringEmitter& emitter, AuthoringModule& module)
{
    ImGui::TextUnformatted(String::ToString(module.displayName).c_str());
    ImGui::Separator();

    switch (module.type)
    {
    case AuthoringModuleType::Required:
        if (auto* data = get_if<RequiredModuleData>(&module.data))
            Render_RequiredDetail(emitter, module, *data);
        break;
    case AuthoringModuleType::Spawn:
        if (auto* data = get_if<SpawnModuleData>(&module.data))
            Render_SpawnDetail(emitter, module, *data);
        break;
    case AuthoringModuleType::Lifetime:
        if (auto* data = get_if<LifetimeModuleData>(&module.data))
            Render_LifetimeDetail(emitter, module, *data);
        break;
    case AuthoringModuleType::InitialSize:
        if (auto* data = get_if<InitialSizeModuleData>(&module.data))
            Render_InitialSizeDetail(emitter, module, *data);
        break;
    case AuthoringModuleType::InitialVelocity:
        if (auto* data = get_if<InitialVelocityModuleData>(&module.data))
            Render_InitialVelocityDetail(emitter, module, *data);
        break;
    case AuthoringModuleType::ColorOverLife:
        if (auto* data = get_if<ColorOverLifeModuleData>(&module.data))
            Render_ColorOverLifeDetail(emitter, module, *data);
        break;
    default:
        ImGui::TextDisabled("Unsupported module type.");
        break;
    }
}

void Detail_View::Render_RequiredDetail(AuthoringEmitter& emitter, AuthoringModule&, RequiredModuleData& data)
{
    bool changed = false;

    if (ImGui::CollapsingHeader("리소스", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (Begin_PropertyTable("RequiredResource"))
        {
            changed |= Draw_StringProperty("텍스처 ID (임시)", data.textureId);
            changed |= Draw_StringProperty("머티리얼", data.materialName);
            End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("이미터 좌표", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (Begin_PropertyTable("RequiredTransform"))
        {
            changed |= Draw_Vec3Property("원점", data.emitterOrigin);
            changed |= Draw_Vec3Property("회전 (도)", data.emitterRotationDegrees);
            End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("공간 / 정렬", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (Begin_PropertyTable("RequiredSpaceSort"))
        {
            changed |= Draw_ScreenAlignmentProperty("화면 정렬", data.screenAlignment);
            changed |= Draw_FloatProperty("카메라 블렌드 거리 최소", data.minCameraBlendDistance);
            changed |= Draw_FloatProperty("카메라 블렌드 거리 최대", data.maxCameraBlendDistance);
            changed |= Draw_BoolProperty("로컬 스페이스 사용", data.useLocalSpace);
            changed |= Draw_SortModeProperty("정렬 모드", data.sortMode);
            End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("시간", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (Begin_PropertyTable("RequiredTime"))
        {
            changed |= Draw_UintProperty("루프 횟수", data.loopCount);
            changed |= Draw_FloatProperty("지속 시간", data.duration);
            changed |= Draw_FloatProperty("지속 시간 최소값", data.durationLow);
            changed |= Draw_BoolProperty("지속 시간 범위 사용", data.useDurationRange);
            changed |= Draw_BoolProperty("루프마다 기간 재계산", data.recalculateDurationEachLoop);
            changed |= Draw_FloatProperty("딜레이", data.delay);
            changed |= Draw_FloatProperty("딜레이 최소값", data.delayLow);
            changed |= Draw_BoolProperty("딜레이 범위 사용", data.useDelayRange);
            changed |= Draw_BoolProperty("첫 루프만 딜레이", data.delayFirstLoopOnly);
            End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("종료 / 킬", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (Begin_PropertyTable("RequiredKill"))
        {
            changed |= Draw_BoolProperty("비활성화 시 킬", data.killOnDeactivate);
            changed |= Draw_BoolProperty("완료 시 킬", data.killOnCompleted);
            End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("렌더링 제한", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (Begin_PropertyTable("RequiredRenderLimit"))
        {
            changed |= Draw_BoolProperty("최대 드로우 수 사용", data.useMaxDrawCount);
            changed |= Draw_UintProperty("최대 드로우 수", data.maxDrawCount);
            End_PropertyTable();
        }
    }

    if (changed)
    {
        emitter.rendererType = data.rendererType;
        emitter.textureId = data.textureId;
        emitter.materialName = data.materialName;
        emitter.resourceSummary = data.textureId;
    }

    Mark_Changed(emitter, changed);
}

void Detail_View::Render_SpawnDetail(AuthoringEmitter& emitter, AuthoringModule&, SpawnModuleData& data)
{
    bool changed = false;

    if (ImGui::CollapsingHeader("연속 스폰", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (Begin_PropertyTable("SpawnRate"))
        {
            changed |= Draw_BoolProperty("연속 스폰 사용", data.processSpawnRate);
            changed |= Draw_FloatDistributionProperty("초당 생성 수", data.spawnRate);
            changed |= Draw_FloatDistributionProperty("스폰 속도 스케일", data.spawnRateScale);
            End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("버스트", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (Begin_PropertyTable("SpawnBurst"))
        {
            changed |= Draw_BoolProperty("버스트 사용", data.processBurstList);
            changed |= Draw_FloatDistributionProperty("버스트 스케일", data.burstScale);
            End_PropertyTable();
        }

        changed |= Draw_BurstListProperty(data.burstList);
    }

    if (ImGui::CollapsingHeader("제한", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (Begin_PropertyTable("SpawnLimit"))
        {
            changed |= Draw_UintProperty("최대 활성 파티클 수", data.maxParticleCount);
            End_PropertyTable();
        }
    }

    Mark_Changed(emitter, changed);
}

void Detail_View::Render_LifetimeDetail(AuthoringEmitter& emitter, AuthoringModule&, LifetimeModuleData& data)
{
    bool changed = false;

    if (ImGui::CollapsingHeader("수명", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (Begin_PropertyTable("Lifetime"))
        {
            changed |= Draw_FloatDistributionProperty("분포", data.lifeTime);
            End_PropertyTable();
        }
    }

    Mark_Changed(emitter, changed);
}

void Detail_View::Render_InitialSizeDetail(AuthoringEmitter& emitter, AuthoringModule&, InitialSizeModuleData& data)
{
    bool changed = false;
    if (Begin_PropertyTable("InitialSize"))
    {
        changed |= Draw_Vector2DistributionProperty("크기", data.size);
        End_PropertyTable();
    }
    Mark_Changed(emitter, changed);
}

void Detail_View::Render_InitialVelocityDetail(AuthoringEmitter& emitter, AuthoringModule&, InitialVelocityModuleData& data)
{
    bool changed = false;
    if (Begin_PropertyTable("InitialVelocity"))
    {
        changed |= Draw_Vec3Property("방사 기준점", data.radialPivot);
        changed |= Draw_FloatDistributionProperty("속도", data.speed);
        End_PropertyTable();
    }
    Mark_Changed(emitter, changed);
}

void Detail_View::Render_ColorOverLifeDetail(AuthoringEmitter& emitter, AuthoringModule&, ColorOverLifeModuleData& data)
{
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 색상", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (Begin_PropertyTable("ColorOverLife"))
        {
            changed |= Draw_Vector3DistributionProperty("색상", data.colorOverLife);
            End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("수명에 따른 알파", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (Begin_PropertyTable("AlphaOverLife"))
        {
            changed |= Draw_FloatDistributionProperty("알파", data.alphaOverLife);
            End_PropertyTable();
        }
    }

    Mark_Changed(emitter, changed);
}

bool Detail_View::Begin_PropertyTable(const char* id)
{
    constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
    if (!ImGui::BeginTable(id, 2, tableFlags))
        return false;

    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 0.4f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.6f);
    return true;
}

void Detail_View::End_PropertyTable()
{
    ImGui::EndTable();
}

void Detail_View::Draw_PropertyLabel(const char* label)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
}

bool Detail_View::Draw_StringProperty(const char* label, string& value)
{
    char buffer[256]{};
    const size_t copyLength = min(value.size(), sizeof(buffer) - 1);
    memcpy(buffer, value.c_str(), copyLength);

    ImGui::PushID(label);
    Draw_PropertyLabel(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool changed = ImGui::InputText("##Value", buffer, sizeof(buffer));
    ImGui::PopID();

    if (changed)
        value = buffer;

    return changed;
}

bool Detail_View::Draw_BoolProperty(const char* label, bool& value)
{
    ImGui::PushID(label);
    Draw_PropertyLabel(label);
    const bool changed = ImGui::Checkbox("##Value", &value);
    ImGui::PopID();
    return changed;
}

bool Detail_View::Draw_FloatProperty(const char* label, float& value, float speed)
{
    ImGui::PushID(label);
    Draw_PropertyLabel(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool changed = ImGui::DragFloat("##Value", &value, speed);
    ImGui::PopID();
    return changed;
}

bool Detail_View::Draw_FloatDistributionProperty(const char* label, FloatDistributionData& value, float speed)
{
    bool changed = false;

    ImGui::PushID(label);
    DistributionMode nextMode = value.mode;
    if (Draw_DistributionModeProperty(label, nextMode))
    {
        Internal::Transition_FloatDistributionMode(value, nextMode);
        changed = true;
    }

    switch (value.mode)
    {
    case DistributionMode::Constant:
        if (auto* constant = get_if<ConstantFloatDistributionData>(&value.payload))
            changed |= Draw_FloatProperty("상수", constant->value, speed);
        break;

    case DistributionMode::Uniform:
        if (auto* uniform = get_if<UniformFloatDistributionData>(&value.payload))
        {
            changed |= Draw_FloatProperty("최소", uniform->minValue, speed);
            changed |= Draw_FloatProperty("최대", uniform->maxValue, speed);
            Internal::Normalize_FloatUniform(*uniform);
        }
        break;

    case DistributionMode::ConstantCurveReserved:
        Draw_PropertyLabel("상수 커브");
        ImGui::TextDisabled("후속 지원 예정");
        break;

    default:
        break;
    }

    ImGui::PopID();
    return changed;
}

bool Detail_View::Draw_Vector2DistributionProperty(const char* label, Vector2DistributionData& value, float speed)
{
    bool changed = false;

    ImGui::PushID(label);
    DistributionMode nextMode = value.mode;
    if (Draw_DistributionModeProperty(label, nextMode))
    {
        Internal::Transition_Vector2DistributionMode(value, nextMode);
        changed = true;
    }

    switch (value.mode)
    {
    case DistributionMode::Constant:
        if (auto* constant = get_if<ConstantVector2DistributionData>(&value.payload))
            changed |= Draw_Vec2Property("상수", constant->value, speed);
        break;

    case DistributionMode::Uniform:
        if (auto* uniform = get_if<UniformVector2DistributionData>(&value.payload))
        {
            changed |= Draw_Vec2Property("최소", uniform->minValue, speed);
            changed |= Draw_Vec2Property("최대", uniform->maxValue, speed);
            Internal::Normalize_Vector2Uniform(*uniform);
        }
        break;

    case DistributionMode::ConstantCurveReserved:
        Draw_PropertyLabel("상수 커브");
        ImGui::TextDisabled("후속 지원 예정");
        break;

    default:
        break;
    }

    ImGui::PopID();
    return changed;
}

bool Detail_View::Draw_Vector3DistributionProperty(const char* label, Vector3DistributionData& value, float speed)
{
    bool changed = false;

    ImGui::PushID(label);
    DistributionMode nextMode = value.mode;
    if (Draw_DistributionModeProperty(label, nextMode))
    {
        Internal::Transition_Vector3DistributionMode(value, nextMode);
        changed = true;
    }

    switch (value.mode)
    {
    case DistributionMode::Constant:
        if (auto* constant = get_if<ConstantVector3DistributionData>(&value.payload))
            changed |= Draw_Vec3Property("상수", constant->value, speed);
        break;

    case DistributionMode::Uniform:
        if (auto* uniform = get_if<UniformVector3DistributionData>(&value.payload))
        {
            changed |= Draw_Vec3Property("최소", uniform->minValue, speed);
            changed |= Draw_Vec3Property("최대", uniform->maxValue, speed);
            Internal::Normalize_Vector3Uniform(*uniform);
        }
        break;

    case DistributionMode::ConstantCurveReserved:
        Draw_PropertyLabel("상수 커브");
        ImGui::TextDisabled("후속 지원 예정");
        break;

    default:
        break;
    }

    ImGui::PopID();
    return changed;
}

bool Detail_View::Draw_BurstListProperty(vector<SpawnModuleData::ParticleBurstData>& burstList)
{
    bool changed = false;

    ImGui::TextUnformatted("버스트 목록");
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_PLUS "##AddBurst"))
    {
        burstList.push_back(SpawnModuleData::ParticleBurstData{});
        changed = true;
    }

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
                    entryChanged |= Draw_BurstEntryProperty(burstList[index]);
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

bool Detail_View::Draw_BurstEntryProperty(SpawnModuleData::ParticleBurstData& burst)
{
    bool changed = false;
    changed |= Draw_FloatProperty("시간", burst.time);
    changed |= Draw_UintProperty("개수", burst.count);
    return changed;
}

bool Detail_View::Draw_ScreenAlignmentProperty(const char* label, EmitterScreenAlignment& value)
{
    int currentIndex = 0;
    switch (value)
    {
    case EmitterScreenAlignment::Square:
        currentIndex = 0;
        break;
    case EmitterScreenAlignment::CameraFacing:
        currentIndex = 1;
        break;
    case EmitterScreenAlignment::Velocity:
        currentIndex = 2;
        break;
    default:
        currentIndex = 0;
        break;
    }

    static constexpr const char* kScreenAlignmentLabels[] = { "정사각형", "카메라 방향", "속도 방향" };

    ImGui::PushID(label);
    Draw_PropertyLabel(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool changed = ImGui::Combo("##Value", &currentIndex, kScreenAlignmentLabels, IM_ARRAYSIZE(kScreenAlignmentLabels));
    ImGui::PopID();

    if (!changed)
        return false;

    switch (currentIndex)
    {
    case 0:
        value = EmitterScreenAlignment::Square;
        break;
    case 1:
        value = EmitterScreenAlignment::CameraFacing;
        break;
    case 2:
        value = EmitterScreenAlignment::Velocity;
        break;
    default:
        value = EmitterScreenAlignment::Square;
        break;
    }

    return true;
}

bool Detail_View::Draw_SortModeProperty(const char* label, EmitterSortMode& value)
{
    int currentIndex = 0;
    switch (value)
    {
    case EmitterSortMode::None:
        currentIndex = 0;
        break;
    case EmitterSortMode::ViewDepth:
        currentIndex = 1;
        break;
    case EmitterSortMode::Age:
        currentIndex = 2;
        break;
    default:
        currentIndex = 0;
        break;
    }

    static constexpr const char* kSortModeLabels[] = { "없음", "카메라 깊이", "수명 진행" };

    ImGui::PushID(label);
    Draw_PropertyLabel(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool changed = ImGui::Combo("##Value", &currentIndex, kSortModeLabels, IM_ARRAYSIZE(kSortModeLabels));
    ImGui::PopID();

    if (!changed)
        return false;

    switch (currentIndex)
    {
    case 0:
        value = EmitterSortMode::None;
        break;
    case 1:
        value = EmitterSortMode::ViewDepth;
        break;
    case 2:
        value = EmitterSortMode::Age;
        break;
    default:
        value = EmitterSortMode::None;
        break;
    }

    return true;
}

bool Detail_View::Draw_UintProperty(const char* label, uint32& value)
{
    int intValue = static_cast<int>(value);

    ImGui::PushID(label);
    Draw_PropertyLabel(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool changed = ImGui::DragInt("##Value", &intValue, 1.f, 0, 100000);
    ImGui::PopID();

    if (changed)
        value = static_cast<uint32>(max(0, intValue));

    return changed;
}

bool Detail_View::Draw_DistributionModeProperty(const char* label, DistributionMode& value)
{
    ImGui::PushID(label);
    Draw_PropertyLabel(label);
    bool changed = false;

    auto getCurrentLabel = [](DistributionMode mode) -> const char*
    {
        switch (mode)
        {
        case DistributionMode::Constant:
            return "상수";
        case DistributionMode::Uniform:
            return "유니폼 범위";
        case DistributionMode::ConstantCurveReserved:
            return "상수 커브 (예정)";
        default:
            return "상수";
        }
    };

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##Value", getCurrentLabel(value)))
    {
        auto drawSelectable = [&](DistributionMode mode, bool enabled)
        {
            if (!enabled)
                ImGui::BeginDisabled();

            const bool isSelected = value == mode;
            if (ImGui::Selectable(getCurrentLabel(mode), isSelected) && enabled)
            {
                value = mode;
                changed = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();

            if (!enabled)
                ImGui::EndDisabled();
        };

        drawSelectable(DistributionMode::Constant, true);
        drawSelectable(DistributionMode::Uniform, true);
        drawSelectable(DistributionMode::ConstantCurveReserved, false);
        ImGui::EndCombo();
    }

    ImGui::PopID();
    return changed;
}

bool Detail_View::Draw_Vec2Property(const char* label, Vec2& value, float speed)
{
    ImGui::PushID(label);
    Draw_PropertyLabel(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool changed = ImGui::DragFloat2("##Value", &value.x, speed);
    ImGui::PopID();
    return changed;
}

bool Detail_View::Draw_Vec3Property(const char* label, Vec3& value, float speed)
{
    ImGui::PushID(label);
    Draw_PropertyLabel(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool changed = ImGui::DragFloat3("##Value", &value.x, speed);
    ImGui::PopID();
    return changed;
}

bool Detail_View::Draw_ColorProperty(const char* label, Color& value)
{
    ImGui::PushID(label);
    Draw_PropertyLabel(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool changed = ImGui::ColorEdit4(
        "##Value",
        &value.x,
        ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar
    );
    ImGui::PopID();
    return changed;
}

void Detail_View::Mark_Changed(AuthoringEmitter& emitter, bool changed)
{
    if (!changed)
        return;

    emitter.previewDirty = true;
    MarkDirty();
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
