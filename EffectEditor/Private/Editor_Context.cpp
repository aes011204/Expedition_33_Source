#include "Editor_Context.h"

void Editor_Context::Set_SelectObject(const Shared<GameObject>& gameObject)
{
    if (_selection.selectedObject == gameObject)
        return;

    _selection.selectedObject = gameObject;
    ++_selectionRevision;
}

void Editor_Context::Clear_Selection()
{
    if (_selection.selectedObject == nullptr)
        return;

    _selection.selectedObject.reset();
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
