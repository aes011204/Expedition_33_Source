#pragma once
#include "MEWindow.h"

NS_BEGIN(Engine)
class RenderTarget;
NS_END

NS_BEGIN(MapEditor)

class MECamera;
struct SceneContext;
class MEPickingSystem;
class IEditorCommand;

class PanelViewport final : public MEWindow
{
public:
    PanelViewport(Shared<MECamera> camera, Shared<RenderTarget> rt, SceneContext* ctx);
    virtual ~PanelViewport() = default;

public:
    ID3D11ShaderResourceView* Get_SceneSRV() const { return _sceneSRV; }
    bool IsSpawnAsInstanced() const { return _spawnAsInstanced; }

    void SetSnapSettings(Shared<SnapSettings> settings) { _snapSettings = settings; }
    void Set_SceneSRV(ID3D11ShaderResourceView* srv) { _sceneSRV = srv; }
    void Set_SculpState(Shared<LandscapeSculptState> state) { _sculptState = state; }
    void Set_PickingSystem(MEPickingSystem* sys) { _pickingSystem = sys; }

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

    bool _gizmoWasUsing = false;
    Vec3 _gizmoPrevPos = {};
    Quat _gizmoPrevRot = {};
    Vec3 _gizmoPrevScale = {};

    ID3D11ShaderResourceView* _sceneSRV = {};

    Shared<LandscapeSculptState> _sculptState;

    bool _sculptRequested = false;
    ImVec2 _sculptMousePos = {};
    ImVec2 _sculptVpPos = {};
    ImVec2 _sculptVpSize = {};

    MEPickingSystem* _pickingSystem = {};

    bool _spawnAsInstanced = false;
    bool _deleteRequested = false;

private:
    bool Ray_Hit_Landscape(ImVec2 mousePos, ImVec2 vpPos, ImVec2 vpSz, Vec3& outHit);

public:
    bool ConsumePick(ImVec2& outMousePos, ImVec2& outVpPos, ImVec2& outVpSize);
    bool ConsumeSpawn(string& outModelPath, ImVec2& outMousePos, ImVec2& outVpPos, ImVec2& outVpSize);
    bool ConsumeCursorSet(ImVec2& outMousePos, ImVec2& outVpPos, ImVec2& outVpSize);
    bool ConsumeSculpt(ImVec2& outMousePos, ImVec2& outVpPos, ImVec2& outVpSize);
    bool ConsumeDelete();
    Shared<IEditorCommand> ConsumeDeleteCommand();

private:
    Shared<IEditorCommand> _deleteCommand = {};

public:
    static Shared<PanelViewport> Create(Shared<MECamera> camera, Shared<RenderTarget> rt, SceneContext* ctx);
};

NS_END
