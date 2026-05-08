#pragma once
#include "MEWindow.h"

NS_BEGIN(MapEditor)

struct SceneContext;
struct EditorObject;

class PanelHierarchy final : public MEWindow
{
public:
    explicit PanelHierarchy(SceneContext* ctx);
    virtual ~PanelHierarchy() = default;

public:
    virtual void Render() override;

private:
    SceneContext* _ctx = {};
    int _renameIndex = -1;

    Shared<EditorObject> _prevSelected;
    bool _needScrollToSelected = false;

    bool _justStartedRename = false;
    char _renameBuffer[256] = {};

    char _modelFolderBuf[256] = {};
    
    EObjectType _types[static_cast<int>(EObjectType::Fail)] = {};

private:
    void StartRename(int index, const Shared<EditorObject>& obj);

public:
    static Shared<PanelHierarchy> Create(SceneContext* ctx);
};

NS_END
