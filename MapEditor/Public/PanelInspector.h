#pragma once
#include "MEWindow.h"

NS_BEGIN(MapEditor)

struct SceneContext;
class MECamera;
struct EditorObject;

class PanelInspector final : public MEWindow
{
private:
    static constexpr uint64 _shapeNum =  magic_enum::enum_count<EZoneShape>();
    static constexpr uint64 _lightNum = magic_enum::enum_count<LightType>() - 1;

public:
    explicit PanelInspector(SceneContext* ctx, MECamera* camera);
    virtual ~PanelInspector() = default;

public:
    virtual void Render() override;

private:
    SceneContext* _ctx = {};
    MECamera* _camera = {};

    const char* _shapes[_shapeNum] = {};
    const char* _lights[_lightNum] = {};

    bool _transformEditing = false;
    Vec3 _editPrevPos = {};
    Quat _editPrevRot = {};
    Vec3 _editPrevScale = {};
    Shared<EditorObject> _editTarget;

    ScatterZone _pendingZone = {};
    float _eraseRadius = 10.f;
    Vec3 _eraseCenter = {};

private:
    void DrawTransform(Shared<EditorObject>& obj);
    void DrawZoneShape(Shared<EditorObject>& obj);
    void DrawMaterialOverrides(Shared<EditorObject>& obj);
    void DrawTypeParams(Shared<EditorObject>& obj);

public:
    static Shared<PanelInspector> Create(SceneContext* ctx, MECamera* camera);
};

NS_END
