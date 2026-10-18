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
        _viewportSize = ImGui::GetContentRegionAvail();

        if (_viewportSize.x < 1.f) _viewportSize.x = 1.f;
        if (_viewportSize.y < 1.f) _viewportSize.y = 1.f;

        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 canvasEnd = ImVec2(canvasPos.x + _viewportSize.x, canvasPos.y + _viewportSize.y);
        ImDrawList* pDrawList = ImGui::GetWindowDrawList();

        // 1. 배경 레이어
        pDrawList->AddRectFilled(canvasPos, canvasEnd, IM_COL32(38, 42, 50, 255));

        // 2. 그리드 레이어 (배경 위에 그리고, 모델 이미지보다 먼저 그려서 모델 뒤에 깔리게 함)
        ImGuizmo::BeginFrame();
        ImGuizmo::SetRect(canvasPos.x, canvasPos.y, _viewportSize.x, _viewportSize.y);
        ImGuizmo::SetDrawlist(pDrawList);

        Matrix view = *GAME->Get_Transform(D3DTS::View);
        Matrix proj = *GAME->Get_Transform(D3DTS::Proj);
        float gridMatrix[16];
        XMStoreFloat4x4((XMFLOAT4X4*)gridMatrix, XMMatrixIdentity());
        
        ImGuizmo::DrawGrid(reinterpret_cast<float*>(&view), reinterpret_cast<float*>(&proj), gridMatrix, 1000.f);

        // 3. 오브젝트 레이어 (가장 위에 그려짐, 투명한 영역으로 그리드가 보임)
        auto imguiManager = ModelImGui_Manager::GetInstance();
        if (imguiManager)
        {
            imguiManager->Request_ViewportSize(static_cast<uint32>(_viewportSize.x), static_cast<uint32>(_viewportSize.y));
            ID3D11ShaderResourceView* srv = imguiManager->Get_SharedViewportSRV();
            
            if (srv)
            {
                const ImTextureID textureId = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(srv));
                pDrawList->AddImage(textureId, canvasPos, canvasEnd);
            }
            else
            {
                pDrawList->AddText(ImVec2(canvasPos.x + 12.f, canvasPos.y + 12.f), IM_COL32(210, 216, 224, 255), "Scene RenderTarget Missing");
            }
        }

        ImGui::InvisibleButton("SceneViewportCanvas", _viewportSize);
    }
    ImGui::End();
}

void ModelSceneWindow::Render_ViewportCanvas()
{
    // Render 함수 내로 통합됨
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
