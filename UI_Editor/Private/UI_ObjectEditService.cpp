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

        bool NearlyEqual(const UI_MaterialVfxDesc& lhs, const UI_MaterialVfxDesc& rhs)
        {
            return lhs.mode == rhs.mode &&
                lhs.noiseTextureGuid == rhs.noiseTextureGuid &&
                lhs.noiseTexturePath == rhs.noiseTexturePath &&
                NearlyEqual(lhs.color, rhs.color) &&
                NearlyEqual(lhs.coreColor, rhs.coreColor) &&
                NearlyEqual(lhs.flowColor, rhs.flowColor) &&
                NearlyEqual(lhs.glow, rhs.glow) &&
                NearlyEqual(lhs.alpha, rhs.alpha) &&
                NearlyEqual(lhs.coreStrength, rhs.coreStrength) &&
                NearlyEqual(lhs.coreWidth, rhs.coreWidth) &&
                NearlyEqual(lhs.coreSoftness, rhs.coreSoftness) &&
                NearlyEqual(lhs.noiseStrength, rhs.noiseStrength) &&
                NearlyEqual(lhs.uvScroll, rhs.uvScroll) &&
                NearlyEqual(lhs.flowSpeed, rhs.flowSpeed) &&
                NearlyEqual(lhs.preserveColor, rhs.preserveColor) &&
                NearlyEqual(lhs.edgeWidth, rhs.edgeWidth) &&
                NearlyEqual(lhs.edgeSoftness, rhs.edgeSoftness) &&
                NearlyEqual(lhs.erosion, rhs.erosion) &&
                NearlyEqual(lhs.flicker, rhs.flicker) &&
                NearlyEqual(lhs.distortion, rhs.distortion) &&
                NearlyEqual(lhs.edgeWobbleSpeed, rhs.edgeWobbleSpeed);
        }

        bool NearlyEqual(const UI_ButtonSheenDesc& lhs, const UI_ButtonSheenDesc& rhs)
        {
            return NearlyEqual(lhs.color, rhs.color) &&
                NearlyEqual(lhs.speed, rhs.speed) &&
                NearlyEqual(lhs.width, rhs.width) &&
                NearlyEqual(lhs.softness, rhs.softness) &&
                NearlyEqual(lhs.angle, rhs.angle) &&
                NearlyEqual(lhs.noiseStrength, rhs.noiseStrength);
        }

        UI_MaterialVfxDesc Resolve_ClampedMaterialVfx(UI_MaterialVfxDesc materialVfx)
        {
            materialVfx.glow = Clamp_MaterialVfxGlow(materialVfx.glow);
            materialVfx.alpha = Clamp_MaterialVfxAlpha(materialVfx.alpha);
            materialVfx.coreStrength = Clamp_MaterialVfxCoreStrength(materialVfx.coreStrength);
            materialVfx.coreWidth = Clamp_MaterialVfxCoreWidth(materialVfx.coreWidth);
            materialVfx.coreSoftness = Clamp_MaterialVfxCoreSoftness(materialVfx.coreSoftness);
            materialVfx.noiseStrength = Clamp_MaterialVfxNoiseStrength(materialVfx.noiseStrength);
            materialVfx.flowSpeed = Clamp_MaterialVfxFlowSpeed(materialVfx.flowSpeed);
            materialVfx.preserveColor = Clamp_MaterialVfxPreserveColor(materialVfx.preserveColor);
            materialVfx.edgeWidth = Clamp_MaterialVfxEdgeWidth(materialVfx.edgeWidth);
            materialVfx.edgeSoftness = Clamp_MaterialVfxEdgeSoftness(materialVfx.edgeSoftness);
            materialVfx.erosion = Clamp_MaterialVfxErosion(materialVfx.erosion);
            materialVfx.flicker = Clamp_MaterialVfxFlicker(materialVfx.flicker);
            materialVfx.distortion = Clamp_MaterialVfxDistortion(materialVfx.distortion);
            materialVfx.edgeWobbleSpeed = Clamp_MaterialVfxEdgeWobbleSpeed(materialVfx.edgeWobbleSpeed);
            return materialVfx;
        }

        UI_ButtonSheenDesc Resolve_ClampedButtonSheen(UI_ButtonSheenDesc buttonSheen)
        {
            buttonSheen.speed = Clamp_ButtonSheenSpeed(buttonSheen.speed);
            buttonSheen.width = Clamp_ButtonSheenWidth(buttonSheen.width);
            buttonSheen.softness = Clamp_ButtonSheenSoftness(buttonSheen.softness);
            buttonSheen.angle = Clamp_ButtonSheenAngle(buttonSheen.angle);
            buttonSheen.noiseStrength = Clamp_ButtonSheenNoiseStrength(buttonSheen.noiseStrength);
            return buttonSheen;
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

        float Resolve_ClampedAlphaMultiplier(float alphaMultiplier)
        {
            return min(max(alphaMultiplier, 0.f), 1.f);
        }

        float Resolve_ClampedFillRatio(float fillRatio)
        {
            return Clamp_FillRatio(fillRatio);
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

    bool UI_ObjectEditService::Can_SetObjectAlphaMultiplier(const UI_Document& document, ObjectId objectId, float alphaMultiplier) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && !NearlyEqual(object->alphaMultiplier, Resolve_ClampedAlphaMultiplier(alphaMultiplier));
    }

    bool UI_ObjectEditService::Set_ObjectAlphaMultiplier(UI_Document& document, ObjectId objectId, float alphaMultiplier) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object)
            return false;

        const float clampedAlphaMultiplier = Resolve_ClampedAlphaMultiplier(alphaMultiplier);
        if (NearlyEqual(object->alphaMultiplier, clampedAlphaMultiplier))
            return false;

        object->alphaMultiplier = clampedAlphaMultiplier;
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

    bool UI_ObjectEditService::Can_SetObjectMaskTextureMode(const UI_Document& document, ObjectId objectId, UI_MaskTextureMode maskTextureMode) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && object->type == UI_ObjectType::Image && object->maskTextureMode != maskTextureMode;
    }

    bool UI_ObjectEditService::Set_ObjectMaskTextureMode(UI_Document& document, ObjectId objectId, UI_MaskTextureMode maskTextureMode) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::Image || object->maskTextureMode == maskTextureMode)
            return false;

        object->maskTextureMode = maskTextureMode;
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

    bool UI_ObjectEditService::Can_SetObjectPatternUvOffset(const UI_Document& document, ObjectId objectId, const ImVec2& patternUvOffset) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object &&
            object->type == UI_ObjectType::Image &&
            object->imageRenderMode == UI_ImageRenderMode::PatternOverlay &&
            !NearlyEqual(object->patternUvOffset, Clamp_PatternUvOffset(patternUvOffset));
    }

    bool UI_ObjectEditService::Set_ObjectPatternUvOffset(UI_Document& document, ObjectId objectId, const ImVec2& patternUvOffset) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::Image || object->imageRenderMode != UI_ImageRenderMode::PatternOverlay)
            return false;

        const ImVec2 clampedPatternUvOffset = Clamp_PatternUvOffset(patternUvOffset);
        if (NearlyEqual(object->patternUvOffset, clampedPatternUvOffset))
            return false;

        object->patternUvOffset = clampedPatternUvOffset;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectPatternUvScale(const UI_Document& document, ObjectId objectId, const ImVec2& patternUvScale) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object &&
            object->type == UI_ObjectType::Image &&
            object->imageRenderMode == UI_ImageRenderMode::PatternOverlay &&
            !NearlyEqual(object->patternUvScale, Clamp_PatternUvScale(patternUvScale));
    }

    bool UI_ObjectEditService::Set_ObjectPatternUvScale(UI_Document& document, ObjectId objectId, const ImVec2& patternUvScale) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::Image || object->imageRenderMode != UI_ImageRenderMode::PatternOverlay)
            return false;

        const ImVec2 clampedPatternUvScale = Clamp_PatternUvScale(patternUvScale);
        if (NearlyEqual(object->patternUvScale, clampedPatternUvScale))
            return false;

        object->patternUvScale = clampedPatternUvScale;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectPatternUv(
        const UI_Document& document,
        ObjectId objectId,
        const ImVec2& patternUvOffset,
        const ImVec2& patternUvScale) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        if (nullptr == object || object->type != UI_ObjectType::Image || object->imageRenderMode != UI_ImageRenderMode::PatternOverlay)
            return false;

        const ImVec2 clampedPatternUvOffset = Clamp_PatternUvOffset(patternUvOffset);
        const ImVec2 clampedPatternUvScale = Clamp_PatternUvScale(patternUvScale);
        return !NearlyEqual(object->patternUvOffset, clampedPatternUvOffset) ||
            !NearlyEqual(object->patternUvScale, clampedPatternUvScale);
    }

    bool UI_ObjectEditService::Set_ObjectPatternUv(
        UI_Document& document,
        ObjectId objectId,
        const ImVec2& patternUvOffset,
        const ImVec2& patternUvScale) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::Image || object->imageRenderMode != UI_ImageRenderMode::PatternOverlay)
            return false;

        const ImVec2 clampedPatternUvOffset = Clamp_PatternUvOffset(patternUvOffset);
        const ImVec2 clampedPatternUvScale = Clamp_PatternUvScale(patternUvScale);
        if (NearlyEqual(object->patternUvOffset, clampedPatternUvOffset) &&
            NearlyEqual(object->patternUvScale, clampedPatternUvScale))
        {
            return false;
        }

        object->patternUvOffset = clampedPatternUvOffset;
        object->patternUvScale = clampedPatternUvScale;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectUseFillRatio(const UI_Document& document, ObjectId objectId, bool useFillRatio) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object &&
            object->type == UI_ObjectType::Image &&
            object->useFillRatio != useFillRatio;
    }

    bool UI_ObjectEditService::Set_ObjectUseFillRatio(UI_Document& document, ObjectId objectId, bool useFillRatio) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::Image || object->useFillRatio == useFillRatio)
            return false;

        object->useFillRatio = useFillRatio;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectFillRatio(const UI_Document& document, ObjectId objectId, float fillRatio) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object &&
            object->type == UI_ObjectType::Image &&
            !NearlyEqual(object->fillRatio, Resolve_ClampedFillRatio(fillRatio));
    }

    bool UI_ObjectEditService::Set_ObjectFillRatio(UI_Document& document, ObjectId objectId, float fillRatio) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::Image)
            return false;

        const float clampedFillRatio = Resolve_ClampedFillRatio(fillRatio);
        if (NearlyEqual(object->fillRatio, clampedFillRatio))
            return false;

        object->fillRatio = clampedFillRatio;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectFillOrigin(const UI_Document& document, ObjectId objectId, UI_FillOrigin fillOrigin) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object &&
            object->type == UI_ObjectType::Image &&
            object->fillOrigin != fillOrigin;
    }

    bool UI_ObjectEditService::Set_ObjectFillOrigin(UI_Document& document, ObjectId objectId, UI_FillOrigin fillOrigin) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::Image || object->fillOrigin == fillOrigin)
            return false;

        object->fillOrigin = fillOrigin;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectMaterialVfx(const UI_Document& document, ObjectId objectId, const UI_MaterialVfxDesc& materialVfx) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object &&
            object->type == UI_ObjectType::Image &&
            !NearlyEqual(object->materialVfx, Resolve_ClampedMaterialVfx(materialVfx));
    }

    bool UI_ObjectEditService::Set_ObjectMaterialVfx(UI_Document& document, ObjectId objectId, const UI_MaterialVfxDesc& materialVfx) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::Image)
            return false;

        const UI_MaterialVfxDesc clampedMaterialVfx = Resolve_ClampedMaterialVfx(materialVfx);
        if (NearlyEqual(object->materialVfx, clampedMaterialVfx))
            return false;

        object->materialVfx = clampedMaterialVfx;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectButtonSheen(const UI_Document& document, ObjectId objectId, const UI_ButtonSheenDesc& buttonSheen) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object &&
            object->type == UI_ObjectType::Image &&
            !NearlyEqual(object->buttonSheen, Resolve_ClampedButtonSheen(buttonSheen));
    }

    bool UI_ObjectEditService::Set_ObjectButtonSheen(UI_Document& document, ObjectId objectId, const UI_ButtonSheenDesc& buttonSheen) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::Image)
            return false;

        const UI_ButtonSheenDesc clampedButtonSheen = Resolve_ClampedButtonSheen(buttonSheen);
        if (NearlyEqual(object->buttonSheen, clampedButtonSheen))
            return false;

        object->buttonSheen = clampedButtonSheen;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectVfxPath(const UI_Document& document, ObjectId objectId, const string& vfxPath) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && object->type == UI_ObjectType::UIVfx && object->vfxPath != vfxPath;
    }

    bool UI_ObjectEditService::Set_ObjectVfxPath(UI_Document& document, ObjectId objectId, const string& vfxPath) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::UIVfx || object->vfxPath == vfxPath)
            return false;

        object->vfxPath = vfxPath;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectVfxPlayOnOpen(const UI_Document& document, ObjectId objectId, bool playOnOpen) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && object->type == UI_ObjectType::UIVfx && object->vfxPlayOnOpen != playOnOpen;
    }

    bool UI_ObjectEditService::Set_ObjectVfxPlayOnOpen(UI_Document& document, ObjectId objectId, bool playOnOpen) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::UIVfx || object->vfxPlayOnOpen == playOnOpen)
            return false;

        object->vfxPlayOnOpen = playOnOpen;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectVfxLoop(const UI_Document& document, ObjectId objectId, bool loop) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && object->type == UI_ObjectType::UIVfx && object->vfxLoop != loop;
    }

    bool UI_ObjectEditService::Set_ObjectVfxLoop(UI_Document& document, ObjectId objectId, bool loop) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::UIVfx || object->vfxLoop == loop)
            return false;

        object->vfxLoop = loop;
        return true;
    }

    bool UI_ObjectEditService::Can_SetObjectVfxStopOnClose(const UI_Document& document, ObjectId objectId, bool stopOnClose) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && object->type == UI_ObjectType::UIVfx && object->vfxStopOnClose != stopOnClose;
    }

    bool UI_ObjectEditService::Set_ObjectVfxStopOnClose(UI_Document& document, ObjectId objectId, bool stopOnClose) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::UIVfx || object->vfxStopOnClose == stopOnClose)
            return false;

        object->vfxStopOnClose = stopOnClose;
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

    bool UI_ObjectEditService::Can_SetObjectStateAnimationPath(const UI_Document& document, ObjectId objectId, const string& stateAnimationPath) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && object->type == UI_ObjectType::Button && object->stateAnimationPath != stateAnimationPath;
    }

    bool UI_ObjectEditService::Set_ObjectStateAnimationPath(UI_Document& document, ObjectId objectId, const string& stateAnimationPath) const
    {
        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
        if (nullptr == object || object->type != UI_ObjectType::Button || object->stateAnimationPath == stateAnimationPath)
            return false;

        object->stateAnimationPath = stateAnimationPath;
        return true;
    }
}
