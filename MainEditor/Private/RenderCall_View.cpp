#include "RenderCall_View.h"

#include "GameInstance.h"
#include "Helper_String.h"

#include <filesystem>

NS_BEGIN(MainEditor)

namespace
{
    string Build_SourceModelLabel(const wstring& sourceModelPath)
    {
        if (sourceModelPath.empty())
            return {};

        return std::filesystem::path(sourceModelPath).stem().string();
    }

    string Build_RowName(const Engine::RenderViewHotspotEntry& entry)
    {
        if (!entry.objectName.empty())
            return String::ToString(entry.objectName);

        if (!entry.sourceModelPath.empty())
            return Build_SourceModelLabel(entry.sourceModelPath);

        if (!entry.className.empty())
            return entry.className;

        return "<unnamed>";
    }
}

RenderCall_View::RenderCall_View()
    : Editor_Window(L"RenderCall")
{
}

void RenderCall_View::Render()
{
    if (!Is_Open())
    {
        if (GAME->Is_RenderViewHotspotCaptureEnabled())
            GAME->Set_RenderViewHotspotCaptureEnabled(false);
        return;
    }

    bool isOpen = Is_Open();
    if (ImGui::Begin("RenderCall", &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        bool capture = GAME->Is_RenderViewHotspotCaptureEnabled();
        if (ImGui::Checkbox("Capture", &capture))
            GAME->Set_RenderViewHotspotCaptureEnabled(capture);

        if (!capture)
        {
            ImGui::TextUnformatted("Render hotspot capture is disabled.");
            Set_Open(isOpen);
            ImGui::End();
            return;
        }

        Engine::RenderViewHotspotSnapshot snapshot{};
        if (!GAME->Try_GetRenderViewHotspotSnapshot(snapshot))
        {
            ImGui::TextUnformatted("Render hotspot snapshot unavailable.");
        }
        else
        {
            ImGui::Text(
                "Total Submissions: %u | Unique Objects: %u",
                snapshot.totalSubmissions,
                snapshot.uniqueObjectCount);
            ImGui::Separator();

            Draw_GroupSection("NonBlend", snapshot, Engine::RenderGroup::NonBlend);
            ImGui::Separator();
            Draw_GroupSection("Shadow", snapshot, Engine::RenderGroup::Shadow);
        }
    }

    Set_Open(isOpen);
    ImGui::End();
}

void RenderCall_View::Draw_GroupSection(
    const char* header,
    const Engine::RenderViewHotspotSnapshot& snapshot,
    Engine::RenderGroup renderGroup)
{
    vector<Aggregated_Row> rows = Build_GroupRows(snapshot, renderGroup);
    const uint32 submissionCount = snapshot.renderGroupCounts[ETOI(renderGroup)];

    if (!ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Text("Submissions: %u | Objects: %zu", submissionCount, rows.size());

    if (rows.empty())
    {
        ImGui::TextUnformatted("No entries.");
        return;
    }

    if (ImGui::BeginTable(header, 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Cnt", ImGuiTableColumnFlags_WidthFixed, 72.f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 88.f);
        ImGui::TableSetupColumn("Submits", ImGuiTableColumnFlags_WidthFixed, 72.f);
        ImGui::TableSetupColumn("Source");
        ImGui::TableHeadersRow();

        for (const Aggregated_Row& row : rows)
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s : %u", row.name.c_str(), row.count);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", row.count);

            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(row.isInstanced ? "Instance" : "Static");

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%u", row.submissionCount);

            ImGui::TableSetColumnIndex(4);
            if (!row.sourceModel.empty())
                ImGui::TextUnformatted(row.sourceModel.c_str());
            else if (!row.className.empty())
                ImGui::TextUnformatted(row.className.c_str());
            else
                ImGui::TextUnformatted("-");
        }

        ImGui::EndTable();
    }
}

vector<RenderCall_View::Aggregated_Row> RenderCall_View::Build_GroupRows(
    const Engine::RenderViewHotspotSnapshot& snapshot,
    Engine::RenderGroup renderGroup) const
{
    unordered_map<uintptr_t, Aggregated_Row> rowMap{};

    for (const Engine::RenderViewHotspotEntry& entry : snapshot.entries)
    {
        if (entry.renderGroup != renderGroup)
            continue;

        Aggregated_Row& row = rowMap[entry.objectKey];
        if (row.objectKey == 0)
        {
            row.objectKey = entry.objectKey;
            row.name = Build_RowName(entry);
            row.className = entry.className;
            row.sourceModel = Build_SourceModelLabel(entry.sourceModelPath);
            row.isInstanced = entry.renderHotspotInstanceCount > 0;
            row.instanceCount = entry.renderHotspotInstanceCount;
        }

        ++row.submissionCount;

        if (entry.renderHotspotInstanceCount > 0)
        {
            row.isInstanced = true;
            row.instanceCount = max(row.instanceCount, entry.renderHotspotInstanceCount);
        }
    }

    vector<Aggregated_Row> rows{};
    rows.reserve(rowMap.size());

    for (auto& [_, row] : rowMap)
    {
        row.count = row.isInstanced ? row.instanceCount : 1;
        rows.push_back(move(row));
    }

    sort(rows.begin(), rows.end(), [](const Aggregated_Row& lhs, const Aggregated_Row& rhs)
    {
        if (lhs.count != rhs.count)
            return lhs.count > rhs.count;

        return lhs.name < rhs.name;
    });

    return rows;
}

Shared<RenderCall_View> RenderCall_View::Create()
{
    Shared<RenderCall_View> instance = make_shared<RenderCall_View>();

    if (FAILED(instance->Initialize()))
        MSG_BOX("Failed to Create : RenderCall_View");

    return instance;
}

NS_END
