#include "pch.h"
#include "PanelAssetBrowser.h"
#include "SceneContext.h"

PanelAssetBrowser::PanelAssetBrowser(SceneContext* ctx)
    : MEWindow{ L"Asset Browser" }, _ctx{ ctx }
{
}

HRESULT PanelAssetBrowser::Initialize()
{
    error_code ec;
    _rootPath = fs::canonical(_mapSavePath, ec).string();
    if (ec || _rootPath.empty())
    {
        return E_FAIL;
    }

    NavigateTo(_rootPath);
    return S_OK;
}

void PanelAssetBrowser::Render()
{
    bool open = IsOpen();
    if (ImGui::Begin("Asset Browser", &open))
    {
        int breadcrumbsSize = static_cast<int>(_breadcrumbs.size());
        for (int i = 0; i < breadcrumbsSize; ++i)
        {
            if (i > 0)
            {
                ImGui::SameLine();
                ImGui::TextUnformatted(">");
                ImGui::SameLine();
            }

            if (ImGui::SmallButton(_breadcrumbs[i].c_str()))
            {
                NavigateTo(_breadcrumbPaths[i]);
                break;
            }
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("Refresh"))
        {
            ScanCurrentPath();
        }

        ImGui::SameLine();
        ImGui::TextDisabled(" | Map:");
        ImGui::SameLine();
        if (_ctx->_currentFilePath.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Untitled");
        }
        else
        {
            fs::path p(_ctx->_currentFilePath);
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "%s", p.filename().string().c_str());
        }

        ImGui::Separator();

        for (auto& dir : _dirs)
        {
            string label = "[Dir] " + dir.filename().string();
            ImGui::Selectable(label.c_str());
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            {
                NavigateTo(dir.string());
            }
        }

        for (auto& file : _files)
        {
            string filename = file.filename().string();
            string ext = file.extension().string();

            ImGui::Selectable(filename.c_str());

            if (ext == ".model" || ext == ".prefab")
            {
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                {
                    string fullPath = file.string();
                    ImGui::SetDragDropPayload("MODEL_PATH", fullPath.c_str(), fullPath.size() + 1);
                    ImGui::Text("Spawn : %s", filename.c_str());
                    ImGui::EndDragDropSource();
                }
            }

            if (ext == ".json")
            {
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                {
                    if (filename.size() > 13 && filename.substr(filename.size() - 13) == "_terrain.json")
                    {
                        _ctx->LoadTerrainConfig(file.string());
                    }
                    else
                    {
                        _ctx->LoadFromFile(file.string());
                    }
                }
            }
        }
    }
    ImGui::End();
    SetOpen(open);
}

void PanelAssetBrowser::ScanCurrentPath()
{
    _dirs.clear();
    _files.clear();

    error_code ec;
    for (auto& entry : fs::directory_iterator(_currentPath, ec))
    {
        if (entry.is_directory(ec))
        {
            _dirs.push_back(entry.path());
        }
        else
        {
            _files.push_back(entry.path());
        }
    }

    sort(_dirs.begin(), _dirs.end());
    sort(_files.begin(), _files.end());
}

void PanelAssetBrowser::NavigateTo(const string& path)
{
    error_code ec;
    if (!fs::exists(path, ec) || !fs::is_directory(path, ec))
    {
        return;
    }

    _currentPath = fs::canonical(path, ec).string();
    RebuildBreadcrumbs();
    ScanCurrentPath();
}

void PanelAssetBrowser::RebuildBreadcrumbs()
{
    _breadcrumbs.clear();
    _breadcrumbPaths.clear();

    fs::path root = fs::path(_rootPath);
    fs::path cur = fs::path(_currentPath);

    vector<fs::path> parts;
    for (fs::path p = cur; p != root.parent_path(); p = p.parent_path())
    {
        parts.push_back(p);
        if (p == root)
        {
            break;
        }
    }

    reverse(parts.begin(), parts.end());
    for (auto& p : parts)
    {
        _breadcrumbs.push_back(p.filename().string());
        _breadcrumbPaths.push_back(p.string());
    }
}

Shared<PanelAssetBrowser> PanelAssetBrowser::Create(SceneContext* ctx)
{
    auto instance = make_shared<PanelAssetBrowser>(ctx);
    if (FAILED(instance->Initialize()))
    {
        return nullptr;
    }

    return instance;
}
