#pragma once
#include "MapEditorDefines.h"

NS_BEGIN(Engine)
class RenderTarget;
NS_END

NS_BEGIN(MapEditor)

class MECamera;
struct SceneContext;

class MEPickingSystem
{
public:
    explicit MEPickingSystem() = default;
    ~MEPickingSystem() = default;

public:
    HRESULT Initialize(ComPtr<Device> device, ComPtr<Context> context, uint32 w, uint32 h);

private:
    ComPtr<Device> _device;
    ComPtr<Context> _context;
    Shared<RenderTarget> _idRT;
    ComPtr<ID3D11Texture2D> _idStagingTex;

private:
    uint32 SampleID(const vector<Shared<EditorObject>>& objects, const ImVec2& mousePos, const ImVec2& vpPos, const ImVec2& vpSize, const RenderTarget& rt);
    void EnsureSize(uint32 w, uint32 h);

public:
    void DoPickAt(SceneContext& scene, const ImVec2& mousePos, const ImVec2& vpPos, const ImVec2& vpSize, const RenderTarget& rt);
    void SetCursorAt(SceneContext& scene, const MECamera& camera, const ImVec2& mousePos, const ImVec2& vpPos, const ImVec2& vpSize, const RenderTarget& rt);

public:
    static Unique<MEPickingSystem> Create(ComPtr<Device> device, ComPtr<Context> context, uint32 w, uint32 h);
};

NS_END
