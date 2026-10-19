#include "Game_View.h"
#include "GameInstance.h"
#include "EditorInstance.h"
#include "RenderTarget.h"

namespace
{
struct ViewportImageRect
{
    ImVec2 imageMin;
    ImVec2 imageMax;
};

static ViewportImageRect Compute_AspectFitRect(const ImVec2& canvasMin, const ImVec2& canvasSize, float aspect)
{
    if (aspect <= 0.f || canvasSize.x <= 1.f || canvasSize.y <= 1.f)
    {
        return {
            canvasMin,
            ImVec2(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y)
        };
    }

    float imageWidth = canvasSize.x;
    float imageHeight = imageWidth / aspect;

    if (imageHeight > canvasSize.y)
    {
        imageHeight = canvasSize.y;
        imageWidth = imageHeight * aspect;
    }

    const ImVec2 imageMin = ImVec2(
        canvasMin.x + (canvasSize.x - imageWidth) * 0.5f,
        canvasMin.y + (canvasSize.y - imageHeight) * 0.5f);

    return {
        imageMin,
        ImVec2(imageMin.x + imageWidth, imageMin.y + imageHeight)
    };
}


    static bool Is_PointInRect(const ImVec2& point, const ImVec2& min, const ImVec2& max)
{
    return
        point.x >= min.x &&
        point.x <= max.x &&
        point.y >= min.y &&
        point.y <= max.y;
}

static POINT Make_InvalidMousePosition()
{
    POINT mousePosition{};
    mousePosition.x = -100000;
    mousePosition.y = -100000;
    return mousePosition;
}

static void Sync_GameMousePositionOverride(const ViewportImageRect& imageRect)
{
    const ImVec2 mousePos = ImGui::GetIO().MousePos;

    const float imageWidth = imageRect.imageMax.x - imageRect.imageMin.x;
    const float imageHeight = imageRect.imageMax.y - imageRect.imageMin.y;

    if (imageWidth <= 0.f || imageHeight <= 0.f)
    {
        GAME->Set_MousePositionOverride(Make_InvalidMousePosition());
        return;
    }

    if (!Is_PointInRect(mousePos, imageRect.imageMin, imageRect.imageMax))
    {
        GAME->Set_MousePositionOverride(Make_InvalidMousePosition());
        return;
    }

    const float normalizedX = (mousePos.x - imageRect.imageMin.x) / imageWidth;
    const float normalizedY = (mousePos.y - imageRect.imageMin.y) / imageHeight;

    POINT gameMousePosition{};
    gameMousePosition.x = static_cast<LONG>(normalizedX * static_cast<float>(GAME->Get_ViewportWidth()));
    gameMousePosition.y = static_cast<LONG>(normalizedY * static_cast<float>(GAME->Get_ViewportHeight()));

    GAME->Set_MousePositionOverride(gameMousePosition);
}
}

Game_View::Game_View()
    : Editor_Window{ L"Game" }
{
}

void Game_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    if (EDITOR->Consume_WindowFocusRequest(EDITOR_VIEWPORT_TARGET::Game))
        ImGui::SetNextWindowFocus();

    if (ImGui::Begin(windowName.c_str(), &isOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        ImVec2 viewportSize = ImGui::GetContentRegionAvail();

        if (viewportSize.x < 1.f)
            viewportSize.x = 1.f;

        if (viewportSize.y < 1.f)
            viewportSize.y = 1.f;

        const uint32 width = static_cast<uint32>(viewportSize.x);
        const uint32 height = static_cast<uint32>(viewportSize.y);

        EditorInstance::GetInstance()->Request_ViewportSize(
            EDITOR_VIEWPORT_TARGET::Game,
            width,
            height);

        const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
        const ImVec2 canvasMax = ImVec2(canvasMin.x + viewportSize.x, canvasMin.y + viewportSize.y);
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(18, 20, 24, 255));
        drawList->AddRect(canvasMin, canvasMax, IM_COL32(55, 60, 70, 255));

        ImGui::InvisibleButton("GameViewportCanvas", viewportSize);

        ID3D11ShaderResourceView* pSRV = EditorInstance::GetInstance()->Get_SharedViewportSRV();
        if (nullptr != pSRV)
        {
            const ImTextureID textureId = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(pSRV));
            const float aspect = EDITOR->Get_EffectiveViewportAspect();
            const ViewportImageRect imageRect =
                Compute_AspectFitRect(canvasMin, viewportSize, aspect);

            drawList->AddImage(textureId, imageRect.imageMin, imageRect.imageMax);
            Sync_GameMousePositionOverride(imageRect);
        }
        else
        {
            drawList->AddText(ImVec2(canvasMin.x + 12.f, canvasMin.y + 12.f), IM_COL32(210, 216, 224, 255), "Game RenderTarget Missing");
        }
    }
    Set_Open(isOpen);
    ImGui::End();
}

Shared<Game_View> Game_View::Create()
{
    return make_shared<Game_View>();
}



