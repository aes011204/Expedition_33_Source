#include "pch.h"
#include "PanelViewport.h"
#include "RenderTarget.h"

#include "MECamera.h"
#include "SceneContext.h"

PanelViewport::PanelViewport(Shared<MECamera> camera, Shared<RenderTarget> rt, SceneContext* ctx)
    : MEWindow{ L"Viewport" }, _camera{ camera }, _rt{ rt }, _ctx{ ctx }
{

}

void PanelViewport::Update(float timeDelta)
{
    if (_camera && _isHovered)
    {
        _camera->UpdateInput(timeDelta);
    }
}

void PanelViewport::Render()
{
    bool open = IsOpen();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    if (ImGui::Begin("Viewport", &open))
    {
        const ImVec2 size = ImGui::GetContentRegionAvail();
        _isHovered = ImGui::IsWindowHovered();

        if (_rt)
        {
            const uint32 w = static_cast<uint32>(size.x);
            const uint32 h = static_cast<uint32>(size.y);

            if (w > 0 && h > 0 && (w != _rt->Get_Width() || h != _rt->Get_Height()))
            {
                _rt->Resize(w, h, DXGI_FORMAT_R8G8B8A8_UNORM, Vec4(0.1f, 0.1f, 0.1f, 1.f));
                if (_camera)
                {
                    _camera->SetAspect(static_cast<float>(w) / static_cast<float>(h));
                }
            }

            ImGui::Image(reinterpret_cast<ImTextureID>(_rt->Get_SRV()), size);

            if (_ctx->_cursorVisible)
            {
                const ImVec2 vpMin = ImGui::GetItemRectMin();
                const ImVec2 vpMax = ImGui::GetItemRectMax();
                const ImVec2 ovrPos = ImVec2(vpMin.x + 10.f, vpMax.y - 36.f);

                ImGui::SetNextWindowPos(ovrPos);
                ImGui::SetNextWindowBgAlpha(0.6f);
                ImGui::SetNextWindowSize(ImVec2(240.f, 0.f));
                constexpr ImGuiWindowFlags ovrFlags =
                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                    ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoSavedSettings;
                if (ImGui::Begin("##CursorOverlay", nullptr, ovrFlags))
                {
                    const Vec3& p = _ctx->_cursorPos;
                    ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.9f, 1.f), "Cursor");
                    ImGui::SameLine();
                    ImGui::Text("%.2f  %.2f  %.2f", p.x, p.y, p.z);
                }
                ImGui::End();
            }


            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MODEL_PATH"))
                {
                    _spawnModelPath = static_cast<const char*> (payload->Data);
                    _spawnMousePos = ImGui::GetIO().MousePos;
                    _spawnVpPos = ImGui::GetItemRectMin();
                    _spawnVpSize = ImGui::GetItemRectSize();
                    _spawnRequested = true;
                }
                ImGui::EndDragDropTarget();
            }

            if (_ctx && ImGui::IsItemClicked(ImGuiMouseButton_Left) &&
                !ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
            {
                const bool shiftHeld = ImGui::GetIO().KeyShift;
                if (shiftHeld)
                {
                    _cursorSetRequested = true;
                    _cursorMousePos = ImGui::GetIO().MousePos;
                    _cursorVpPos = ImGui::GetItemRectMin();
                    _cursorVpSize = ImGui::GetItemRectSize();
                }

                else
                {
                    _pickRequested = true;
                    _pickMousePos = ImGui::GetIO().MousePos;
                    _pickVpPos = ImGui::GetItemRectMin();
                    _pickVpSize = ImGui::GetItemRectSize();
                }
            }

            if (_camera && _ctx && _ctx->_selected)
            {
                const ImVec2 winPos = ImGui::GetWindowPos();
                const ImVec2 winSize = ImGui::GetWindowSize();

                ImGuizmo::SetDrawlist();
                ImGuizmo::SetRect(winPos.x, winPos.y, winSize.x, winSize.y);

                if (ImGui::IsWindowHovered())
                {
                    if (ImGui::IsKeyPressed(ImGuiKey_W))
                    {
                        _gizmoOp = ImGuizmo::TRANSLATE;
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_E))
                    {
                        _gizmoOp = ImGuizmo::ROTATE;
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_R))
                    {
                        _gizmoOp = ImGuizmo::SCALE;
                    }
                }

                float view[16] = {}, proj[16] = {};
                memcpy(view, &_camera->GetView(), sizeof(float) * 16);
                memcpy(proj, &_camera->GetProj(), sizeof(float) * 16);
                float mat[16] = {};
                memcpy(mat, &_ctx->_selected->_worldMatrix, sizeof(float) * 16);

                const bool ctrlHeld = ImGui::GetIO().KeyCtrl && _snapSettings;
                float snapValues[3] = {};

                if (ctrlHeld)
                {
                    if (_gizmoOp == ImGuizmo::TRANSLATE)
                    {
                        snapValues[0] = snapValues[1] = snapValues[2] = _snapSettings->translate;
                    }
                    else if (_gizmoOp == ImGuizmo::ROTATE)
                    {
                        snapValues[0] = snapValues[1] = snapValues[2] = _snapSettings->rotate;
                    }
                    else if (_gizmoOp == ImGuizmo::SCALE)
                    {
                        snapValues[0] = snapValues[1] = snapValues[2] = _snapSettings->scale;
                    }
                }

                const bool manipulated = ImGuizmo::Manipulate(view, proj, _gizmoOp, _gizmoMode, mat,
                    nullptr, ctrlHeld ? snapValues : nullptr);
                if (manipulated)
                {
                    memcpy(&_ctx->_selected->_worldMatrix, mat, sizeof(float) * 16);
                    _ctx->_selected->_worldMatrix.Decompose(
                        _ctx->_selected->_scale,
                        _ctx->_selected->_rotation,
                        _ctx->_selected->_position);
                }
            }
        }

        else
        {
            ImGui::Text("No RenderTarget");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    SetOpen(open);
}



bool PanelViewport::ConsumePick(ImVec2& outMousePos, ImVec2& outVpPos, ImVec2& outVpSize)
{
    if (!_pickRequested)
    {
        return false;
    }

    _pickRequested = false;
    outMousePos = _pickMousePos;
    outVpPos = _pickVpPos;
    outVpSize = _pickVpSize;
    return true;
}

bool PanelViewport::ConsumeSpawn(string& outModelPath, ImVec2& outMousePos, ImVec2& outVpPos, ImVec2& outVpSize)
{
    if (!_spawnRequested)
    {
        return false;
    }

    _spawnRequested = false;
    outModelPath = _spawnModelPath;
    outMousePos = _spawnMousePos;
    outVpPos = _spawnVpPos;
    outVpSize = _spawnVpSize;

    return true;
}

bool PanelViewport::ConsumeCursorSet(ImVec2& outMousePos, ImVec2& outVpPos, ImVec2& outVpSize)
{
    if (!_cursorSetRequested)
    {
        return false;
    }

    _cursorSetRequested = false;
    outMousePos = _cursorMousePos;
    outVpPos = _cursorVpPos;
    outVpSize = _cursorVpSize;

    return true;
}

Shared<PanelViewport> PanelViewport::Create(Shared<MECamera> camera, Shared<RenderTarget> rt, SceneContext* ctx)
{
    return make_shared<PanelViewport>(camera, rt, ctx);
}
