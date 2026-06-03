#include "pch.h"
#include "UI_Editor_Manager.h"

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
    }

    bool UI_Editor_Manager::Is_ObjectEffectivelyVisible(ObjectId objectId) const
    {
        return _objectCommandService.Is_ObjectEffectivelyVisible(_document, objectId);
    }

    bool UI_Editor_Manager::Is_ObjectEffectivelyLocked(ObjectId objectId) const
    {
        return _objectCommandService.Is_ObjectEffectivelyLocked(_document, objectId);
    }

    bool UI_Editor_Manager::Set_ObjectVisible(ObjectId objectId, bool visible)
    {
        if (!_objectEditService.Can_SetObjectVisible(_document, objectId, visible))
            return false;

        Push_UndoSnapshot();
        _objectEditService.Set_ObjectVisible(_document, objectId, visible);
        Mark_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Set_ObjectLocked(ObjectId objectId, bool locked)
    {
        if (!_objectEditService.Can_SetObjectLocked(_document, objectId, locked))
            return false;

        Push_UndoSnapshot();
        _objectEditService.Set_ObjectLocked(_document, objectId, locked);
        Mark_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Set_UIObjectLocalPosition(ObjectId objectId, const ImVec2& position)
    {
        return Apply_UIObjectEdit(
            objectId,
            [&](const UI_Object&) { return _objectEditService.Can_SetObjectLocalPosition(_document, objectId, position); },
            [&]() { return _objectEditService.Set_ObjectLocalPosition(_document, objectId, position); });
    }

    bool UI_Editor_Manager::Set_UIObjectSize(ObjectId objectId, const ImVec2& size)
    {
        return Apply_UIObjectEdit(
            objectId,
            [&](const UI_Object&) { return _objectEditService.Can_SetObjectSize(_document, objectId, size); },
            [&]() { return _objectEditService.Set_ObjectSize(_document, objectId, size); });
    }

    bool UI_Editor_Manager::Set_UIObjectScale(ObjectId objectId, const ImVec2& scale)
    {
        return Apply_UIObjectEdit(
            objectId,
            [&](const UI_Object&) { return _objectEditService.Can_SetObjectScale(_document, objectId, scale); },
            [&]() { return _objectEditService.Set_ObjectScale(_document, objectId, scale); });
    }

    bool UI_Editor_Manager::Set_UIObjectRotationRadians(ObjectId objectId, float rotationRadians)
    {
        return Apply_UIObjectEdit(
            objectId,
            [&](const UI_Object&) { return _objectEditService.Can_SetObjectRotationRadians(_document, objectId, rotationRadians); },
            [&]() { return _objectEditService.Set_ObjectRotationRadians(_document, objectId, rotationRadians); });
    }

    bool UI_Editor_Manager::Set_UIObjectSortOrder(ObjectId objectId, int32 sortOrder)
    {
        return Apply_UIObjectEdit(
            objectId,
            [&](const UI_Object&) { return _objectEditService.Can_SetObjectSortOrder(_document, objectId, sortOrder); },
            [&]() { return _objectEditService.Set_ObjectSortOrder(_document, objectId, sortOrder); });
    }

    bool UI_Editor_Manager::Set_UIObjectRenderGroup(ObjectId objectId, UI_RenderGroup renderGroup)
    {
        return Apply_UIObjectEdit(
            objectId,
            [&](const UI_Object&) { return _objectEditService.Can_SetObjectRenderGroup(_document, objectId, renderGroup); },
            [&]() { return _objectEditService.Set_ObjectRenderGroup(_document, objectId, renderGroup); });
    }

    bool UI_Editor_Manager::Set_UIObjectColor(ObjectId objectId, const ImVec4& color)
    {
        return Apply_UIObjectEdit(
            objectId,
            [&](const UI_Object&) { return _objectEditService.Can_SetObjectColor(_document, objectId, color); },
            [&]() { return _objectEditService.Set_ObjectColor(_document, objectId, color); });
    }

    bool UI_Editor_Manager::Set_UIObjectAlphaMultiplier(ObjectId objectId, float alphaMultiplier)
    {
        return Apply_UIObjectEdit(
            objectId,
            [&](const UI_Object&) { return _objectEditService.Can_SetObjectAlphaMultiplier(_document, objectId, alphaMultiplier); },
            [&]() { return _objectEditService.Set_ObjectAlphaMultiplier(_document, objectId, alphaMultiplier); });
    }

    bool UI_Editor_Manager::Set_UIObjectImageRenderMode(ObjectId objectId, UI_ImageRenderMode imageRenderMode)
    {
        return Apply_UIObjectEdit(
            objectId,
            [&](const UI_Object&) { return _objectEditService.Can_SetObjectImageRenderMode(_document, objectId, imageRenderMode); },
            [&]() { return _objectEditService.Set_ObjectImageRenderMode(_document, objectId, imageRenderMode); });
    }

    bool UI_Editor_Manager::Set_UIObjectTextureBinding(ObjectId objectId, const string& textureGuid, const string& texturePath)
    {
        return Apply_UIObjectEdit(
            objectId,
            [&](const UI_Object&) { return _objectEditService.Can_SetObjectTextureBinding(_document, objectId, textureGuid, texturePath); },
            [&]() { return _objectEditService.Set_ObjectTextureBinding(_document, objectId, textureGuid, texturePath); });
    }

    bool UI_Editor_Manager::Set_UIObjectMaskTextureBinding(ObjectId objectId, const string& textureGuid, const string& texturePath)
    {
        return Apply_UIObjectEdit(
            objectId,
            [&](const UI_Object&) { return _objectEditService.Can_SetObjectMaskTextureBinding(_document, objectId, textureGuid, texturePath); },
            [&]() { return _objectEditService.Set_ObjectMaskTextureBinding(_document, objectId, textureGuid, texturePath); });
    }

    bool UI_Editor_Manager::Set_UIObjectMaskTextureMode(ObjectId objectId, UI_MaskTextureMode maskTextureMode)
    {
        return Apply_UIObjectEdit(
            objectId,
            [&](const UI_Object&) { return _objectEditService.Can_SetObjectMaskTextureMode(_document, objectId, maskTextureMode); },
            [&]() { return _objectEditService.Set_ObjectMaskTextureMode(_document, objectId, maskTextureMode); });
    }

    bool UI_Editor_Manager::Set_UIObjectPatternMaskTextureBinding(ObjectId objectId, const string& textureGuid, const string& texturePath)
    {
        return Apply_UIObjectEdit(
            objectId,
            [&](const UI_Object&) { return _objectEditService.Can_SetObjectPatternMaskTextureBinding(_document, objectId, textureGuid, texturePath); },
            [&]() { return _objectEditService.Set_ObjectPatternMaskTextureBinding(_document, objectId, textureGuid, texturePath); });
    }

    bool UI_Editor_Manager::Set_UIObjectText(ObjectId objectId, const string& text)
    {
        return Apply_UIObjectEdit(
            objectId,
            [&](const UI_Object&) { return _objectEditService.Can_SetObjectText(_document, objectId, text); },
            [&]() { return _objectEditService.Set_ObjectText(_document, objectId, text); });
    }

    bool UI_Editor_Manager::Set_UIObjectFontSize(ObjectId objectId, float fontSize)
    {
        return Apply_UIObjectEdit(
            objectId,
            [&](const UI_Object&) { return _objectEditService.Can_SetObjectFontSize(_document, objectId, fontSize); },
            [&]() { return _objectEditService.Set_ObjectFontSize(_document, objectId, fontSize); });
    }

    bool UI_Editor_Manager::Set_UIObjectTextAlign(ObjectId objectId, UI_TextAlign textAlign)
    {
        return Apply_UIObjectEdit(
            objectId,
            [&](const UI_Object&) { return _objectEditService.Can_SetObjectTextAlign(_document, objectId, textAlign); },
            [&]() { return _objectEditService.Set_ObjectTextAlign(_document, objectId, textAlign); });
    }

    bool UI_Editor_Manager::Set_UIObjectTextWrap(ObjectId objectId, bool textWrap)
    {
        return Apply_UIObjectEdit(
            objectId,
            [&](const UI_Object&) { return _objectEditService.Can_SetObjectTextWrap(_document, objectId, textWrap); },
            [&]() { return _objectEditService.Set_ObjectTextWrap(_document, objectId, textWrap); });
    }

    bool UI_Editor_Manager::Set_UIObjectFontBinding(
        ObjectId objectId,
        const string& fontGuid,
        const string& fontPath,
        const string& fontTag)
    {
        return Apply_UIObjectEdit(
            objectId,
            [&](const UI_Object&) { return _objectEditService.Can_SetObjectFontBinding(_document, objectId, fontGuid, fontPath, fontTag); },
            [&]() { return _objectEditService.Set_ObjectFontBinding(_document, objectId, fontGuid, fontPath, fontTag); });
    }

    bool UI_Editor_Manager::Set_UIObjectStateAnimationPath(ObjectId objectId, const string& stateAnimationPath)
    {
        return Apply_UIObjectEdit(
            objectId,
            [&](const UI_Object&) { return _objectEditService.Can_SetObjectStateAnimationPath(_document, objectId, stateAnimationPath); },
            [&]() { return _objectEditService.Set_ObjectStateAnimationPath(_document, objectId, stateAnimationPath); });
    }

    bool UI_Editor_Manager::Set_UIObjectWorldPosition(ObjectId objectId, const ImVec2& worldPosition)
    {
        if (Is_ObjectEffectivelyLocked(objectId))
            return false;

        if (nullptr == static_cast<const UI_Document&>(_document).Find_Object(objectId) ||
            NearlyEqual(_document.Get_WorldPosition(objectId), worldPosition))
        {
            return false;
        }

        if (!_historyService.Is_EditTransactionOpen())
            Push_UndoSnapshot();

        if (!_transformService.Set_WorldPosition(_document, objectId, worldPosition))
            return false;

        Mark_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Set_UIObjectWorldRect(ObjectId objectId, const ImVec2& worldPosition, const ImVec2& worldSize)
    {
        if (Is_ObjectEffectivelyLocked(objectId))
            return false;

        if (!_transformService.Can_SetWorldRect(_document, objectId))
            return false;

        if (NearlyEqual(_document.Get_WorldPosition(objectId), worldPosition) &&
            NearlyEqual(_document.Get_WorldSize(objectId), worldSize))
        {
            return false;
        }

        if (!_historyService.Is_EditTransactionOpen())
            Push_UndoSnapshot();

        if (!_transformService.Set_WorldRect(_document, objectId, worldPosition, worldSize))
            return false;

        Mark_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Set_UIObjectPivotKeepWorldRect(ObjectId objectId, const ImVec2& pivot)
    {
        if (Is_ObjectEffectivelyLocked(objectId))
            return false;

        if (!_transformService.Can_SetPivotKeepWorldRect(_document, objectId, pivot))
            return false;

        if (!_historyService.Is_EditTransactionOpen())
            Push_UndoSnapshot();

        if (!_transformService.Set_PivotKeepWorldRect(_document, objectId, pivot))
            return false;

        Mark_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Set_UIObjectAnchorsKeepWorldRect(ObjectId objectId, const ImVec2& anchorMin, const ImVec2& anchorMax)
    {
        if (Is_ObjectEffectivelyLocked(objectId))
            return false;

        if (!_transformService.Can_SetAnchorsKeepWorldRect(_document, objectId, anchorMin, anchorMax))
            return false;

        if (!_historyService.Is_EditTransactionOpen())
            Push_UndoSnapshot();

        if (!_transformService.Set_AnchorsKeepWorldRect(_document, objectId, anchorMin, anchorMax))
            return false;

        Mark_Dirty();
        return true;
    }

    ImVec2 UI_Editor_Manager::Get_UIObjectWorldPosition(ObjectId objectId) const
    {
        return _transformService.Get_WorldTopLeft(_document, objectId);
    }

    bool UI_Editor_Manager::Align_SelectedObjects(UI_AlignmentCommand command)
    {
        vector<ObjectId> targetObjectIds = _objectCommandService.Filter_UnlockedObjectIds(_document, Get_RootSelectedObjectIds());
        if (!_objectCommandService.Can_AlignObjects(targetObjectIds.size(), command))
            return false;

        Begin_EditTransaction();

        const bool changed = _objectCommandService.Align_Objects(_document, targetObjectIds, command);
        if (changed)
            Mark_Dirty();

        Commit_EditTransactionIfChanged();
        return changed;
    }

    bool UI_Editor_Manager::Apply_UIObjectEdit(
        ObjectId objectId,
        const function<bool(const UI_Object&)>& canApply,
        const function<bool()>& applyEdit)
    {
        if (Is_ObjectEffectivelyLocked(objectId))
            return false;

        const UI_Object* object = static_cast<const UI_Document&>(_document).Find_Object(objectId);
        if (nullptr == object)
            return false;

        if (canApply && !canApply(*object))
            return false;

        if (!_historyService.Is_EditTransactionOpen())
            Push_UndoSnapshot();

        if (!applyEdit || !applyEdit())
            return false;

        Mark_Dirty();
        return true;
    }
}
