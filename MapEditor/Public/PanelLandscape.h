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
    function<void(int32, const WCHAR*)> _onSave;
    function<void(int32, const WCHAR*)> _onLoad;
    function<void(int32, const LandscapeNoiseParams&)> _onApplyNoise;
    function<void(int32, const LandscapeErosionParams&)> _onApplyErosion;
    function<void(int32, const WCHAR*)> _onAddLayer;
    function<void(int32, uint32)> _onRemoveLayer;
    function<void(int32, uint32)> _onSetBaseLayer;
    function<void(int32, const LandscapeConfig&)> _onModify;

    bool   _showCreateDialog = false;
    int32  _createCountX = 64;
    int32  _createCountZ = 64;
    float  _createSizeX = 1.f;
    float  _createSizeZ = 1.f;
    float  _createOffsetX = 0.f;
    float  _createOffsetY = 0.f;
    float  _createOffsetZ = 0.f;
    char   _createName[64] = "Landscape_0";
    int32 _createChunkSize = 16;

    LandscapeNoiseParams _noiseParams{};
    LandscapeErosionParams _erosionParams{};

    bool _isModifyMode = false;
    int32 _modifyTargetIdx = -1;

private:
    void Render_LandscapeList();
    void Render_CreateDialog();
    void Render_SculptTools();
    void Render_WorldOffset();
    void Render_SaveLoad();
    void Render_Noise();
    void Render_Erosion();
    void Render_PaintTools();

public:
    static Shared<PanelLandscape> Create(SceneContext* ctx, const LandscapePanelContext& landscapeCtx);
};

NS_END
