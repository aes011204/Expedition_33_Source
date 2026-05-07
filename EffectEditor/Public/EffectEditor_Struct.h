#pragma once

namespace EffectEditor
{
struct EditorSelection
{
    Shared<GameObject> selectedObject;
};

struct EditorFocusRequest
{
    Shared<GameObject> targetObject;
};

struct FNotification
{
    string message;
    NotifyType type;

    float duration = 3.f;
    float timer = 0.f;
    float alpha = 1.f;
};
}
