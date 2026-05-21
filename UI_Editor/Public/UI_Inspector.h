#pragma once

#include "UI_Editor_Window.h"
#include "UI_Document.h"

namespace UIEditor
{
    class UI_Editor_Manager;

    class UI_Inspector final : public UI_Editor_Window
    {
    public:
        explicit UI_Inspector(UI_Editor_Manager& editorManager);
        ~UI_Inspector() override = default;

    public:
        void Render() override;

    private:
        void Render_LayoutPresets(UI_Object& object);
        bool Render_TexturePicker(UI_Object& object, ObjectId& outCreatePatternOverlaySourceId);
        bool Render_FontPicker(UI_Object& object);
        bool Draw_UITextureSlotPicker(const char* label, const char* popupId, string& textureGuid, string& texturePath);
        bool Draw_FontSlotPicker(const char* label, const char* popupId, UI_Object& object);
        void Refresh_UITextureAssetCache();
        void Refresh_FilteredUITextureAssets(const string& searchText);

    public:
        static Shared<UI_Inspector> Create(UI_Editor_Manager& editorManager);

    private:
        UI_Editor_Manager& _editorManager;
        char _textureSearchBuffer[128] = {};
        char _fontSearchBuffer[128] = {};
        vector<AssetMeta> _uiTextureAssets = {};
        vector<const AssetMeta*> _filteredUITextureAssets = {};
        string _lastTextureSearchText = "";
        bool _uiTextureAssetCacheDirty = true;
        bool _filteredUITextureAssetsDirty = true;
    };
}
