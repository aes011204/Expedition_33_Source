#pragma once
#include "MEWindow.h"

NS_BEGIN(MapEditor)

struct SceneContext;

class PanelAssetBrowser final : public MEWindow
{
public:
    explicit PanelAssetBrowser(SceneContext* ctx);
    virtual ~PanelAssetBrowser() = default;

public:
    virtual void Render() override;

private:
    SceneContext* _ctx = {};
    string _rootPath = {};
    string _currentPath = {};
    vector<string> _breadcrumbs;
    vector<string> _breadcrumbPaths;
    vector<fs::path> _dirs;
    vector<fs::path> _files;

private:
    void ScanCurrentPath();
    void NavigateTo(const string& path);
    void RebuildBreadcrumbs();

public:
    static Shared<PanelAssetBrowser> Create(SceneContext* ctx);
};

NS_END
