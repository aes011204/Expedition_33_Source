#include "pch.h"
#include "UI_Editor_PreviewService.h"

namespace UIEditor
{
    HRESULT UI_Editor_PreviewService::Initialize(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        bool assetManagerReady)
    {
        _device = device;
        _context = context;
        _assetManagerReady = assetManagerReady;

        if (nullptr != _context)
            _fontPreviewSpriteBatch = make_unique<SpriteBatch>(_context.Get());

        if (nullptr != _device)
        {
            D3D11_RASTERIZER_DESC rasterizerDesc = {};
            rasterizerDesc.FillMode = D3D11_FILL_SOLID;
            rasterizerDesc.CullMode = D3D11_CULL_NONE;
            rasterizerDesc.ScissorEnable = TRUE;
            rasterizerDesc.DepthClipEnable = TRUE;
            _device->CreateRasterizerState(&rasterizerDesc, _fontPreviewRasterizerState.GetAddressOf());
        }

        return S_OK;
    }

    void UI_Editor_PreviewService::Begin_Frame()
    {
        _fontPreviewCommands.clear();
        _texturePreviewLoadCountThisFrame = 0;
        _compositePreviewCreateCountThisFrame = 0;
        _materialVfxPreviewTime = fmodf(_materialVfxPreviewTime + max(0.f, ImGui::GetIO().DeltaTime), 32.f);
        _materialVfxPreviewCache.clear();
        _materialVfxPreviewFailedKeys.clear();
        _buttonSheenPreviewTime = fmodf(_buttonSheenPreviewTime + max(0.f, ImGui::GetIO().DeltaTime), 32.f);
        _buttonSheenPreviewCache.clear();
        _buttonSheenPreviewFailedKeys.clear();
    }

    void UI_Editor_PreviewService::Clear_RuntimeCaches()
    {
        _textureThumbCache.clear();
        _texturePreviewFailedKeys.clear();
        _maskedTexturePreviewCache.clear();
        _maskedTexturePreviewFailedKeys.clear();
        _fillRatioPreviewCache.clear();
        _fillRatioPreviewFailedKeys.clear();
        _patternOverlayPreviewCache.clear();
        _patternOverlayPreviewFailedKeys.clear();
        _materialVfxPreviewCache.clear();
        _materialVfxPreviewFailedKeys.clear();
        _materialVfxPreviewTime = 0.f;
        _buttonSheenPreviewCache.clear();
        _buttonSheenPreviewFailedKeys.clear();
        _buttonSheenPreviewTime = 0.f;
        _fontPreviewCache.clear();
        _fontPreviewFailedGuids.clear();
        _fontPreviewCommands.clear();
    }
}
