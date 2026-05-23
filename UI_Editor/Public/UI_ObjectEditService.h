#pragma once

#include "UI_Document.h"

namespace UIEditor
{
    class UI_ObjectEditService final
    {
    public:
        UI_ObjectEditService() = default;
        ~UI_ObjectEditService() = default;

    public:
        bool Can_SetObjectVisible(const UI_Document& document, ObjectId objectId, bool visible) const;
        bool Set_ObjectVisible(UI_Document& document, ObjectId objectId, bool visible) const;
        bool Can_SetObjectLocked(const UI_Document& document, ObjectId objectId, bool locked) const;
        bool Set_ObjectLocked(UI_Document& document, ObjectId objectId, bool locked) const;

        bool Can_SetObjectLocalPosition(const UI_Document& document, ObjectId objectId, const ImVec2& position) const;
        bool Set_ObjectLocalPosition(UI_Document& document, ObjectId objectId, const ImVec2& position) const;
        bool Can_SetObjectSize(const UI_Document& document, ObjectId objectId, const ImVec2& size) const;
        bool Set_ObjectSize(UI_Document& document, ObjectId objectId, const ImVec2& size) const;
        bool Can_SetObjectScale(const UI_Document& document, ObjectId objectId, const ImVec2& scale) const;
        bool Set_ObjectScale(UI_Document& document, ObjectId objectId, const ImVec2& scale) const;
        bool Can_SetObjectRotationRadians(const UI_Document& document, ObjectId objectId, float rotationRadians) const;
        bool Set_ObjectRotationRadians(UI_Document& document, ObjectId objectId, float rotationRadians) const;
        bool Can_SetObjectSortOrder(const UI_Document& document, ObjectId objectId, int32 sortOrder) const;
        bool Set_ObjectSortOrder(UI_Document& document, ObjectId objectId, int32 sortOrder) const;
        bool Can_SetObjectRenderGroup(const UI_Document& document, ObjectId objectId, UI_RenderGroup renderGroup) const;
        bool Set_ObjectRenderGroup(UI_Document& document, ObjectId objectId, UI_RenderGroup renderGroup) const;
        bool Can_SetObjectColor(const UI_Document& document, ObjectId objectId, const ImVec4& color) const;
        bool Set_ObjectColor(UI_Document& document, ObjectId objectId, const ImVec4& color) const;

        bool Can_SetObjectImageRenderMode(const UI_Document& document, ObjectId objectId, UI_ImageRenderMode imageRenderMode) const;
        bool Set_ObjectImageRenderMode(UI_Document& document, ObjectId objectId, UI_ImageRenderMode imageRenderMode) const;
        bool Can_SetObjectTextureBinding(const UI_Document& document, ObjectId objectId, const string& textureGuid, const string& texturePath) const;
        bool Set_ObjectTextureBinding(UI_Document& document, ObjectId objectId, const string& textureGuid, const string& texturePath) const;
        bool Can_SetObjectMaskTextureBinding(const UI_Document& document, ObjectId objectId, const string& textureGuid, const string& texturePath) const;
        bool Set_ObjectMaskTextureBinding(UI_Document& document, ObjectId objectId, const string& textureGuid, const string& texturePath) const;
        bool Can_SetObjectPatternMaskTextureBinding(const UI_Document& document, ObjectId objectId, const string& textureGuid, const string& texturePath) const;
        bool Set_ObjectPatternMaskTextureBinding(UI_Document& document, ObjectId objectId, const string& textureGuid, const string& texturePath) const;

        bool Can_SetObjectText(const UI_Document& document, ObjectId objectId, const string& text) const;
        bool Set_ObjectText(UI_Document& document, ObjectId objectId, const string& text) const;
        bool Can_SetObjectFontSize(const UI_Document& document, ObjectId objectId, float fontSize) const;
        bool Set_ObjectFontSize(UI_Document& document, ObjectId objectId, float fontSize) const;
        bool Can_SetObjectTextAlign(const UI_Document& document, ObjectId objectId, UI_TextAlign textAlign) const;
        bool Set_ObjectTextAlign(UI_Document& document, ObjectId objectId, UI_TextAlign textAlign) const;
        bool Can_SetObjectTextWrap(const UI_Document& document, ObjectId objectId, bool textWrap) const;
        bool Set_ObjectTextWrap(UI_Document& document, ObjectId objectId, bool textWrap) const;
        bool Can_SetObjectFontBinding(const UI_Document& document, ObjectId objectId, const string& fontGuid, const string& fontPath, const string& fontTag) const;
        bool Set_ObjectFontBinding(UI_Document& document, ObjectId objectId, const string& fontGuid, const string& fontPath, const string& fontTag) const;
    };
}
