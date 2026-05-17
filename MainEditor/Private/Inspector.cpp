#include "Inspector.h"

#include "Component.h"
#include "Component_Inspector.h"
#include "Editor_Context.h"
#include "EditorInstance.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "Helper_String.h"
#include "Inspector_Factory.h"
#include "ModelCom.h"
#include "Reflection_Types.h"
#include "TransformCom.h"

const vector<Engine::BTBlackboardKeyView>* Inspector::_btBlackboardKeys = nullptr;


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
        if (_isPrimaryShadowLightTarget && selectedObject != nullptr)
            _isPrimaryShadowLightTarget = false;

        if (_isPrimaryShadowLightTarget)
        {
            Draw_PrimaryShadowLightInspector();
            Set_Open(isOpen);
            ImGui::End();
            return;
        }

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

void Inspector::Set_PrimaryShadowLightTarget(bool enabled)
{
    _isPrimaryShadowLightTarget = enabled;

    if (enabled)
        EDITOR->Clear_SelectedObject();
}

void Inspector::Draw_PrimaryShadowLightInspector()
{
    const LIGHT_DESC* sourceLight = GAME->Get_PrimaryShadowLightDesc();
    if (sourceLight == nullptr && GAME->Get_LightCount() > 0)
        sourceLight = GAME->Get_LightDesc(0);

    const ShadowLightDesc* sourceShadow = GAME->Get_ShadowLightDesc();

    ImGui::TextUnformatted("Name: Primary Shadow Light");
    ImGui::Separator();

    if (sourceLight == nullptr)
    {
        ImGui::TextDisabled("현재 레벨에 Light가 없음");
        return;
    }

    if (sourceShadow == nullptr)
    {
        ImGui::TextDisabled("Shadow 시스템이 초기화되지 않음");
        return;
    }

    LIGHT_DESC lightDesc = *sourceLight;
    ShadowLightDesc shadowDesc = *sourceShadow;

    bool lightChanged = false;
    bool shadowChanged = false;

    if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Type");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::TextUnformatted(lightDesc.type == LightType::Directional ? "Directional" : "Non-Directional");

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Cast Shadow");
        ImGui::SameLine(InspectorLabelWidth);
        lightChanged |= ImGui::Checkbox("##CastShadow", &lightDesc.castShadow);

        float direction[3] = { lightDesc.direction.x, lightDesc.direction.y, lightDesc.direction.z };
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Direction");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::DragFloat3("##LightDirection", direction, 0.01f))
        {
            lightDesc.direction.x = direction[0];
            lightDesc.direction.y = direction[1];
            lightDesc.direction.z = direction[2];
            lightChanged = true;
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Diffuse");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        lightChanged |= ImGui::ColorEdit4("##LightDiffuse", &lightDesc.diffuse.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Ambient");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        lightChanged |= ImGui::ColorEdit4("##LightAmbient", &lightDesc.ambient.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Specular");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        lightChanged |= ImGui::ColorEdit4("##LightSpecular", &lightDesc.specular.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar);
    }

    if (ImGui::CollapsingHeader("Shadow", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Eye");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        shadowChanged |= ImGui::DragFloat3("##ShadowEye", &shadowDesc.eye.x, 0.1f);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Target");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        shadowChanged |= ImGui::DragFloat3("##ShadowTarget", &shadowDesc.at.x, 0.1f);

        float fovyDegree = DirectX::XMConvertToDegrees(shadowDesc.fovy);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("FOV Y");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::DragFloat("##ShadowFovY", &fovyDegree, 0.1f, 1.f, 179.f, "%.2f deg"))
        {
            shadowDesc.fovy = DirectX::XMConvertToRadians(fovyDegree);
            shadowChanged = true;
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Aspect");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        shadowChanged |= ImGui::DragFloat("##ShadowAspect", &shadowDesc.aspect, 0.01f, 0.1f, 10.f);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Near / Far");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        float nearFar[2] = { shadowDesc.zn, shadowDesc.zf };
        if (ImGui::DragFloat2("##ShadowNearFar", nearFar, 0.1f, 0.001f, 10000.f))
        {
            shadowDesc.zn = nearFar[0];
            shadowDesc.zf = nearFar[1];
            shadowChanged = true;
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Bias");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        shadowChanged |= ImGui::DragFloat("##ShadowBias", &shadowDesc.bias, 0.00005f, 0.f, 0.01f, "%.6f");

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Strength");
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        shadowChanged |= ImGui::DragFloat("##ShadowIntensity", &shadowDesc.intensity, 0.01f, 0.f, 1.f);
    }

    if (lightChanged)
        GAME->Update_PrimaryShadowLightDesc(lightDesc);

    if (shadowChanged)
        GAME->Update_ShadowLight(shadowDesc);
}

bool Inspector::Draw_ComponentOverride(const Shared<Component>& component)
{
    if (component == nullptr)
        return false;

    const ClassReflectionInfo& info = component->Get_ReflectionInfo();

    string inspectorKey = info.className;
    if (dynamic_pointer_cast<ModelCom>(component))
        inspectorKey = "ModelCom";

    if (inspectorKey.empty())
        inspectorKey = String::ToString(component->Get_Name());

    Shared<Component_Inspector> inspector = EDITOR->Find_Inspector(inspectorKey);
    if (inspector == nullptr)
        return false;

    inspector->Draw_Inspector(component);
    return true;
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

    Draw_ComponentOverride(static_pointer_cast<Component>(transform));
    Draw_ObjectReflection(target);

    target->Visit_Components([&](const wstring&, const Shared<Component>& component)
    {
        if (component == nullptr)
            return;

        // 트랜스폼은 무조건 맨위에 그릴거라 여기서 스킵
        if (component == transform)
            return;

        if (Draw_ComponentOverride(component))
            return;

        const ClassReflectionInfo& info = component->Get_ReflectionInfo();
        if (info.properties.empty())
            return;

        Draw_ReflectionSection(info.className, component.get(), info);
    });
}

void Inspector::Draw_ObjectReflection(const Shared<GameObject>& target)
{
    CHECK_NULL(target);

    const ClassReflectionInfo& info = target->Get_ReflectionInfo();
    if (info.properties.empty())
        return;

    Draw_ReflectionSection(info.className, target.get(), info);
}

bool Inspector::Draw_Vec2PropertyRow(const string& idPrefix, const PropertyInfo& property, Vec2& value)
{
    bool changed = false;

    ImGui::PushID(idPrefix.c_str());

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float columnWidth = (availableWidth - 4.f) / 2.f;
    const float dragWidth = columnWidth - 22.f;

    static const ImVec4 colorX(0.75f, 0.18f, 0.18f, 1.f);
    static const ImVec4 colorY(0.25f, 0.60f, 0.25f, 1.f);

    changed |= Draw_VecAxis("X", colorX, value.x, property.dragSpeed, idPrefix + "_X", dragWidth);
    ImGui::SameLine(0.f, 4.f);
    changed |= Draw_VecAxis("Y", colorY, value.y, property.dragSpeed, idPrefix + "_Y", dragWidth);

    ImGui::PopID();
    return changed;
}

void Inspector::Draw_ReflectionSection(
    const string& headerId,
    void* instance,
    const ClassReflectionInfo& info)
{
    if (instance == nullptr || info.properties.empty())
        return;

    const char* sectionLabel =
        info.displayName.empty() ? info.className.c_str() : info.displayName.c_str();

    if (!ImGui::CollapsingHeader(sectionLabel, ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::PushID(headerId.c_str());
    Draw_ReflectionProperties(instance, info);
    ImGui::PopID();
}

void Inspector::Draw_ReflectionProperties(
    void* instance,
    const ClassReflectionInfo& info)
{
    for (const PropertyInfo& property : info.properties)
    {
        Draw_ReflectionProperty(info.className, instance, property);
        ImGui::Spacing();
    }
}

void Inspector::Draw_ReflectionProperty(
    const string& idPrefix,
    void* instance,
    const PropertyInfo& property)
{
    char* base = reinterpret_cast<char*>(instance);
    void* address = base + property.offset;

    const string visibleLabel =
        property.displayName.empty() ? property.name : property.displayName;
    const string hiddenId = "##" + idPrefix + "_" + property.name;

    switch (property.type)
    {
    case PropertyType::Bool:
    {
        bool* value = static_cast<bool*>(address);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::Checkbox(hiddenId.c_str(), value);
        break;
    }

    case PropertyType::Int:
    {
        int* value = static_cast<int*>(address);
        const bool useClamp = !(property.minVal == 0.f && property.maxVal == 0.f);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

        if (useClamp)
        {
            const int minValue = static_cast<int>(property.minVal);
            const int maxValue = static_cast<int>(property.maxVal);
            ImGui::DragInt(hiddenId.c_str(), value, property.dragSpeed, minValue, maxValue);
        }
        else
        {
            ImGui::DragInt(hiddenId.c_str(), value, property.dragSpeed);
        }
        break;
    }

    case PropertyType::Float:
    {
        float* value = static_cast<float*>(address);
        const bool useClamp = !(property.minVal == 0.f && property.maxVal == 0.f);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

        if (useClamp)
            ImGui::DragFloat(hiddenId.c_str(), value, property.dragSpeed, property.minVal, property.maxVal);
        else
            ImGui::DragFloat(hiddenId.c_str(), value, property.dragSpeed);

        break;
    }

    case PropertyType::Vec2:
    {
        Vec2* value = static_cast<Vec2*>(address);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);

        Draw_Vec2PropertyRow(idPrefix + "_" + property.name, property, *value);
        break;
    }

    case PropertyType::Vec3:
    {
        Vec3* value = static_cast<Vec3*>(address);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);

        Draw_Vec3PropertyRow(idPrefix + "_" + property.name, property, *value);
        break;
    }

    case PropertyType::Vec4:
    {
        Vec4* value = static_cast<Vec4*>(address);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);

        Draw_Vec4PropertyRow(idPrefix + "_" + property.name, property, *value);
        break;
    }

    case PropertyType::Color:
    {
        Vec4* value = static_cast<Vec4*>(address);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);

        Draw_ColorPropertyRow(idPrefix + "_" + property.name, *value);
        break;
    }

    case PropertyType::String:
    {
        std::string* value = static_cast<std::string*>(address);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);

        if (property.isBlackboardKey && _btBlackboardKeys != nullptr)
        {
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

            const char* preview = value->empty() ? "(None)" : value->c_str();

            if (ImGui::BeginCombo(hiddenId.c_str(), preview))
            {
                for (const auto& key : *_btBlackboardKeys)
                {
                    if (key.type != property.blackboardValueType)
                        continue;

                    const bool isSelected = (*value == key.name);

                    if (ImGui::Selectable(key.name.c_str(), isSelected))
                        *value = key.name;

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::TextUnformatted(value->c_str());
            ImGui::EndDisabled();
        }
        break;
    }

    case PropertyType::Enum:
    {
        int* value = static_cast<int*>(address);

        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);

        Draw_EnumPropertyRow(idPrefix + "_" + property.name, property, *value);
        break;
    }

    case PropertyType::ReadOnly:
    default:
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", visibleLabel.c_str());
        ImGui::SameLine(InspectorLabelWidth);
        ImGui::BeginDisabled();
        ImGui::TextUnformatted("Unsupported");
        ImGui::EndDisabled();
        break;
    }
    }
}


bool Inspector::Draw_Vec3PropertyRow(
    const string& idPrefix,
    const PropertyInfo& property,
    Vec3& value)
{
    bool changed = false;

    ImGui::PushID(idPrefix.c_str());

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float columnWidth = (availableWidth - 8.f) / 3.f;
    const float dragWidth = columnWidth - 22.f;

    static const ImVec4 colorX(0.75f, 0.18f, 0.18f, 1.f);
    static const ImVec4 colorY(0.25f, 0.60f, 0.25f, 1.f);
    static const ImVec4 colorZ(0.20f, 0.40f, 0.75f, 1.f);

    changed |= Draw_VecAxis("X", colorX, value.x, property.dragSpeed, idPrefix + "_X", dragWidth);
    ImGui::SameLine(0.f, 4.f);

    changed |= Draw_VecAxis("Y", colorY, value.y, property.dragSpeed, idPrefix + "_Y", dragWidth);
    ImGui::SameLine(0.f, 4.f);

    changed |= Draw_VecAxis("Z", colorZ, value.z, property.dragSpeed, idPrefix + "_Z", dragWidth);

    ImGui::PopID();

    return changed;
}

bool Inspector::Draw_Vec4PropertyRow(const string& idPrefix, const PropertyInfo& property, Vec4& value)
{
    ImGui::PushID(idPrefix.c_str());

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const bool changed = ImGui::DragFloat4("##Vec4", &value.x, property.dragSpeed);

    ImGui::PopID();
    return changed;
}

bool Inspector::Draw_ColorPropertyRow(const string& idPrefix, Vec4& value)
{
    ImGui::PushID(idPrefix.c_str());

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

    const bool changed = ImGui::ColorEdit4(
        "##Color",
        &value.x,
        ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar);

    ImGui::PopID();
    return changed;
}

bool Inspector::Draw_EnumPropertyRow(const string& idPrefix, const PropertyInfo& property, int& value)
{
     ImGui::PushID(idPrefix.c_str());

    if (property.enumNames.empty() || property.enumValues.empty() || property.enumNames.size() != property.enumValues.size())
    {
        ImGui::BeginDisabled();
        ImGui::TextUnformatted("Invalid Enum Metadata");
        ImGui::EndDisabled();
        ImGui::PopID();
        return false;
    }

    int currentIndex = -1;
    for (int i = 0; i < static_cast<int>(property.enumValues.size()); ++i)
    {
        if (property.enumValues[i] == value)
        {
            currentIndex = i;
            break;
        }
    }

    const char* preview =
        (currentIndex >= 0 && currentIndex < static_cast<int>(property.enumNames.size()))
        ? property.enumNames[currentIndex].c_str()
        : "Unknown";

    bool changed = false;

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::BeginCombo("##Enum", preview))
    {
        for (int i = 0; i < static_cast<int>(property.enumNames.size()); ++i)
        {
            const bool isSelected = (currentIndex == i);

            if (ImGui::Selectable(property.enumNames[i].c_str(), isSelected))
            {
                value = property.enumValues[i];
                changed = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    ImGui::PopID();

    return changed;
}

bool Inspector::Draw_VecAxis(
    const char* axisLabel,
    const ImVec4& color,
    float& value,
    float speed,
    const string& uniqueId,
    float width)
{
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);

    ImGui::Button(axisLabel, ImVec2(20.f, ImGui::GetFrameHeight()));

    ImGui::PopStyleColor(3);
    ImGui::SameLine(0.f, 2.f);

    ImGui::SetNextItemWidth(width);
    const string dragId = "##" + uniqueId;

    return ImGui::DragFloat(dragId.c_str(), &value, speed);
}

void Inspector::Set_BTBlackboardKeys(const vector<Engine::BTBlackboardKeyView>* keys)
{
    _btBlackboardKeys = keys;
}

Shared<Inspector> Inspector::Create()
{
    return make_shared<Inspector>();
}
