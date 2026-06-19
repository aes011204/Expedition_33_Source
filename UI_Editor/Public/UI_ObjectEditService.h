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
        bool Can_SetObjectAlphaMultiplier(const UI_Document& document, ObjectId objectId, float alphaMultiplier) const;
        bool Set_ObjectAlphaMultiplier(UI_Document& document, ObjectId objectId, float alphaMultiplier) const;

        bool Can_SetObjectImageRenderMode(const UI_Document& document, ObjectId objectId, UI_ImageRenderMode imageRenderMode) const;
        bool Set_ObjectImageRenderMode(UI_Document& document, ObjectId objectId, UI_ImageRenderMode imageRenderMode) const;
        bool Can_SetObjectTextureBinding(const UI_Document& document, ObjectId objectId, const string& textureGuid, const string& texturePath) const;
        bool Set_ObjectTextureBinding(UI_Document& document, ObjectId objectId, const string& textureGuid, const string& texturePath) const;
        bool Can_SetObjectMaskTextureBinding(const UI_Document& document, ObjectId objectId, const string& textureGuid, const string& texturePath) const;
        bool Set_ObjectMaskTextureBinding(UI_Document& document, ObjectId objectId, const string& textureGuid, const string& texturePath) const;
        bool Can_SetObjectMaskTextureMode(const UI_Document& document, ObjectId objectId, UI_MaskTextureMode maskTextureMode) const;
        bool Set_ObjectMaskTextureMode(UI_Document& document, ObjectId objectId, UI_MaskTextureMode maskTextureMode) const;
        bool Can_SetObjectPatternMaskTextureBinding(const UI_Document& document, ObjectId objectId, const string& textureGuid, const string& texturePath) const;
        bool Set_ObjectPatternMaskTextureBinding(UI_Document& document, ObjectId objectId, const string& textureGuid, const string& texturePath) const;
        bool Can_SetObjectPatternUvOffset(const UI_Document& document, ObjectId objectId, const ImVec2& patternUvOffset) const;
        bool Set_ObjectPatternUvOffset(UI_Document& document, ObjectId objectId, const ImVec2& patternUvOffset) const;
        bool Can_SetObjectPatternUvScale(const UI_Document& document, ObjectId objectId, const ImVec2& patternUvScale) const;
        bool Set_ObjectPatternUvScale(UI_Document& document, ObjectId objectId, const ImVec2& patternUvScale) const;
        bool Can_SetObjectPatternUv(const UI_Document& document, ObjectId objectId, const ImVec2& patternUvOffset, const ImVec2& patternUvScale) const;
        bool Set_ObjectPatternUv(UI_Document& document, ObjectId objectId, const ImVec2& patternUvOffset, const ImVec2& patternUvScale) const;
        bool Can_SetObjectUseFillRatio(const UI_Document& document, ObjectId objectId, bool useFillRatio) const;
        bool Set_ObjectUseFillRatio(UI_Document& document, ObjectId objectId, bool useFillRatio) const;
        bool Can_SetObjectFillRatio(const UI_Document& document, ObjectId objectId, float fillRatio) const;
        bool Set_ObjectFillRatio(UI_Document& document, ObjectId objectId, float fillRatio) const;
        bool Can_SetObjectFillOrigin(const UI_Document& document, ObjectId objectId, UI_FillOrigin fillOrigin) const;
        bool Set_ObjectFillOrigin(UI_Document& document, ObjectId objectId, UI_FillOrigin fillOrigin) const;
        bool Can_SetObjectMaterialVfx(const UI_Document& document, ObjectId objectId, const UI_MaterialVfxDesc& materialVfx) const;
        bool Set_ObjectMaterialVfx(UI_Document& document, ObjectId objectId, const UI_MaterialVfxDesc& materialVfx) const;
        bool Can_SetObjectButtonSheen(const UI_Document& document, ObjectId objectId, const UI_ButtonSheenDesc& buttonSheen) const;
        bool Set_ObjectButtonSheen(UI_Document& document, ObjectId objectId, const UI_ButtonSheenDesc& buttonSheen) const;

        bool Can_SetObjectVfxPath(const UI_Document& document, ObjectId objectId, const string& vfxPath) const;
        bool Set_ObjectVfxPath(UI_Document& document, ObjectId objectId, const string& vfxPath) const;
        bool Can_SetObjectVfxPlayOnOpen(const UI_Document& document, ObjectId objectId, bool playOnOpen) const;
        bool Set_ObjectVfxPlayOnOpen(UI_Document& document, ObjectId objectId, bool playOnOpen) const;
        bool Can_SetObjectVfxLoop(const UI_Document& document, ObjectId objectId, bool loop) const;
        bool Set_ObjectVfxLoop(UI_Document& document, ObjectId objectId, bool loop) const;
        bool Can_SetObjectVfxStopOnClose(const UI_Document& document, ObjectId objectId, bool stopOnClose) const;
        bool Set_ObjectVfxStopOnClose(UI_Document& document, ObjectId objectId, bool stopOnClose) const;

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

        bool Can_SetObjectStateAnimationPath(const UI_Document& document, ObjectId objectId, const string& stateAnimationPath) const;
        bool Set_ObjectStateAnimationPath(UI_Document& document, ObjectId objectId, const string& stateAnimationPath) const;
    };
}
