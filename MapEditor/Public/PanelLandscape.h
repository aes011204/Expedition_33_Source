#pragma once
#include "MEWindow.h"
#include "MELandscapeActor.h"

NS_BEGIN(MapEditor)

struct SceneContext;

class PanelLandscape final : public MEWindow
{
public:
    PanelLandscape();
    virtual ~PanelLandscape() = default;

public:
    HRESULT Initialize(SceneContext* ctx, const LandscapePanelContext& landscapeCtx);
    virtual void Update(float timeDelta) override;
    virtual void Render() override;

private:
    SceneContext* _ctx = {};
    Shared<LandscapeSculptState> _sculptState;
    function<void(const LandscapeConfig&)> _onAdd;
    function<void(int32)> _onRemove;

    bool   _showCreateDialog = false;
    int32  _createCountX = 64;
    int32  _createCountZ = 64;
    float  _createSizeX = 1.f;
    float  _createSizeZ = 1.f;
    float  _createOffsetX = 0.f;
    float  _createOffsetY = 0.f;
    float  _createOffsetZ = 0.f;
    char   _createName[64] = "Landscape_0";

private:
    void Render_LandscapeList();
    void Render_CreateDialog();
    void Render_SculptTools();
    void Render_WorldOffset();

public:
    static Shared<PanelLandscape> Create(SceneContext* ctx, const LandscapePanelContext& landscapeCtx);
};

NS_END
