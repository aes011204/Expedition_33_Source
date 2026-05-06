#include "pch.h"
#include "UI_Editor_Manager.h"

namespace
{
    const char* Get_ObjectTypeName(UI_ObjectType objectType)
    {
        switch (objectType)
        {
        case UI_ObjectType::Image:
            return "Image";
        case UI_ObjectType::Text:
            return "Text";
        default:
            return "Unknown";
        }
    }

    ImU32 Get_ObjectFillColor(UI_ObjectType objectType)
    {
        switch (objectType)
        {
        case UI_ObjectType::Image:
            return IM_COL32(76, 125, 194, 110);
        case UI_ObjectType::Text:
            return IM_COL32(104, 178, 120, 110);
        default:
            return IM_COL32(180, 180, 180, 110);
        }
    }
}

HRESULT UI_Editor_Manager::Initialize()
{
    return S_OK;
}

void UI_Editor_Manager::Render()
{
    Render_DockSpace();
    Render_Hierarchy();
    Render_Viewport();
    Render_Inspector();
}

void UI_Editor_Manager::Render_DockSpace()
{
    ImGuiViewport* pViewport = ImGui::GetMainViewport();
    if (nullptr == pViewport)
        return;

    ImGui::SetNextWindowPos(pViewport->WorkPos);
    ImGui::SetNextWindowSize(pViewport->WorkSize);
    ImGui::SetNextWindowViewport(pViewport->ID);

    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    WindowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));

    bool isOpen = true;
    ImGui::Begin("UIEditorDockSpace", &isOpen, WindowFlags);
    ImGui::PopStyleVar(3);

    const ImGuiID DockSpaceId = ImGui::GetID("UIEditorDockSpaceId");
    ImGui::DockSpace(DockSpaceId, ImVec2(0.f, 0.f), ImGuiDockNodeFlags_None);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New"))
            {

            }
            if (ImGui::MenuItem("Save"))
            {

            }
            if (ImGui::MenuItem("Load"))
            {

            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }


    ImGui::End();
}

void UI_Editor_Manager::Render_Hierarchy()
{
    ImGui::Begin("Hierarchy");

    const vector<UI_Object>& objects = _document.Get_Objects();
    for (const UI_Object& object : objects)
    {
        const bool isSelected = (_selectedObjectId == object.id);
        const string itemLabel = object.name + "##UIObject" + to_string(object.id);

        if (ImGui::Selectable(itemLabel.c_str(), isSelected))
            _selectedObjectId = object.id;
    }

    ImGui::End();
}

void UI_Editor_Manager::Render_Viewport()
{
    ImGui::Begin("Viewport");
    ImVec2 available = ImGui::GetContentRegionAvail();
    available.x = max(1.f, available.x);
    available.y = max(1.f, available.y);

    const ImVec2 canvasSize = _document.Get_CanvasSize();

    float scaleX = available.x / canvasSize.x;
    float scaleY = available.y / canvasSize.y;
    float scale = min(scaleX, scaleY);

    const float gridSize = 16.f;

    ImVec2 drawSize = ImVec2(canvasSize.x * scale, canvasSize.y * scale);
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasMax = ImVec2(canvasPos.x + drawSize.x, canvasPos.y + drawSize.y);

    const ImU32 canvasColor = IM_COL32(26, 28, 32, 255);
    const ImU32 gridColor   = IM_COL32(52, 56, 64, 160);
    const ImU32 boaderColor = IM_COL32(92, 100, 112, 255);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(canvasPos, canvasMax, canvasColor);

    for (float x = 0.f; x <= canvasSize.x; x += gridSize)
    {
        float screenX = canvasPos.x + x * scale;
        drawList->AddLine(ImVec2(screenX, canvasPos.y), ImVec2(screenX, canvasMax.y), gridColor);
    }
    for (float y = 0.f; y <= canvasSize.y; y += gridSize)
    {
        float screenY = canvasPos.y + y * scale;
        drawList->AddLine(ImVec2(canvasPos.x, screenY), ImVec2(canvasMax.x, screenY), gridColor);
    }

    const vector<UI_Object>& objects = _document.Get_Objects();
    for (const UI_Object& object : objects)
    {
        const ImVec2 objectSize = ImVec2(object.size.x * object.scale.x, object.size.y * object.scale.y);
        const ImVec2 objectMin = ImVec2(canvasPos.x + object.position.x * scale, canvasPos.y + object.position.y * scale);
        const ImVec2 objectMax = ImVec2(objectMin.x + objectSize.x * scale, objectMin.y + objectSize.y * scale);
        const bool isSelected = (_selectedObjectId == object.id);

        const ImU32 fillColor = Get_ObjectFillColor(object.type);
        const ImU32 objectBorderColor = isSelected ? IM_COL32(255, 217, 102, 255) : IM_COL32(210, 214, 220, 220);
        const float borderThickness = isSelected ? 2.5f : 1.5f;

        drawList->AddRectFilled(objectMin, objectMax, fillColor);
        drawList->AddRect(objectMin, objectMax, objectBorderColor, 0.f, 0, borderThickness);

        if (objectMax.x - objectMin.x > 32.f && objectMax.y - objectMin.y > 18.f)
            drawList->AddText(ImVec2(objectMin.x + 4.f, objectMin.y + 3.f), IM_COL32(245, 247, 250, 230), object.name.c_str());
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        const ImVec2 mousePos = ImGui::GetMousePos();
        const bool isInsideCanvas =
            mousePos.x >= canvasPos.x && mousePos.x <= canvasMax.x &&
            mousePos.y >= canvasPos.y && mousePos.y <= canvasMax.y;

        if (isInsideCanvas)
        {
            ObjectId pickedObjectId = 0;
            const ImVec2 mouseCanvasPos = ImVec2((mousePos.x - canvasPos.x) / scale, (mousePos.y - canvasPos.y) / scale);

            for (auto iter = objects.rbegin(); iter != objects.rend(); ++iter)
            {
                const UI_Object& object = *iter;
                const ImVec2 objectSize = ImVec2(object.size.x * object.scale.x, object.size.y * object.scale.y);
                const bool isInsideObject =
                    mouseCanvasPos.x >= object.position.x && mouseCanvasPos.x <= object.position.x + objectSize.x &&
                    mouseCanvasPos.y >= object.position.y && mouseCanvasPos.y <= object.position.y + objectSize.y;

                if (isInsideObject)
                {
                    pickedObjectId = object.id;
                    break;
                }
            }

            _selectedObjectId = pickedObjectId;
        }
    }

    drawList->AddRect(canvasPos, canvasMax, boaderColor);

    ImGui::Dummy(drawSize);
    ImGui::TextDisabled("Canvas %.0f x %.0f / scale %.2f", canvasSize.x, canvasSize.y, scale);

    ImGui::End();
}

void UI_Editor_Manager::Render_Inspector()
{
    ImGui::Begin("Inspector");

    UI_Object* selectedObject = _document.Find_Object(_selectedObjectId);
    if (nullptr != selectedObject)
    {
        ImGui::Text("ID: %u", selectedObject->id);
        ImGui::Text("Name: %s", selectedObject->name.c_str());
        ImGui::Text("Type: %s", Get_ObjectTypeName(selectedObject->type));
        ImGui::Separator();

        ImGui::DragFloat2("Position", &selectedObject->position.x, 1.f);
        ImGui::DragFloat2("Size", &selectedObject->size.x, 1.f, 1.f, 4096.f);
        ImGui::DragFloat2("Scale", &selectedObject->scale.x, 0.01f, 0.01f, 10.f, "%.2f");
        ImGui::DragFloat("Rotation", &selectedObject->rotationRadians, 0.01f, -6.283f, 6.283f, "%.3f");
        ImGui::DragInt("Sort Order", &selectedObject->sortOrder, 1.f);
        ImGui::ColorEdit4("Color", &selectedObject->color.x);

        selectedObject->size.x = max(1.f, selectedObject->size.x);
        selectedObject->size.y = max(1.f, selectedObject->size.y);
        selectedObject->scale.x = max(0.01f, selectedObject->scale.x);
        selectedObject->scale.y = max(0.01f, selectedObject->scale.y);

        if (!selectedObject->text.empty())
            ImGui::TextWrapped("Text: %s", selectedObject->text.c_str());
        if (!selectedObject->fontTag.empty())
            ImGui::Text("Font: %s", selectedObject->fontTag.c_str());
        if (!selectedObject->texturePath.empty())
            ImGui::TextWrapped("Texture: %s", selectedObject->texturePath.c_str());
    }

    ImGui::End();
}

Unique<UI_Editor_Manager> UI_Editor_Manager::Create()
{
    Unique<UI_Editor_Manager> pInstance{new UI_Editor_Manager()};

    if (FAILED(pInstance->Initialize()))
        return nullptr;

    return pInstance;
}
