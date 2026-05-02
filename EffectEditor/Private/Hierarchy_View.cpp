#include "Hierarchy_View.h"

Hierarchy_View::Hierarchy_View()
    : Editor_Window{ L"Hierarchy" }
{
}

Hierarchy_View::~Hierarchy_View()
{
    Free();
}

void Hierarchy_View::Update(float timeDelta)
{
}

void Hierarchy_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::BulletText("No scene objects registered yet");
            ImGui::TreePop();
        }
    }
    Set_Open(isOpen);
    ImGui::End();
}

Shared<Hierarchy_View> Hierarchy_View::Create()
{
    return make_shared<Hierarchy_View>();
}

void Hierarchy_View::Free()
{
    __super::Free();
}



