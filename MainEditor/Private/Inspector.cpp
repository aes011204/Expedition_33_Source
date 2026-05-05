#include "Inspector.h"

#include "Component.h"
#include "Component_Inspector.h"
#include "Editor_Context.h"
#include "EditorInstance.h"
#include "GameObject.h"
#include "Inspector_Factory.h"
#include "TransformCom.h"

Inspector::Inspector()
    : Editor_Window{ L"Inspector" }
{
}

void Inspector::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    if (ImGui::Begin("Inspector", &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

        Shared<GameObject> selectedObject = EDITOR->Selected_Object();
        if (selectedObject == nullptr)
        {
            ImGui::TextUnformatted("No Selected");
            ImGui::Separator();
            ImGui::TextDisabled("Inspector");
            Set_Open(isOpen);
            ImGui::End();
            return;
        }

        Draw_Components(selectedObject);
    }

    Set_Open(isOpen);
    ImGui::End();
}

void Inspector::Draw_Component(const string& key, const Shared<Component>& component)
{
    if (component == nullptr)
        return;

    Shared<Component_Inspector> inspector = EDITOR->Find_Inspector(key);
    if (inspector == nullptr)
        return;

    inspector->Draw_Inspector(component);
}

void Inspector::Draw_Components(const Shared<GameObject>& target)
{
    CHECK_NULL(target);

    Shared<TransformCom> transform = target->Get_Transform();
    if (transform == nullptr)
    {
        ImGui::TextDisabled("Transform Component 없음");
        return;
    }

    Draw_Component("Transform", static_pointer_cast<Component>(transform));
}

Shared<Inspector> Inspector::Create()
{
    return make_shared<Inspector>();
}
