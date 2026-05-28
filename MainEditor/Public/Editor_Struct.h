#pragma once

NS_BEGIN(Engine)
class GameObject;
class UI_Base;
NS_END

namespace MainEditor
{
    struct EditorSelection
    {
        Shared<Engine::GameObject> selectedObject;
        Weak<Engine::UI_Base> selectedUI{};
        string selectedUIGroup{};
        string selectedUIDisplayName{};
        bool selectedUIPositionDrivenByCode = false;
    };

    struct EditorFocusRequest
    {
        Shared<Engine::GameObject> targetObject;
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
