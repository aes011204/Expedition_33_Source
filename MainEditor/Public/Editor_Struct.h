#pragma once

NS_BEGIN(Engine)
class GameObject;
class UI_Base;
NS_END

namespace Client
{
    class IRuntimeWorldUI_EditorTarget;
}

namespace MainEditor
{
    struct EditorSelection
    {
        Shared<Engine::GameObject> selectedObject;
        Weak<Engine::UI_Base> selectedUI{};
        string selectedUIGroup{};
        string selectedUIDisplayName{};
        bool selectedUIPositionDrivenByCode = false;
        Client::IRuntimeWorldUI_EditorTarget* selectedRuntimeWorldUITarget = nullptr;
        string selectedRuntimeWorldUIGroup{};
        string selectedRuntimeWorldUIDisplayName{};
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
