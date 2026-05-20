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
        
        auto imguiManager = ModelImGui_Manager::GetInstance();
        if (imguiManager)
        {
            imguiManager->Request_ViewportSize(static_cast<uint32>(_viewportSize.x), static_cast<uint32>(_viewportSize.y));
            ID3D11ShaderResourceView* srv = imguiManager->Get_SharedViewportSRV();
            
            if (srv)
            {
                const ImTextureID textureId = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(srv));
                // 엔진에서 이미 Background > Grid > Object 순서로 그려진 이미지를 출력
                ImGui::Image(textureId, _viewportSize);
            }
            else
            {
                ImGui::GetWindowDrawList()->AddRectFilled(canvasPos, ImVec2(canvasPos.x + _viewportSize.x, canvasPos.y + _viewportSize.y), IM_COL32(38, 42, 50, 255));
                ImGui::Text("Scene RenderTarget Missing");
            }
        }

        // 마우스 입력을 위한 투명 버튼
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
