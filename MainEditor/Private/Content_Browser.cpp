#include "Content_Browser.h"

#include "Editor_ImGuiInclude.h"
#include "Utils.h"

Content_Browser::Content_Browser()
    : EditorWindow{ L"Content Browser" }
    , _resourceRoot{ filesystem::path{ ".." } / "Bin" / "Resources" }
{
}

void Content_Browser::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();

    const string windowName = Utils::ToString(Get_WindowName());
    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        ImGui::TextUnformatted(_resourceRoot.string().c_str());
        ImGui::Separator();
        Render_Directory(_resourceRoot);
    }
    Set_Open(isOpen);
    ImGui::End();
}

void Content_Browser::Render_Directory(const filesystem::path& rootPath)
{
    if (!filesystem::exists(rootPath))
    {
        ImGui::TextDisabled("Resource folder is not available yet.");
        return;
    }

    uint32 visibleCount = 0;

    for (const filesystem::directory_entry& entry : filesystem::directory_iterator(rootPath))
    {
        const string strName = entry.path().filename().string();
        if (entry.is_directory())
            ImGui::BulletText("[%s]", strName.c_str());
        else
            ImGui::BulletText("%s", strName.c_str());

        if (++visibleCount >= 128)
        {
            ImGui::TextDisabled("More items hidden...");
            break;
        }
    }
}



