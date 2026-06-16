#pragma once

#include "UI_Editor_Window.h"
#include "UI_SpriteVfx.h"

namespace UIEditor
{
    class UI_Editor_Manager;

    class UI_Vfx_View final : public UI_Editor_Window
    {
    public:
        explicit UI_Vfx_View(UI_Editor_Manager& editorManager);
        ~UI_Vfx_View() override = default;

    public:
        void Render() override;

    private:
        void Render_Toolbar();
        void Render_AssetControls();
        void Render_TextureControls();
        void Render_SpawnControls();
        void Render_MotionControls();
        void Render_AppearanceControls();
        void Render_Preview();
        bool Save_Asset(bool forceSaveAs);
        bool Load_Asset();
        bool Show_SaveDialog(wstring& outFilePath) const;
        bool Show_LoadDialog(wstring& outFilePath) const;
        wstring Resolve_DefaultVfxDirectory() const;
        void Ensure_UivfxExtension(wstring& filePath) const;
        string Make_RuntimePath(const wstring& filePath) const;

    public:
        static Shared<UI_Vfx_View> Create(UI_Editor_Manager& editorManager);

    private:
        UI_Editor_Manager& _editorManager;
        Client::UISpriteVfxAsset _asset{};
        wstring _filePath{};
        bool _isDirty{ false };
        bool _isPlaying{ true };
        float _previewTime{ 0.f };
        char _nameBuffer[128]{};
        char _textureGuidBuffer[128]{};
        char _texturePathBuffer[512]{};
        char _maskTextureGuidBuffer[128]{};
        char _maskTexturePathBuffer[512]{};
        char _noiseTextureGuidBuffer[128]{};
        char _noiseTexturePathBuffer[512]{};
    };
}
