#include "pch.h"
#include "LogWindow.h"
#include "GameInstance.h"
#include "Log_Manager.h"

HRESULT LogWindow::Initialize()
{
    return EditorBlock::Initialize();
}

void LogWindow::Update(float timeDelta)
{
    
}

void LogWindow::Render()
{
    static bool autoScroll = true;

    if (!_showBlock) return;
    if (!ImGui::Begin("Log Console", &_showBlock))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Clear"))
        GAME->Clear_ConsoleLogs();

    ImGui::SameLine();
    ImGui::Checkbox("Auto-Scroll", &autoScroll);
    ImGui::Separator();

    ImGui::BeginChild("LogScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_None);

    ImGui::PushTextWrapPos(0.0f);

    GAME->Visit_ConsoleLogs([&](const Engine::ConsoleLogEntry& entry)
        {
            ImVec4 color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f); 
            
            switch (entry.logLevel)
            {
            case LogLevel::Warn:     color = ImVec4(1.0f, 0.8f, 0.0f, 1.0f); break; // Yellow
            case LogLevel::Error:    color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); break; // Red
            case LogLevel::Critical: color = ImVec4(1.0f, 0.0f, 1.0f, 1.0f); break; // Magenta
            case LogLevel::Debug:    color = ImVec4(0.4f, 0.7f, 1.0f, 1.0f); break; // Blue
            default:
                break;
            }
            ImGui::TextColored(color, "[%d] %s", entry.sequence, entry.formattedLine.c_str());
        });
    if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::End();
}

Shared<LogWindow> LogWindow::Create()
{
    auto prototype = make_shared<LogWindow>();

    if (FAILED(prototype->Initialize()))
    {
        MSG_BOX("Failed to Created : LogWindow");
        return nullptr;
    }

    return prototype;
}

