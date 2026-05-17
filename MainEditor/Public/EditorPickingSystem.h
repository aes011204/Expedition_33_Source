#pragma once
#include "Editor_Defines.h"

NS_BEGIN(Engine)
class RenderTarget;
NS_END

NS_BEGIN(MainEditor)

class EditorPickingSystem
{
public:
    EditorPickingSystem() = default;
    ~EditorPickingSystem();

public:
    HRESULT Initialize(ComPtr<Device> device, ComPtr<Context> context, uint32 w, uint32 h);

private:
    ComPtr<Device> _device;
    ComPtr<Context> _context;

    Shared<RenderTarget> _idRT;
    ComPtr<ID3D11Texture2D> _idStagingTex;

private:
    void EnsureSize(uint32 w, uint32 h);
    uint32 SampleID(const ImVec2& mousePos, const ImVec2& vpMin, const ImVec2& vpMax);

public:
    void DoPickAt(const ImVec2& mousePos, const ImVec2& vpMin, const ImVec2& vpMax);

public:
    static Unique<EditorPickingSystem> Create(ComPtr<Device> device, ComPtr<Context> context, uint32 w, uint32 h);
};

NS_END
