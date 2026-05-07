#include "BT_VIew.h"
#include "BT_Inspector.h"

BT_Inspector::BT_Inspector() 
{
}

BT_Inspector::~BT_Inspector()
{
    Free();
}

void BT_Inspector::Draw_Inspector(EditorNode* node)
{
    if (node == nullptr)
    {
        ImGui::Text("No node selected");
        return;
    }

    ImGui::Text("Selected Node");
    ImGui::Separator();

    if (ImGui::InputText("Name", node->titleBuffer, sizeof(node->titleBuffer)))
    {
        node->title = node->titleBuffer;
    }

    ImGui::Text("Type: %d", static_cast<int>(node->type));
    ImGui::Separator();

    switch (node->type)
    {
    case NodeType::Selector:
        ImGui::Text("Selector Node");
        break;

    case NodeType::Sequence:
        ImGui::Text("Sequence Node");
        break;

    case NodeType::Task:
    {
        ImGui::Text("Task Node");
        static int damage = 10;
        ImGui::InputInt("Damage", &damage);
        break;
    }

    case NodeType::Decorator:
    {
        ImGui::Text("Decorator Node");
        static bool flag = true;
        ImGui::Checkbox("Condition", &flag);
        break;
    }

    default:
        break;
    }
}
Shared<BT_Inspector> BT_Inspector::Create()
{
    return make_shared<BT_Inspector>();
}

void BT_Inspector::Free()
{
}
