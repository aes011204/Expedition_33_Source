#include "Content_Browser.h"

Content_Browser::Content_Browser()
    : Editor_Window{ L"Content Browser" }
    , _resourceRoot{ fs::path{ ".." } / "Bin" / "Resources" }
{
}

Content_Browser::~Content_Browser()
{
    Free();
}

void Content_Browser::Update(float timeDelta)
{
}

void Content_Browser::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();

    const string windowName = String::ToString(Get_WindowName());
    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        ImGui::TextUnformatted(_resourceRoot.string().c_str());
        ImGui::Separator();
        Render_Directory(_resourceRoot);
    }
    Set_Open(isOpen);
    ImGui::End();
}

void Content_Browser::Render_Directory(const fs::path& rootPath)
{
    if (!fs::exists(rootPath))
    {
        ImGui::TextDisabled("Resource folder is not available yet.");
        return;
    }

    uint32 visibleCount = 0;

    for (const fs::directory_entry& entry : fs::directory_iterator(rootPath))
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

Shared<Content_Browser> Content_Browser::Create()
{
    return make_shared<Content_Browser>();
}

void Content_Browser::Free()
{
    __super::Free();
}
