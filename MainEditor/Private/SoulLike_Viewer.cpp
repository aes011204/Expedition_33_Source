#include "pch.h"
#include "SoulLike_Viewer.h"

#include "CombatStatCom.h"
#include "ContainerObject.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "ModelCom.h"
#include "PartObject.h"
#include "SL_Base.h"
#include "SL_StateMachineCom.h"
#include "TransformCom.h"
#include "TypeRegistry.h"

#include <algorithm>
#include <cctype>

NS_BEGIN(MainEditor)

namespace
{
    string ToLowerCopy(string value)
    {
        transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
        {
            return static_cast<char>(tolower(ch));
        });
        return value;
    }

    bool ContainsInsensitive(const string& value, const string& filter)
    {
        if (filter.empty())
            return true;

        return ToLowerCopy(value).find(ToLowerCopy(filter)) != string::npos;
    }

    string FormatAnimStateIndex(uint32 stateIndex)
    {
        if (stateIndex == UINT32_MAX)
            return "-";

        return "State #" + to_string(stateIndex);
    }
}

SoulLike_Viewer::SoulLike_Viewer()
    : Editor_Window(L"SoulLike_Viewer")
{
}

HRESULT SoulLike_Viewer::Initialize()
{
    Refresh_Entries();
    return S_OK;
}

void SoulLike_Viewer::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (_autoRefresh)
        Refresh_Entries();
}

void SoulLike_Viewer::Pre_Render()
{
}

void SoulLike_Viewer::Render()
{
    if (!_isOpen)
        return;

    if (!ImGui::Begin("SoulLike_Viewer", &_isOpen))
    {
        ImGui::End();
        return;
    }

    _isVisible = true;
    _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

    Draw_Header();
    ImGui::Separator();

    const float detailHeight = 150.f;
    ImGui::BeginChild("SoulLikeRuntimeTable", ImVec2(0.f, -detailHeight), true);
    Draw_Table();
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::BeginChild("SoulLikeRuntimeDetail", ImVec2(0.f, 0.f), true);
    if (_selectedIndex >= 0 && _selectedIndex < static_cast<int32>(_entries.size()))
        Draw_Detail(_entries[_selectedIndex]);
    else
        ImGui::TextDisabled("Select an SL actor.");
    ImGui::EndChild();

    ImGui::End();
}

void SoulLike_Viewer::Refresh_Entries()
{
    _entries.clear();

    GAME->Visit_CurrentLevelObjects(
        [this](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            Shared<Client::SL_Base> slActor = dynamic_pointer_cast<Client::SL_Base>(gameObject);
            if (slActor == nullptr)
                return;

            RuntimeEntry entry{};
            entry.actor = slActor;
            entry.layerTag = layerTag;
            entry.objectName = String::ToString(slActor->Get_Name());
            entry.className = Resolve_ClassName(slActor);
            entry.sourcePrefab = slActor->Get_SourcePrefabName();
            entry.visible = slActor->Is_Visible();
            entry.markedDestroy = slActor->Is_MarkDestroy();

            if (Shared<TransformCom> transform = slActor->Get_Transform())
                entry.position = transform->Get_WorldPosition();

            if (Shared<Client::CombatStatCom> stat = slActor->Get_Stat())
            {
                entry.hasStat = true;
                auto& statInfo = stat->Get_StatInfo();
                auto& runtimeStat = stat->Get_RuntimeStat();

                entry.curHp = stat->Get_CurHp();
                entry.maxHp = statInfo.maxHp;
                entry.curAp = stat->Get_CurAP();
                entry.maxAp = stat->Get_MaxAP();
                entry.shield = stat->Get_CurrentShield();
                entry.speed = stat->Get_Speed();
                entry.aliveState = string(magic_enum::enum_name(runtimeStat.CurAliveState));
            }

            entry.stateName = Resolve_StateName(slActor);

            if (Shared<ModelCom> model = Find_PrimaryModel(slActor))
            {
                entry.animationStateIndex = model->Get_AnimationStateIndex();
                entry.animationStateName = Resolve_ModelStateName(model);
                entry.animationTimeSec = model->Get_CurrentAnimationTimeSec();
                entry.animationDurationSec = model->Get_CurrentAnimationDurationSec();

                if (model->Get_NumAnimations() > 0)
                {
                    const char* animationName = model->Get_AnimationName(model->Get_CurrentAnimationIndex());
                    entry.animationClipName = animationName != nullptr ? animationName : "";
                }
            }

            _entries.push_back(entry);
        });

    if (_selectedIndex >= static_cast<int32>(_entries.size()))
        _selectedIndex = static_cast<int32>(_entries.size()) - 1;
}

void SoulLike_Viewer::Draw_Header()
{
    const int32 totalCount = static_cast<int32>(_entries.size());
    int32 aliveCount = 0;
    for (const RuntimeEntry& entry : _entries)
    {
        if (entry.aliveState != "Dead")
            ++aliveCount;
    }

    ImGui::Text("SL Actors: %d", totalCount);
    ImGui::SameLine();
    ImGui::TextDisabled("Alive: %d", aliveCount);

    ImGui::SameLine(0.f, 16.f);
    ImGui::Checkbox("Auto", &_autoRefresh);
    ImGui::SameLine();
    if (ImGui::Button("Refresh"))
        Refresh_Entries();

    ImGui::SameLine(0.f, 16.f);
    ImGui::Checkbox("Show Dead", &_showDead);

    ImGui::SetNextItemWidth(220.f);
    ImGui::InputTextWithHint("##SoulLikeViewerSearch", "Search name/class/state", _searchBuffer, IM_ARRAYSIZE(_searchBuffer));
}

void SoulLike_Viewer::Draw_Table()
{
    if (!ImGui::BeginTable(
        "SoulLikeRuntimeActorTable",
        8,
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY))
    {
        return;
    }

    ImGui::TableSetupColumn("Actor", ImGuiTableColumnFlags_WidthFixed, 170.f);
    ImGui::TableSetupColumn("Class", ImGuiTableColumnFlags_WidthFixed, 145.f);
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 110.f);
    ImGui::TableSetupColumn("Anim State", ImGuiTableColumnFlags_WidthFixed, 120.f);
    ImGui::TableSetupColumn("HP", ImGuiTableColumnFlags_WidthFixed, 120.f);
    ImGui::TableSetupColumn("Alive", ImGuiTableColumnFlags_WidthFixed, 70.f);
    ImGui::TableSetupColumn("Layer", ImGuiTableColumnFlags_WidthFixed, 120.f);
    ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    for (int32 i = 0; i < static_cast<int32>(_entries.size()); ++i)
    {
        const RuntimeEntry& entry = _entries[i];
        if (!Passes_Filter(entry))
            continue;

        ImGui::TableNextRow();
        ImGui::PushID(i);

        ImGui::TableSetColumnIndex(0);
        const bool selected = (_selectedIndex == i);
        if (ImGui::Selectable(entry.objectName.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
            _selectedIndex = i;

        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(entry.className.c_str());

        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(entry.stateName.empty() ? "-" : entry.stateName.c_str());

        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted(entry.animationStateName.empty() ? "-" : entry.animationStateName.c_str());

        ImGui::TableSetColumnIndex(4);
        if (entry.hasStat)
            ImGui::Text("%.0f / %.0f", entry.curHp, max(entry.maxHp, 0.f));
        else
            ImGui::TextDisabled("-");

        ImGui::TableSetColumnIndex(5);
        ImGui::TextUnformatted(entry.aliveState.empty() ? "-" : entry.aliveState.c_str());

        ImGui::TableSetColumnIndex(6);
        ImGui::TextUnformatted(String::ToString(entry.layerTag).c_str());

        ImGui::TableSetColumnIndex(7);
        ImGui::Text("(%.2f, %.2f, %.2f)", entry.position.x, entry.position.y, entry.position.z);

        ImGui::PopID();
    }

    ImGui::EndTable();
}

void SoulLike_Viewer::Draw_Detail(const RuntimeEntry& entry)
{
    ImGui::Text("%s", entry.objectName.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s", entry.className.c_str());

    if (!entry.sourcePrefab.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("Prefab: %s", entry.sourcePrefab.c_str());
    }

    ImGui::Separator();

    if (ImGui::BeginTable("SoulLikeRuntimeDetailTable", 2, ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 120.f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        auto DrawRow = [](const char* name, const char* format, auto&&... args)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("%s", name);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text(format, std::forward<decltype(args)>(args)...);
        };

        DrawRow("Layer", "%s", String::ToString(entry.layerTag).c_str());
        DrawRow("State", "%s", entry.stateName.empty() ? "-" : entry.stateName.c_str());
        DrawRow("Anim State", "%s", entry.animationStateName.empty() ? "-" : entry.animationStateName.c_str());
        DrawRow("Anim Clip", "%s", entry.animationClipName.empty() ? "-" : entry.animationClipName.c_str());
        DrawRow("Anim Time", "%.3f / %.3f", entry.animationTimeSec, entry.animationDurationSec);
        DrawRow("HP", "%.0f / %.0f", entry.curHp, entry.maxHp);
        DrawRow("AP", "%u / %u", entry.curAp, entry.maxAp);
        DrawRow("Shield", "%u", entry.shield);
        DrawRow("Speed", "%.2f", entry.speed);
        DrawRow("Flags", "Visible=%s  Destroy=%s", entry.visible ? "true" : "false", entry.markedDestroy ? "true" : "false");

        ImGui::EndTable();
    }
}

bool SoulLike_Viewer::Passes_Filter(const RuntimeEntry& entry) const
{
    if (!_showDead && entry.aliveState == "Dead")
        return false;

    const string filter = _searchBuffer;
    if (filter.empty())
        return true;

    return ContainsInsensitive(entry.objectName, filter) ||
        ContainsInsensitive(entry.className, filter) ||
        ContainsInsensitive(entry.stateName, filter) ||
        ContainsInsensitive(entry.animationStateName, filter) ||
        ContainsInsensitive(entry.animationClipName, filter) ||
        ContainsInsensitive(String::ToString(entry.layerTag), filter);
}

Shared<ModelCom> SoulLike_Viewer::Find_PrimaryModel(const Shared<Client::SL_Base>& actor)
{
    if (actor == nullptr)
        return nullptr;

    if (Shared<ModelCom> directModel = actor->Get_Component<ModelCom>())
        return directModel;

    Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(actor);
    if (container == nullptr)
        return nullptr;

    if (PartObject* bodyPart = container->Get_PartObject(L"Part_Body"))
    {
        if (Shared<ModelCom> bodyModel = bodyPart->Get_Component<ModelCom>())
            return bodyModel;
    }

    for (const auto& [partTag, partObject] : container->Get_PartObjects())
    {
        UNREFERENCED_PARAMETER(partTag);

        if (partObject == nullptr)
            continue;

        if (Shared<ModelCom> model = partObject->Get_Component<ModelCom>())
            return model;
    }

    return nullptr;
}

string SoulLike_Viewer::Resolve_ClassName(const Shared<Client::SL_Base>& actor)
{
    if (actor == nullptr)
        return "";

    const ClassReflectionInfo& info = actor->Get_ReflectionInfo();
    if (!info.displayName.empty())
        return info.displayName;

    if (!info.className.empty())
        return info.className;

    return String::ToString(actor->Get_Name());
}

string SoulLike_Viewer::Resolve_StateName(const Shared<Client::SL_Base>& actor)
{
    if (actor == nullptr)
        return "";

    Shared<Client::SL_StateMachineCom> stateMachine = actor->Get_Component<Client::SL_StateMachineCom>();
    if (stateMachine == nullptr)
        return "-";

    const Client::SL_PlayerState state = stateMachine->Get_CurrentSoulLikeState();
    if (state == Client::SL_PlayerState::End)
        return "-";

    return string(magic_enum::enum_name(state));
}

string SoulLike_Viewer::Resolve_ModelStateName(const Shared<ModelCom>& model)
{
    if (model == nullptr)
        return "";

    const uint32 stateIndex = model->Get_AnimationStateIndex();
    if (stateIndex == UINT32_MAX)
        return "-";

    const string enumType = String::ToString(model->Get_AnimEnumType());
    if (!enumType.empty())
    {
        string stateName{};
        GAME->Visit_GlobalEnums([&](const TypeRegistry::GlobalEnumInfo& info)
        {
            if (!stateName.empty() || info.displayName != enumType)
                return;

            for (size_t i = 0; i < info.enumValues.size() && i < info.enumNames.size(); ++i)
            {
                if (info.enumValues[i] == static_cast<int>(stateIndex))
                {
                    stateName = info.enumNames[i];
                    return;
                }
            }
        });

        if (!stateName.empty())
            return stateName;
    }

    return FormatAnimStateIndex(stateIndex);
}

Shared<SoulLike_Viewer> SoulLike_Viewer::Create()
{
    auto instance = make_shared<SoulLike_Viewer>();

    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}

NS_END
