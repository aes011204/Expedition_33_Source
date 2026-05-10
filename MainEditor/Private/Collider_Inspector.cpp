#include "Collider_Inspector.h"

#include "ColliderCom.h"

void Collider_Inspector::Draw_Inspector(const Shared<Component>& component)
{
    Shared<ColliderCom> collider = dynamic_pointer_cast<ColliderCom>(component);
    if (collider == nullptr)
        return;

    if (!Draw_Header("Collider"))
        return;

    bool isActive = collider->Get_IsActive();
    if (ImGui::Checkbox("Render (Is Active)", &isActive))
        collider->Set_IsActive(isActive);

    ImGui::SameLine();
    bool isColl = collider->Get_IsColl();
    ImGui::BeginDisabled();
    ImGui::Checkbox("Is Coll", &isColl);
    ImGui::EndDisabled();

    int presetIndex = static_cast<int>(collider->Get_CollisionPreset());
    if (presetIndex < 0 || presetIndex >= static_cast<int>(Collision_Preset::END))
        presetIndex = 0;

    const string currentPresetLabel = Get_PresetLabel(static_cast<Collision_Preset>(presetIndex));
    if (ImGui::BeginCombo("Collision Preset", currentPresetLabel.c_str()))
    {
        for (int i = 0; i < static_cast<int>(Collision_Preset::END); ++i)
        {
            const Collision_Preset preset = static_cast<Collision_Preset>(i);
            const bool isSelected = (presetIndex == i);
            const string presetLabel = Get_PresetLabel(preset);

            if (ImGui::Selectable(presetLabel.c_str(), isSelected))
                collider->Set_CollisionPreset(preset);

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    ImGui::TextDisabled("Channel");
    ImGui::SameLine(120.f);
    const string channelLabel = Get_ChannelLabel(collider->Get_Channel());
    ImGui::TextUnformatted(channelLabel.c_str());

    ImGui::TextDisabled("Overlap Mask");
    ImGui::SameLine(120.f);
    ImGui::Text("0x%08X", collider->Get_OverlapMask());

    ImGui::TextDisabled("Block Mask");
    ImGui::SameLine(120.f);
    ImGui::Text("0x%08X", collider->Get_BlockMask());

    ImGui::TextDisabled("PhysX");
    ImGui::SameLine(120.f);
    ImGui::Text("%s%s",
        collider->Is_PhysXRegistered() ? "Registered" : "Not Registered",
        collider->Is_Trigger() ? " / Trigger" : "");

    ImGui::Separator();

    ColliderShapeDesc shapeDesc = collider->Get_ShapeDesc();
    shapeDesc.shape = collider->Get_Shape();

    const string shapeLabel = Get_ShapeLabel(shapeDesc.shape);
    ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), "Shape : %s", shapeLabel.c_str());

    bool changed = false;

    switch (shapeDesc.shape)
    {
    case CollisionShape::AABB:
        changed |= ImGui::DragFloat3("Center##AABB", &shapeDesc.center.x, 0.01f);
        changed |= ImGui::DragFloat3("Extents##AABB", &shapeDesc.extents.x, 0.01f, 0.01f, FLT_MAX);
        break;

    case CollisionShape::OBB:
        changed |= ImGui::DragFloat3("Center##OBB", &shapeDesc.center.x, 0.01f);
        changed |= ImGui::DragFloat3("Extents##OBB", &shapeDesc.extents.x, 0.01f, 0.01f, FLT_MAX);
        changed |= ImGui::DragFloat3("Radians##OBB", &shapeDesc.radians.x, 0.01f);
        break;

    case CollisionShape::Sphere:
        changed |= ImGui::DragFloat3("Center##Sphere", &shapeDesc.center.x, 0.01f);
        changed |= ImGui::DragFloat("Radius##Sphere", &shapeDesc.radius, 0.01f, 0.01f, FLT_MAX);
        break;

    default:
        ImGui::TextDisabled("Unsupported collider shape.");
        break;
    }

    if (changed)
        collider->Set_ShapeDesc(shapeDesc);
}

string Collider_Inspector::Get_ShapeLabel(CollisionShape shape)
{
    const string label = string(magic_enum::enum_name(shape));
    return label.empty() ? "Unknown" : label;
}

string Collider_Inspector::Get_ChannelLabel(Collision_Channel channel)
{
    const string label = string(magic_enum::enum_name(channel));
    return label.empty() ? "Unknown" : label;
}

string Collider_Inspector::Get_PresetLabel(Collision_Preset preset)
{
    const string label = string(magic_enum::enum_name(preset));
    return label.empty() ? "Unknown" : label;
}

Shared<Collider_Inspector> Collider_Inspector::Create()
{
    return make_shared<Collider_Inspector>();
}
