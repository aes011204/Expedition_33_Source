#include "pch.h"
#include "UI_ObjectEditService.h"
#include "UI_DocumentInternalAccess.h"

namespace UIEditor
{
    namespace
    {
        bool NearlyEqual(float lhs, float rhs)
        {
            return fabsf(lhs - rhs) <= 0.001f;
        }

        bool NearlyEqual(const ImVec2& lhs, const ImVec2& rhs)
        {
            return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y);
        }

        bool NearlyEqual(const ImVec4& lhs, const ImVec4& rhs)
        {
            return NearlyEqual(lhs.x, rhs.x) &&
                NearlyEqual(lhs.y, rhs.y) &&
                NearlyEqual(lhs.z, rhs.z) &&
                NearlyEqual(lhs.w, rhs.w);
        }

        ImVec2 Resolve_ClampedScale(const ImVec2& scale)
        {
            return ImVec2(max(0.01f, scale.x), max(0.01f, scale.y));
        }

        ImVec2 Resolve_ClampedSize(const UI_Document& document, ObjectId objectId, const ImVec2& size)
        {
            const UI_Object* object = document.Find_Object(objectId);
            if (nullptr == object)
                return size;

            const ImVec2 parentReferenceSize = document.Get_ParentAnchorReferenceSize(objectId);
            const float minSizeX = 1.f - parentReferenceSize.x * max(0.f, object->anchorMax.x - object->anchorMin.x);
            const float minSizeY = 1.f - parentReferenceSize.y * max(0.f, object->anchorMax.y - object->anchorMin.y);
            return ImVec2(max(minSizeX, size.x), max(minSizeY, size.y));
        }

        float Resolve_ClampedFontSize(float fontSize)
        {
            return max(4.f, fontSize);
        }
    }

    bool UI_ObjectEditService::Can_SetObjectVisible(const UI_Document& document, ObjectId objectId, bool visible) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && object->visible != visible;
    }

    bool UI_ObjectEditService::Set_ObjectVisible(UI_Document& document, ObjectId objectId, bool visible) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->visible == visible)
            return false;

        object->visible = visible;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectLocked(const UI_Document& document, ObjectId objectId, bool locked) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && object->locked != locked;
    }

    bool UI_ObjectEditService::Set_ObjectLocked(UI_Document& document, ObjectId objectId, bool locked) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->locked == locked)
            return false;

        object->locked = locked;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectLocalPosition(const UI_Document& document, ObjectId objectId, const ImVec2& position) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && !NearlyEqual(object->position, position);
    }

    bool UI_ObjectEditService::Set_ObjectLocalPosition(UI_Document& document, ObjectId objectId, const ImVec2& position) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || NearlyEqual(object->position, position))
            return false;

        object->position = position;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectSize(const UI_Document& document, ObjectId objectId, const ImVec2& size) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        if (nullptr == object)
            return false;

        return !NearlyEqual(object->size, Resolve_ClampedSize(document, objectId, size));
    }

    bool UI_ObjectEditService::Set_ObjectSize(UI_Document& document, ObjectId objectId, const ImVec2& size) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object)
            return false;

        const ImVec2 clampedSize = Resolve_ClampedSize(document, objectId, size);
        if (NearlyEqual(object->size, clampedSize))
            return false;

        object->size = clampedSize;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectScale(const UI_Document& document, ObjectId objectId, const ImVec2& scale) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        if (nullptr == object)
            return false;

        return !NearlyEqual(object->scale, Resolve_ClampedScale(scale));
    }

    bool UI_ObjectEditService::Set_ObjectScale(UI_Document& document, ObjectId objectId, const ImVec2& scale) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object)
            return false;

        const ImVec2 clampedScale = Resolve_ClampedScale(scale);
        if (NearlyEqual(object->scale, clampedScale))
            return false;

        object->scale = clampedScale;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectRotationRadians(const UI_Document& document, ObjectId objectId, float rotationRadians) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && !NearlyEqual(object->rotationRadians, rotationRadians);
    }

    bool UI_ObjectEditService::Set_ObjectRotationRadians(UI_Document& document, ObjectId objectId, float rotationRadians) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || NearlyEqual(object->rotationRadians, rotationRadians))
            return false;

        object->rotationRadians = rotationRadians;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectSortOrder(const UI_Document& document, ObjectId objectId, int32 sortOrder) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && object->sortOrder != sortOrder;
    }

    bool UI_ObjectEditService::Set_ObjectSortOrder(UI_Document& document, ObjectId objectId, int32 sortOrder) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->sortOrder == sortOrder)
            return false;

        object->sortOrder = sortOrder;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectRenderGroup(const UI_Document& document, ObjectId objectId, UI_RenderGroup renderGroup) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && object->renderGroup != renderGroup;
    }

    bool UI_ObjectEditService::Set_ObjectRenderGroup(UI_Document& document, ObjectId objectId, UI_RenderGroup renderGroup) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->renderGroup == renderGroup)
            return false;

        object->renderGroup = renderGroup;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectColor(const UI_Document& document, ObjectId objectId, const ImVec4& color) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && !NearlyEqual(object->color, color);
    }

    bool UI_ObjectEditService::Set_ObjectColor(UI_Document& document, ObjectId objectId, const ImVec4& color) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || NearlyEqual(object->color, color))
            return false;

        object->color = color;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectImageRenderMode(const UI_Document& document, ObjectId objectId, UI_ImageRenderMode imageRenderMode) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && object->type == UI_ObjectType::Image && object->imageRenderMode != imageRenderMode;
    }

    bool UI_ObjectEditService::Set_ObjectImageRenderMode(UI_Document& document, ObjectId objectId, UI_ImageRenderMode imageRenderMode) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::Image || object->imageRenderMode == imageRenderMode)
            return false;

        object->imageRenderMode = imageRenderMode;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectTextureBinding(const UI_Document& document, ObjectId objectId, const string& textureGuid, const string& texturePath) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && object->type == UI_ObjectType::Image &&
            (object->textureGuid != textureGuid || object->texturePath != texturePath);
    }

    bool UI_ObjectEditService::Set_ObjectTextureBinding(UI_Document& document, ObjectId objectId, const string& textureGuid, const string& texturePath) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::Image ||
            (object->textureGuid == textureGuid && object->texturePath == texturePath))
        {
            return false;
        }

        object->textureGuid = textureGuid;
        object->texturePath = texturePath;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectMaskTextureBinding(const UI_Document& document, ObjectId objectId, const string& textureGuid, const string& texturePath) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && object->type == UI_ObjectType::Image &&
            (object->maskTextureGuid != textureGuid || object->maskTexturePath != texturePath);
    }

    bool UI_ObjectEditService::Set_ObjectMaskTextureBinding(UI_Document& document, ObjectId objectId, const string& textureGuid, const string& texturePath) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::Image ||
            (object->maskTextureGuid == textureGuid && object->maskTexturePath == texturePath))
        {
            return false;
        }

        object->maskTextureGuid = textureGuid;
        object->maskTexturePath = texturePath;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectPatternMaskTextureBinding(const UI_Document& document, ObjectId objectId, const string& textureGuid, const string& texturePath) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && object->type == UI_ObjectType::Image &&
            (object->patternMaskTextureGuid != textureGuid || object->patternMaskTexturePath != texturePath);
    }

    bool UI_ObjectEditService::Set_ObjectPatternMaskTextureBinding(UI_Document& document, ObjectId objectId, const string& textureGuid, const string& texturePath) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::Image ||
            (object->patternMaskTextureGuid == textureGuid && object->patternMaskTexturePath == texturePath))
        {
            return false;
        }

        object->patternMaskTextureGuid = textureGuid;
        object->patternMaskTexturePath = texturePath;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectText(const UI_Document& document, ObjectId objectId, const string& text) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && object->type == UI_ObjectType::Text && object->text != text;
    }

    bool UI_ObjectEditService::Set_ObjectText(UI_Document& document, ObjectId objectId, const string& text) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::Text || object->text == text)
            return false;

        object->text = text;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectFontSize(const UI_Document& document, ObjectId objectId, float fontSize) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        if (nullptr == object || object->type != UI_ObjectType::Text)
            return false;

        return !NearlyEqual(object->fontSize, Resolve_ClampedFontSize(fontSize));
    }

    bool UI_ObjectEditService::Set_ObjectFontSize(UI_Document& document, ObjectId objectId, float fontSize) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::Text)
            return false;

        const float clampedFontSize = Resolve_ClampedFontSize(fontSize);
        if (NearlyEqual(object->fontSize, clampedFontSize))
            return false;

        object->fontSize = clampedFontSize;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectTextAlign(const UI_Document& document, ObjectId objectId, UI_TextAlign textAlign) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && object->type == UI_ObjectType::Text && object->textAlign != textAlign;
    }

    bool UI_ObjectEditService::Set_ObjectTextAlign(UI_Document& document, ObjectId objectId, UI_TextAlign textAlign) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::Text || object->textAlign == textAlign)
            return false;

        object->textAlign = textAlign;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectTextWrap(const UI_Document& document, ObjectId objectId, bool textWrap) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && object->type == UI_ObjectType::Text && object->textWrap != textWrap;
    }

    bool UI_ObjectEditService::Set_ObjectTextWrap(UI_Document& document, ObjectId objectId, bool textWrap) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::Text || object->textWrap == textWrap)
            return false;

        object->textWrap = textWrap;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectFontBinding(
        const UI_Document& document,
        ObjectId objectId,
        const string& fontGuid,
        const string& fontPath,
        const string& fontTag) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && object->type == UI_ObjectType::Text &&
            (object->fontGuid != fontGuid || object->fontPath != fontPath || object->fontTag != fontTag);
    }

    bool UI_ObjectEditService::Set_ObjectFontBinding(
        UI_Document& document,
        ObjectId objectId,
        const string& fontGuid,
        const string& fontPath,
        const string& fontTag) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::Text ||
            (object->fontGuid == fontGuid && object->fontPath == fontPath && object->fontTag == fontTag))
        {
            return false;
        }

        object->fontGuid = fontGuid;
        object->fontPath = fontPath;
        object->fontTag = fontTag;
        return true;
    }
}
