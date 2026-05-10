#pragma once
#include "MEWindow.h"

NS_BEGIN(Engine)
class RenderTarget;
NS_END

NS_BEGIN(MapEditor)

class MECamera;
struct SceneContext;

class PanelViewport final : public MEWindow
{
public:
    PanelViewport(Shared<MECamera> camera, Shared<RenderTarget> rt, SceneContext* ctx);
    virtual ~PanelViewport() = default;

public:
    void SetSnapSettings(Shared<SnapSettings> settings) { _snapSettings = settings; }

public:
    virtual void Update(float timeDelta) override;
    virtual void Render() override;

private:
    Shared<MECamera> _camera;
    Shared<RenderTarget> _rt;
    SceneContext* _ctx = {};

    bool _isHovered = false;

    ImGuizmo::OPERATION _gizmoOp = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE _gizmoMode = ImGuizmo::WORLD;

    Shared<SnapSettings> _snapSettings;

    bool _pickRequested = false;
    ImVec2 _pickMousePos = {};
    ImVec2 _pickVpPos = {};
    ImVec2 _pickVpSize = {};

    bool _spawnRequested = false;
    string _spawnModelPath = {};
    ImVec2 _spawnMousePos = {};
    ImVec2 _spawnVpPos = {};
    ImVec2 _spawnVpSize = {};

    bool _cursorSetRequested = false;
    ImVec2 _cursorMousePos = {};
    ImVec2 _cursorVpPos = {};
    ImVec2 _cursorVpSize = {};

public:
    bool ConsumePick(ImVec2& outMousePos, ImVec2& outVpPos, ImVec2& outVpSize);
    bool ConsumeSpawn(string& outModelPath, ImVec2& outMousePos, ImVec2& outVpPos, ImVec2& outVpSize);
    bool ConsumeCursorSet(ImVec2& outMousePos, ImVec2& outVpPos, ImVec2& outVpSize);

public:
    static Shared<PanelViewport> Create(Shared<MECamera> camera, Shared<RenderTarget> rt, SceneContext* ctx);
};

NS_END
