#pragma once
#include "Editor_Window.h"

NS_BEGIN(MainEditor)

class PathTool : public Editor_Window
{
public:
    PathTool();
    virtual ~PathTool() = default;

public:
    virtual void Update(float timeDelta) override;
    virtual void Render() override;

private:
    void Handle_PointPlacement();
    void Render_PointList();
    void Render_PathOverlay();
    void Render_SavePanel();
    void Save_ToJson();

private:
    vector<Vec3> _points;

    char _saveNameBuffer[128] = "path_01";

public:
    static Shared<PathTool> Create();
};

NS_END
