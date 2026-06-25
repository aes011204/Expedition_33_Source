#include "PathTool.h"
#include "Scene_View.h"
#include "EditorInstance.h"
#include "GameInstance.h"
#include "PhysX_Types.h"

namespace
{
    static bool Try_MakeViewportRay(const POINT& pixelPos, Vec3& outOrigin, Vec3& outDirection)
    {
        const uint32 viewportWidth = GAME->Get_ViewportWidth();
        const uint32 viewportHeight = GAME->Get_ViewportHeight();
        if (viewportWidth == 0 || viewportHeight == 0)
        {
            return false;
        }

        const Matrix* viewInverse = GAME->Get_Transform_Inverse(D3DTS::View);
        const Matrix* projInverse = GAME->Get_Transform_Inverse(D3DTS::Proj);
        if (viewInverse == nullptr || projInverse == nullptr)
        {
            return false;
        }

        const float texcoordX = (static_cast<float>(pixelPos.x) + 0.5f) / static_cast<float>(viewportWidth);
        const float texcoordY = (static_cast<float>(pixelPos.y) + 0.5f) / static_cast<float>(viewportHeight);

        Vec4 nearPoint(texcoordX * 2.f - 1.f, texcoordY * -2.f + 1.f, 0.f, 1.f);
        Vec4 farPoint(texcoordX * 2.f - 1.f, texcoordY * -2.f + 1.f, 1.f, 1.f);

        nearPoint = Vec4::Transform(nearPoint, *projInverse);
        farPoint = Vec4::Transform(farPoint, *projInverse);
        if (nearPoint.w != 0.f)
        {
            nearPoint /= nearPoint.w;
        }
        if (farPoint.w != 0.f)
        {
            farPoint /= farPoint.w;
        }

        nearPoint = Vec4::Transform(nearPoint, *viewInverse);
        farPoint = Vec4::Transform(farPoint, *viewInverse);

        outOrigin = Vec3(nearPoint.x, nearPoint.y, nearPoint.z);
        outDirection = Vec3(farPoint.x - nearPoint.x, farPoint.y - nearPoint.y, farPoint.z - nearPoint.z);
        if (outDirection.LengthSquared() <= 0.f)
        {
            return false;
        }

        outDirection.Normalize();
        return true;
    }

    static bool Try_SampleWorldPos(const POINT& pixelPos, Vec3& outWorldPos)
    {
        Vec3 rayOrigin{};
        Vec3 rayDirection{};
        if (Try_MakeViewportRay(pixelPos, rayOrigin, rayDirection))
        {
            PhysXQueryHit hit{};
            if (GAME->Raycast_Environment(rayOrigin, rayDirection, 10000.f, hit))
            {
                outWorldPos = hit.position;
                return true;
            }
        }

        if (FAILED(GAME->Update_Picking()))
        {
            return false;
        }

        return GAME->Pick_WorldPosition(pixelPos, outWorldPos);
    }

    static bool Try_WorldToScreen(const Vec3& worldPos, const ImVec2& vpMin, const ImVec2& vpMax, ImVec2& outScreen)
    {
        const Matrix* view = GAME->Get_Transform(D3DTS::View);
        const Matrix* proj = GAME->Get_Transform(D3DTS::Proj);
        if (view == nullptr || proj == nullptr)
        {
            return false;
        }

        const Matrix viewProj = (*view) * (*proj);
        Vec4 clip = Vec4::Transform(Vec4(worldPos.x, worldPos.y, worldPos.z, 1.f), viewProj);
        if (clip.w <= 0.f)
        {
            return false;
        }

        const float ndcX = clip.x / clip.w;
        const float ndcY = -clip.y / clip.w;

        outScreen.x = vpMin.x + (ndcX * 0.5f + 0.5f) * (vpMax.x - vpMin.x);
        outScreen.y = vpMin.y + (ndcY * 0.5f + 0.5f) * (vpMax.y - vpMin.y);
        return true;
    }
}

PathTool::PathTool()
    : Editor_Window{ L"Path Tool" }
{
}

void PathTool::Update(float timeDelta)
{
    Handle_PointPlacement();
}

void PathTool::Render()
{
    Render_PathOverlay();

    if (!Is_Open())
    {
        return;
    }

    bool isOpen = Is_Open();
    if (ImGui::Begin("Path Tool", &isOpen))
    {
        Render_PointList();
    }
    Set_Open(isOpen);
    ImGui::End();
}

void PathTool::Handle_PointPlacement()
{
    const bool shiftHeld = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
    if (!shiftHeld)
    {
        return;
    }

    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        return;
    }

    auto sceneView = dynamic_pointer_cast<Scene_View>(EDITOR->Get_Window(L"Scene"));
    if (!sceneView || !sceneView->Is_Hovered())
    {
        return;
    }

    if (EDITOR->IsPlaying())
    {
        return;
    }

    const ImVec2 vpMin = sceneView->Get_ViewportMin();
    const ImVec2 vpMax = sceneView->Get_ViewportMax();
    const float vpW = vpMax.x - vpMin.x;
    const float vpH = vpMax.y - vpMin.y;
    if (vpW <= 0.f || vpH <= 0.f)
    {
        return;
    }

    const ImVec2 mousePos = ImGui::GetIO().MousePos;
    const float depthW = static_cast<float>(GAME->Get_ViewportWidth());
    const float depthH = static_cast<float>(GAME->Get_ViewportHeight());

    POINT pixelPos{};
    pixelPos.x = static_cast<LONG>((mousePos.x - vpMin.x) * (depthW / max(vpW, 1.f)));
    pixelPos.y = static_cast<LONG>((mousePos.y - vpMin.y) * (depthH / max(vpH, 1.f)));

    Vec3 worldPos{};
    if (Try_SampleWorldPos(pixelPos, worldPos))
    {
        _points.push_back(worldPos);
    }
}

void PathTool::Render_PointList()
{
    ImGui::Text("Points : %d", static_cast<int>(_points.size()));
    ImGui::Separator();

    ImGui::BeginDisabled(_points.empty());
    if (ImGui::Button("Remove Last"))
    {
        _points.pop_back();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All"))
    {
        _points.clear();
    }
    ImGui::EndDisabled();

    Render_SavePanel();
    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(_points.size()); ++i)
    {
        const Vec3& p = _points[i];
        ImGui::Text("[%d]  X: %.2f  Y: %.2f  Z: %.2f", i, p.x, p.y, p.z);
    }
}

void PathTool::Render_PathOverlay()
{
    if (_points.empty())
    {
        return;
    }

    auto sceneView = dynamic_pointer_cast<Scene_View>(EDITOR->Get_Window(L"Scene"));
    if (!sceneView)
    {
        return;
    }

    const ImVec2 vpMin = sceneView->Get_ViewportMin();
    const ImVec2 vpMax = sceneView->Get_ViewportMax();

    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    ImVec2 prevScreen{};
    bool hasPrev = false;

    for (int i = 0; i < static_cast<int>(_points.size()); ++i)
    {
        ImVec2 screen{};
        if (!Try_WorldToScreen(_points[i], vpMin, vpMax, screen))
        {
            hasPrev = false;
            continue;
        }

        if (hasPrev)
        {
            drawList->AddLine(prevScreen, screen, IM_COL32(255, 220, 0, 255), 2.f);
        }

        const ImU32 dotColor = (i == static_cast<int>(_points.size()) - 1)
            ? IM_COL32(255, 80, 80, 255)
            : IM_COL32(255, 220, 0, 255);

        drawList->AddCircleFilled(screen, 5.f, dotColor);

        prevScreen = screen;
        hasPrev = true;
    }
}

void PathTool::Render_SavePanel()
{
    ImGui::Separator();
    ImGui::Text("Save");
    ImGui::SetNextItemWidth(200.f);
    ImGui::InputText("##SaveName", _saveNameBuffer, sizeof(_saveNameBuffer));
    ImGui::SameLine();
    ImGui::BeginDisabled(_points.empty());
    if (ImGui::Button("Save JSON"))
    {
        Save_ToJson();
    }
    ImGui::EndDisabled();
}

void PathTool::Save_ToJson()
{
    const fs::path dir = L"../../../Client/Bin/Resources/Monster_Path";
    error_code ec;
    fs::create_directories(dir, ec);

    string fileName = _saveNameBuffer;
    if (fileName.empty())
    {
        fileName = "path";
    }
    if (!fileName.ends_with(".json"))
    {
        fileName += ".json";
    }

    const fs::path filePath = dir / fileName;

    json root;
    json pointsArr = json::array();
    for (const Vec3& p : _points)
    {
        json pt;
        pt["x"] = p.x;
        pt["y"] = p.y;
        pt["z"] = p.z;
        pointsArr.push_back(pt);
    }
    root["points"] = pointsArr;

    ofstream ofs(filePath);
    if (ofs.is_open())
    {
        ofs << root.dump(4);
    }
}

Shared<PathTool> PathTool::Create()
{
    return make_shared<PathTool>();
}
