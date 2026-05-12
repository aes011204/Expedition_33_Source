#include "pch.h"
#include "ModelSceneWindow.h"
#include "ModelImGui_Manager.h"
#include "GameInstance.h"

NS_BEGIN(ModelEditor)


HRESULT ModelSceneWindow::Initialize()
{
    return S_OK;
}

void ModelSceneWindow::Update(float timeDelta)
{
}

void ModelSceneWindow::Render()
{
    if (!_showBlock)
        return;

    if (ImGui::Begin("Scene", &_showBlock, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        ImGuizmo::BeginFrame();

        // 윈도우의 제목 표시줄 등을 제외한 실제 콘텐츠 영역의 위치와 크기 설정
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
        ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
        
        float rectX = windowPos.x + contentMin.x;
        float rectY = windowPos.y + contentMin.y;
        float rectW = contentMax.x - contentMin.x;
        float rectH = contentMax.y - contentMin.y;
        
        ImGuizmo::SetRect(rectX, rectY, rectW, rectH);
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());

        Matrix view = *GAME->Get_Transform(D3DTS::View);
        Matrix proj = *GAME->Get_Transform(D3DTS::Proj);
        float gridMatrix[16];
        XMStoreFloat4x4((XMFLOAT4X4*)gridMatrix, XMMatrixIdentity());

        ImGuizmo::DrawGrid(reinterpret_cast<float*>(&view), reinterpret_cast<float*>(&proj), gridMatrix, 1000.f);

        Render_ViewportCanvas();
    }
    ImGui::End();
}

void ModelSceneWindow::Render_ViewportCanvas()
{
    _viewportSize = ImGui::GetContentRegionAvail();

    if (_viewportSize.x < 1.f)
        _viewportSize.x = 1.f;

    if (_viewportSize.y < 1.f)
        _viewportSize.y = 1.f;

    const uint32 width = static_cast<uint32>(_viewportSize.x);
    const uint32 height = static_cast<uint32>(_viewportSize.y);

    auto imguiManager = ModelImGui_Manager::GetInstance();
    if (!imguiManager) return;

    imguiManager->Request_ViewportSize(width, height);

    ID3D11ShaderResourceView* pSRV = imguiManager->Get_SharedViewportSRV();
    if (nullptr != pSRV)
    {
        const ImTextureID textureId =
            static_cast<ImTextureID>(reinterpret_cast<intptr_t>(pSRV));

        ImGui::Image(textureId, _viewportSize);
    }
    else
    {
        const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        const ImVec2 canvasEnd = ImVec2(canvasPos.x + _viewportSize.x, canvasPos.y + _viewportSize.y);
        ImDrawList* pDrawList = ImGui::GetWindowDrawList();

        pDrawList->AddRectFilled(canvasPos, canvasEnd, IM_COL32(34, 38, 44, 255));
        pDrawList->AddRect(canvasPos, canvasEnd, IM_COL32(85, 95, 110, 255));
        pDrawList->AddText(ImVec2(canvasPos.x + 12.f, canvasPos.y + 12.f), IM_COL32(210, 216, 224, 255), "Scene RenderTarget Missing");

        ImGui::InvisibleButton("SceneViewportCanvas", _viewportSize);
    }
}

Shared<ModelSceneWindow> ModelSceneWindow::Create()
{
    auto instance = make_shared<ModelSceneWindow>();
    if (FAILED(instance->Initialize()))
    {
        return nullptr;
    }
    return instance;
}

NS_END
