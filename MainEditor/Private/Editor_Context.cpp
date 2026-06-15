#include "Editor_Context.h"

void Editor_Context::Set_SelectObject(const Shared<GameObject>& gameObject)
{
    if (_selection.selectedObject == gameObject)
        return;

    _selection.selectedUI.reset();
    _selection.selectedUIGroup.clear();
    _selection.selectedUIDisplayName.clear();
    _selection.selectedUIPositionDrivenByCode = false;
    _selection.selectedRuntimeWorldUITarget = nullptr;
    _selection.selectedRuntimeWorldUIGroup.clear();
    _selection.selectedRuntimeWorldUIDisplayName.clear();
    _selection.selectedObject = gameObject;
    ++_selectionRevision;
}

void Editor_Context::Set_SelectUI(
    const Shared<UI_Base>& ui,
    const string& groupName,
    const string& displayName,
    bool positionDrivenByCode)
{
    if (_selection.selectedObject == nullptr)
    {
        const Shared<UI_Base> selectedUI = _selection.selectedUI.lock();
        if (selectedUI == ui &&
            _selection.selectedUIGroup == groupName &&
            _selection.selectedUIDisplayName == displayName &&
            _selection.selectedUIPositionDrivenByCode == positionDrivenByCode)
        {
            return;
        }
    }

    _selection.selectedObject.reset();
    _selection.selectedUI = ui;
    _selection.selectedUIGroup = groupName;
    _selection.selectedUIDisplayName = displayName;
    _selection.selectedUIPositionDrivenByCode = positionDrivenByCode;
    _selection.selectedRuntimeWorldUITarget = nullptr;
    _selection.selectedRuntimeWorldUIGroup.clear();
    _selection.selectedRuntimeWorldUIDisplayName.clear();
    ++_selectionRevision;
}

void Editor_Context::Set_SelectRuntimeWorldUI(
    Client::IRuntimeWorldUI_EditorTarget* target,
    const string& groupName,
    const string& displayName)
{
    if (_selection.selectedObject == nullptr &&
        _selection.selectedUI.expired() &&
        _selection.selectedRuntimeWorldUITarget == target &&
        _selection.selectedRuntimeWorldUIGroup == groupName &&
        _selection.selectedRuntimeWorldUIDisplayName == displayName)
    {
        return;
    }

    _selection.selectedObject.reset();
    _selection.selectedUI.reset();
    _selection.selectedUIGroup.clear();
    _selection.selectedUIDisplayName.clear();
    _selection.selectedUIPositionDrivenByCode = false;
    _selection.selectedRuntimeWorldUITarget = target;
    _selection.selectedRuntimeWorldUIGroup = groupName;
    _selection.selectedRuntimeWorldUIDisplayName = displayName;
    ++_selectionRevision;
}

void Editor_Context::Clear_SelectedObject()
{
    if (_selection.selectedObject == nullptr)
        return;

    _selection.selectedObject.reset();
    ++_selectionRevision;
}

void Editor_Context::Clear_SelectedUI()
{
    const bool hasUI = !_selection.selectedUI.expired() ||
        !_selection.selectedUIGroup.empty() ||
        !_selection.selectedUIDisplayName.empty() ||
        _selection.selectedUIPositionDrivenByCode;

    if (!hasUI)
        return;

    _selection.selectedUI.reset();
    _selection.selectedUIGroup.clear();
    _selection.selectedUIDisplayName.clear();
    _selection.selectedUIPositionDrivenByCode = false;
    ++_selectionRevision;
}

void Editor_Context::Clear_SelectedRuntimeWorldUI()
{
    const bool hasRuntimeWorldUI = _selection.selectedRuntimeWorldUITarget != nullptr ||
        !_selection.selectedRuntimeWorldUIGroup.empty() ||
        !_selection.selectedRuntimeWorldUIDisplayName.empty();

    if (!hasRuntimeWorldUI)
        return;

    _selection.selectedRuntimeWorldUITarget = nullptr;
    _selection.selectedRuntimeWorldUIGroup.clear();
    _selection.selectedRuntimeWorldUIDisplayName.clear();
    ++_selectionRevision;
}

void Editor_Context::Clear_Selection()
{
    const bool hasObject = (_selection.selectedObject != nullptr);
    const bool hasUI = !_selection.selectedUI.expired() ||
        !_selection.selectedUIGroup.empty() ||
        !_selection.selectedUIDisplayName.empty() ||
        _selection.selectedUIPositionDrivenByCode;
    const bool hasRuntimeWorldUI = _selection.selectedRuntimeWorldUITarget != nullptr ||
        !_selection.selectedRuntimeWorldUIGroup.empty() ||
        !_selection.selectedRuntimeWorldUIDisplayName.empty();

    if (!hasObject && !hasUI && !hasRuntimeWorldUI)
        return;

    _selection.selectedObject.reset();
    _selection.selectedUI.reset();
    _selection.selectedUIGroup.clear();
    _selection.selectedUIDisplayName.clear();
    _selection.selectedUIPositionDrivenByCode = false;
    _selection.selectedRuntimeWorldUITarget = nullptr;
    _selection.selectedRuntimeWorldUIGroup.clear();
    _selection.selectedRuntimeWorldUIDisplayName.clear();
    ++_selectionRevision;
}

void Editor_Context::Request_FocusObject(const Shared<GameObject>& gameObject)
{
    _focusRequest.targetObject = gameObject;
}

Shared<GameObject> Editor_Context::Consume_FocusObjectRequest()
{
    Shared<GameObject> targetObject = _focusRequest.targetObject;
    _focusRequest.targetObject.reset();
    return targetObject;
}

Unique<Editor_Context> Editor_Context::Create()
{
    return make_unique<Editor_Context>();
}

void Editor_Context::Free()
{
    Base::Free();
}
