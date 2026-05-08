#include "pch.h"
#include "PanelAssetBrowser.h"
#include "SceneContext.h"
//#include "MEUe5Importer.h"

PanelAssetBrowser::PanelAssetBrowser(SceneContext* ctx)
    : MEWindow{ L"Asset Browser" }, _ctx{ ctx }
{
    AddFolder("Saved Maps", _mapSavePath);
}

void PanelAssetBrowser::Render()
{
    bool open = IsOpen();
    if (ImGui::Begin("Asset Browser", &open))
    {
        if (ImGui::BeginTabBar("##folders"))
        {
            for (int i = 0; i < static_cast<int>(_folders.size()); ++i)
            {
                if (ImGui::BeginTabItem(_folders[i].label.c_str()))
                {
                    if (_selectedFolder != i)
                    {
                        _selectedFolder = i;
                        ScanCurrentFolder();
                    }
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }

        if (ImGui::Button("Refresh"))
        {
            ScanCurrentFolder();
        }

        ImGui::Separator();

        for (auto& file : _files)
        {
            ImGui::Selectable(file.c_str());
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            {
                const string fullPath = _folders[_selectedFolder].path + file;
                _ctx->LoadFromFile(fullPath);
            }
        }
    }
    ImGui::End();
    SetOpen(open);
}

void PanelAssetBrowser::ScanCurrentFolder()
{
    _files.clear();
    if (_folders.empty())
    {
        return;
    }

    const string& path = _folders[_selectedFolder].path;
    error_code ec;
    fs::create_directories(path, ec);

    for (auto& entry : fs::directory_iterator(path, ec))
    {
        if (entry.path().extension() == ".json")
        {
            _files.push_back(entry.path().filename().string());
        }
    }
}

void PanelAssetBrowser::AddFolder(const string& label, const string& path, bool isImport)
{
    _folders.push_back({ label, path, isImport });
    if (_folders.size() == 1)
    {
        ScanCurrentFolder();
    }
}

Shared<PanelAssetBrowser> PanelAssetBrowser::Create(SceneContext* ctx)
{
    return make_shared<PanelAssetBrowser>(ctx);
}
