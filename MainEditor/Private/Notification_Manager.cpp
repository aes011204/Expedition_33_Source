#include "Notification_Manager.h"

Notification_Manager::Notification_Manager()
{
}

Notification_Manager::~Notification_Manager()
{
}

void Notification_Manager::Initialize()
{
    _notifications.clear();
}

void Notification_Manager::Update(float timeDelta)
{
    if (_notifications.empty())
        return;

    for (auto it = _notifications.begin(); it != _notifications.end();)
    {
        it->timer += timeDelta;

        if (it->timer > it->duration - 0.5f)
        {
            float remaining = it->duration - it->timer;
            it->alpha = clamp(remaining / 0.5f, 0.0f, 1.0f);
        }
        if (it->timer >= it->duration)
        {
            it = _notifications.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void Notification_Manager::Render()
{
    if (_notifications.empty())
        return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 workPos = viewport->WorkPos;
    ImVec2 workSize = viewport->WorkSize;
    float PAD = 10.0f;

    // 우측 하단에서 시작할 위치
    ImVec2 windowPos;
    windowPos.x = workPos.x + workSize.x - PAD;
    windowPos.y = workPos.y + workSize.y - PAD;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoInputs;

    int id = 0;
    for (auto& note : _notifications)
    {
        ImGui::SetNextWindowBgAlpha(0.8f * note.alpha);

        ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(1.0f, 1.0f));

        string windowName = "##Notify_" + to_string(id++);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, note.alpha);

        ImVec4 textColor = ImVec4(1, 1, 1, 1); 
        switch (note.type)
        {
        case NotifyType::Success: textColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f); break; // Green
        case NotifyType::Warning: textColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f); break; // Yellow
        case NotifyType::Error:   textColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); break; // Red
        }
        if (ImGui::Begin(windowName.c_str(), nullptr, flags))
        {
            ImGui::TextColored(textColor, note.message.c_str());

            float height = ImGui::GetWindowHeight();
            windowPos.y -= (height + PAD);
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }
}

void Notification_Manager::Add_Internal(const string& message, NotifyType type)
{
    FNotification note;
    note.message = message;
    note.type = type;

    note.duration = 3.0f;
    note.timer = 0.0f;
    note.alpha = 1.0f;

    _notifications.push_back(note);

    LOG_INFO(message);
}

unique_ptr<Notification_Manager> Notification_Manager::Create()
{
    auto instance = make_unique<Notification_Manager>();

    instance->Initialize();

    return instance;
}
