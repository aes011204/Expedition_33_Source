#include "Game_View.h"

#include "Editor_ImGuiInclude.h"
#include "Utils.h"

Game_View::Game_View()
    : EditorWindow{ L"Game" }
{
}

void Game_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string windowName = Utils::ToString(Get_WindowName());
    
    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        ImGui::TextUnformatted("Game viewport will display the play-mode render target here.");
    }
    Set_Open(isOpen);
    ImGui::End();
}



