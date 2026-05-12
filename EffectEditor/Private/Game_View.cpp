#include "Game_View.h"

#include "EffectEditorInstance.h"
#include "GameInstance.h"
#include "RenderTarget.h"

NS_BEGIN(EffectEditor)

namespace Internal
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
            canvasMin.y + (canvasSize.y - imageHeight) * 0.5f
        );

        return {
            imageMin,
            ImVec2(imageMin.x + imageWidth, imageMin.y + imageHeight)
        };
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
    const string& windowName = Get_ImGuiWindowName();

    if (EDITOR->Consume_WindowFocusRequest(EditorViewportTarget::Game))
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

        const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
        const ImVec2 canvasMax = ImVec2(canvasMin.x + viewportSize.x, canvasMin.y + viewportSize.y);
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(18, 20, 24, 255));
        drawList->AddRect(canvasMin, canvasMax, IM_COL32(55, 60, 70, 255));

        ImGui::InvisibleButton("GameViewportCanvas", viewportSize);
        EDITOR->Set_GameViewportInputState(ImGui::IsItemHovered());

        ID3D11ShaderResourceView* viewportSrv = EDITOR->Get_SharedViewportSRV();
        if (nullptr != viewportSrv)
        {
            const ImTextureID textureId = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(viewportSrv));
            const float aspect = EDITOR->Get_EffectiveViewportAspect();
            const Internal::ViewportImageRect imageRect =
                Internal::Compute_AspectFitRect(canvasMin, viewportSize, aspect);

            drawList->AddImage(textureId, imageRect.imageMin, imageRect.imageMax);
        }
        else
            drawList->AddText(ImVec2(canvasMin.x + 12.f, canvasMin.y + 12.f), IM_COL32(210, 216, 224, 255), "Game RenderTarget Missing");
    }
    else
        EDITOR->Set_GameViewportInputState(false);

    Set_Open(isOpen);
    ImGui::End();
}

Shared<Game_View> Game_View::Create()
{
    return make_shared<Game_View>();
}

NS_END
