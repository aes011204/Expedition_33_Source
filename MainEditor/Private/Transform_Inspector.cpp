#include "Transform_Inspector.h"

#include "Component.h"
#include "TransformCom.h"

void Transform_Inspector::Draw_Inspector(const Shared<Component>& component)
{
    Shared<TransformCom> transform = dynamic_pointer_cast<TransformCom>(component);
    if (transform == nullptr)
        return;

    if (!Draw_Header("Transform"))
        return;

    ImGui::Spacing();
    Draw_SpaceSelector(_space);
    ImGui::Spacing();

    const bool isWorld = (_space == ETransformSpace::World);

    {
        Vec3 position = isWorld ? transform->Get_WorldPosition() : transform->Get_Position();

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Position");
        ImGui::SameLine(72.f);

        if (Draw_XYZRow("Position", position, 0.1f, "%.2f"))
        {
            if (isWorld)
                transform->Set_WorldPosition(position);
            else
                transform->Set_Position(position);
        }
    }

    ImGui::Spacing();

    {
        Vec3 rotation = isWorld ? transform->Get_WorldRotationEuler() : transform->Get_RotationEuler();

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Rotation");
        ImGui::SameLine(72.f);

        if (Draw_XYZRow("Rotation", rotation, 0.5f, "%.1f"))
        {
            if (isWorld)
                transform->Set_WorldRotationEuler(rotation);
            else
                transform->Set_RotationEuler(rotation);
        }
    }

    ImGui::Spacing();

    {
        Vec3 scale = transform->Get_Scale();

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Scale");
        ImGui::SameLine(72.f);

        if (Draw_XYZRow("Scale", scale, 0.01f, "%.3f"))
            transform->Set_Scale(scale);
    }

    ImGui::Spacing();
}

bool Transform_Inspector::Draw_SpaceSelector(ETransformSpace& space)
{
    bool changed = false;

    const char* currentLabel =
        (space == ETransformSpace::World) ? "World" : "Local";

    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Space");
    ImGui::SameLine(72.f);

    ImGui::SetNextItemWidth(110.f);
    if (ImGui::BeginCombo("##TransformSpace", currentLabel))
    {
        const bool localSelected = (space == ETransformSpace::Local);
        if (ImGui::Selectable("Local", localSelected))
        {
            space = ETransformSpace::Local;
            changed = true;
        }

        const bool worldSelected = (space == ETransformSpace::World);
        if (ImGui::Selectable("World", worldSelected))
        {
            space = ETransformSpace::World;
            changed = true;
        }

        ImGui::EndCombo();
    }

    return changed;
}

bool Transform_Inspector::XYZ_DragFloat(
    const char* label,
    const ImVec4& color,
    float& value,
    float speed,
    const char* uniqueId,
    float fieldWidth,
    const char* format)
{
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);

    ImGui::Button(label, ImVec2(20.f, ImGui::GetFrameHeight()));

    ImGui::PopStyleColor(3);
    ImGui::SameLine(0.f, 2.f);

    ImGui::SetNextItemWidth(fieldWidth);

    const string dragId = string("##") + uniqueId;
    return ImGui::DragFloat(dragId.c_str(), &value, speed, 0.f, 0.f, format);
}

bool Transform_Inspector::Draw_XYZRow(
    const char* id,
    Vec3& value,
    float speed,
    const char* format)
{
    bool changed = false;

    ImGui::PushID(id);

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float columnWidth = (availableWidth - 8.f) / 3.f;
    const float dragWidth = columnWidth - 22.f;

    changed |= XYZ_DragFloat("X", ImVec4(0.75f, 0.18f, 0.18f, 1.f), value.x, speed, "X", dragWidth, format);
    ImGui::SameLine(0.f, 4.f);

    changed |= XYZ_DragFloat("Y", ImVec4(0.25f, 0.60f, 0.25f, 1.f), value.y, speed, "Y", dragWidth, format);
    ImGui::SameLine(0.f, 4.f);

    changed |= XYZ_DragFloat("Z", ImVec4(0.20f, 0.40f, 0.75f, 1.f), value.z, speed, "Z", dragWidth, format);

    ImGui::PopID();

    return changed;
}

Shared<Transform_Inspector> Transform_Inspector::Create()
{
    return make_shared<Transform_Inspector>();
}
