#include "Content_Browser.h"

#include "GameInstance.h"

Content_Browser::Content_Browser()
    : Editor_Window{ L"Content Browser" }
{
    const wstring assetRoot = GAME->Get_AssetRoot();
    if (!assetRoot.empty())
        _resourceRoot = filesystem::path(assetRoot);
}

void Content_Browser::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();

    const string windowName = String::ToString(Get_WindowName());
    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        if (_resourceRoot.empty())
        {
            const wstring assetRoot = GAME->Get_AssetRoot();
            if (!assetRoot.empty())
                _resourceRoot = filesystem::path(assetRoot);
        }

        ImGui::TextUnformatted(_resourceRoot.string().c_str());
        ImGui::Separator();
        Render_Directory(_resourceRoot);
    }

    Set_Open(isOpen);
    ImGui::End();
}

void Content_Browser::Render_Directory(const filesystem::path& rootPath)
{
    if (rootPath.empty() || !filesystem::exists(rootPath))
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

Shared<Content_Browser> Content_Browser::Create()
{
    return make_shared<Content_Browser>();
}
