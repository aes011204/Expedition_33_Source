#pragma once
#include "MEWindow.h"

NS_BEGIN(MapEditor)

struct SceneContext;

class PanelAssetBrowser final : public MEWindow
{
private:
    struct FolderEntry
    {
        string label;
        string path;
        bool isImport = false;
    };

public:
    explicit PanelAssetBrowser(SceneContext* ctx);
    virtual ~PanelAssetBrowser() = default;

public:
    virtual void Render() override;

private:
    SceneContext* _ctx = {};
    vector<FolderEntry> _folders;
    vector<string> _files;
    int _selectedFolder = {};

private:
    void ScanCurrentFolder();

public:
    void AddFolder(const string& label, const string& path, bool isImport = false);

public:
    static Shared<PanelAssetBrowser> Create(SceneContext* ctx);
};

NS_END
