#pragma once

#include "UI_Editor_Window.h"
#include "UI_InspectorAssetPickerService.h"
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
        void Render_LayoutPresets(const UI_Object& object);
        void Render_TexturePicker(const UI_Object& object, ObjectId& outCreatePatternOverlaySourceId);
        void Render_StateAnimationPicker(const UI_Object& object);
        void Render_FontPicker(const UI_Object& object);
        bool Draw_UITextureSlotPicker(const char* label, const char* popupId, string& textureGuid, string& texturePath);
        void Draw_FontSlotPicker(const char* label, const char* popupId, const UI_Object& object);

    public:
        static Shared<UI_Inspector> Create(UI_Editor_Manager& editorManager);

    private:
        UI_Editor_Manager& _editorManager;
        UI_InspectorAssetPickerService _assetPickerService = {};
        char _textureSearchBuffer[128] = {};
        char _fontSearchBuffer[128] = {};
    };
}
