#include "pch.h"
#include "PanelViewport.h"
#include "RenderTarget.h"

#include "MECamera.h"
#include "SceneContext.h"
#include "CmdTransformObject.h"
#include "MEPickingSystem.h"
#include "GameInstance.h"
#include "LightingPipeline.h"

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

            ImGui::Image(reinterpret_cast<ImTextureID>(_sceneSRV ? _sceneSRV : _rt->Get_SRV()), size);


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

            if (_camera)
            {
                const ImVec2 vpMin = ImGui::GetItemRectMin();
                const ImVec2 vpMax = ImGui::GetItemRectMax();
                const ImVec2 camPos = ImVec2(vpMin.x + 10.f, vpMin.y + 10.f);

                ImGui::SetNextWindowPos(camPos);
                ImGui::SetNextWindowBgAlpha(0.6f);
                ImGui::SetNextWindowSize(ImVec2(220.f, 0.f));
                constexpr ImGuiWindowFlags camFlags =
                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
                if (ImGui::Begin("##CameraOverlay", nullptr, camFlags))
                {
                    ImGui::Text("Speed: %.1f", _camera->GetMoveSpeed());
                    float& sens = _camera->GetRotateSensitivityRef();
                    ImGui::SetNextItemWidth(160.f);
                    ImGui::SliderFloat("Rotate", &sens, 0.001f, 0.01f, "%.4f");
                    ImGui::Checkbox("BBox", &_ctx->_objectBBoxMode);
                    ImGui::Checkbox("Instance 숨기기", &_ctx->_hideInstancedMesh);
                    ImGui::Checkbox("Instanced Spawn", &_spawnAsInstanced);

                    if (auto lp = GAME->Get_LightingPipeline())
                    {
                        bool iblOn = lp->Is_IBLEnabled();
                        if (ImGui::Checkbox("IBL", &iblOn))
                            lp->Set_IBLEnabled(iblOn);
                        if (iblOn)
                        {
                            auto& params = lp->Get_DeferredLightingParams();
                            ImGui::SetNextItemWidth(160.f);
                            ImGui::DragFloat("IBL Intensity", &params.iblIntensity, 0.01f, 0.f, 8.f, "%.2f");
                        }
                    }
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

            const bool sculptActive = _sculptState && _sculptState->mode != ESculptMode::None;

            if (_ctx && ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing() && sculptActive)
            {
                _sculptRequested = true;
                _sculptMousePos = ImGui::GetIO().MousePos;
                _sculptVpPos = ImGui::GetItemRectMin();
                _sculptVpSize = ImGui::GetItemRectSize();
            }
            else if (_ctx && ImGui::IsItemClicked(ImGuiMouseButton_Left) &&
                !ImGuizmo::IsOver() && !ImGuizmo::IsUsing() && !sculptActive)
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

                bool gizmoUsing = ImGuizmo::IsUsing();
                if (gizmoUsing && !_gizmoWasUsing)
                {
                    _gizmoPrevPos = _ctx->_selected->_position;
                    _gizmoPrevRot = _ctx->_selected->_rotation;
                    _gizmoPrevScale = _ctx->_selected->_scale;
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

                if (!gizmoUsing && _gizmoWasUsing)
                {
                    _ctx->PushCommand(CmdTransformObject::Create(
                        _ctx->_selected,
                        _gizmoPrevPos, _gizmoPrevRot, _gizmoPrevScale,
                        _ctx->_selected->_position, _ctx->_selected->_rotation, _ctx->_selected->_scale));
                }

                _gizmoWasUsing = gizmoUsing;
            }

            if (sculptActive && ImGui::IsItemHovered() && _camera && _ctx && _pickingSystem)
            {
                const ImVec2 vpMin = ImGui::GetItemRectMin();
                const ImVec2 vpMax = ImGui::GetItemRectMax();
                const ImVec2 vpSz = ImVec2(vpMax.x - vpMin.x, vpMax.y - vpMin.y);
                const ImVec2 mouse = ImGui::GetIO().MousePos;

                Vec3 hitPos{};
                if (_pickingSystem->SampleWorldPos(*_camera, mouse, vpMin, vpSz, hitPos))
                {
                    Matrix viewProj = Matrix(_camera->GetView()) * Matrix(_camera->GetProj());

                    auto WorldToScreen = [&](Vec3 wp, ImVec2& out) -> bool
                        {
                            Vec4 clip = Vec4::Transform(Vec4(wp.x, wp.y, wp.z, 1.f), viewProj);
                            if (clip.w <= 0.f) return false;
                            out.x = vpMin.x + ((clip.x / clip.w) + 1.f) * 0.5f * vpSz.x;
                            out.y = vpMin.y + (1.f - (clip.y / clip.w)) * 0.5f * vpSz.y;
                            return true;
                        };

                    ImVec2 center;
                    if (WorldToScreen(hitPos, center))
                    {
                        float screenRadius = 8.f;
                        ImVec2 edge;
                        if (WorldToScreen(Vec3(hitPos.x + _sculptState->brushRadius, hitPos.y, hitPos.z), edge))
                        {
                            float dx = edge.x - center.x;
                            float dy = edge.y - center.y;
                            screenRadius = sqrtf(dx * dx + dy * dy);
                        }

                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        dl->AddCircle(center, max(2.f, screenRadius), IM_COL32(255, 220, 0, 220), 64, 2.f);
                    }
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



bool PanelViewport::Ray_Hit_Landscape(ImVec2 mousePos, ImVec2 vpPos, ImVec2 vpSz, Vec3& outHit)
{
    if (!_ctx || _ctx->_activeLandscapeIdx < 0 || _ctx->_activeLandscapeIdx >= static_cast<int32>(_ctx->_landscapes.size()))
    {
        return false;
    }

    auto& actor = _ctx->_landscapes[_ctx->_activeLandscapeIdx];
    auto vb = actor->Get_VIBuffer();
    if (!vb)
    {
        return false;
    }

    float u = (mousePos.x - vpPos.x) / vpSz.x;
    float v = (mousePos.y - vpPos.y) / vpSz.y;
    float ndcX = u * 2.f - 1.f;
    float ndcY = 1.f - v * 2.f;

    Matrix viewProjInv = (Matrix(_camera->GetView()) * Matrix(_camera->GetProj())).Invert();
    Vec3 nearP = Vec3::Transform(Vec3(ndcX, ndcY, 0.f), viewProjInv);
    Vec3 farP = Vec3::Transform(Vec3(ndcX, ndcY, 1.f), viewProjInv);
    Vec3 dir = farP - nearP;
    dir.Normalize();

    Vec3 landscapePos = actor->Get_Transform()->Get_Position();
    float step = min(vb->Get_SizeX(), vb->Get_SizeZ()) * 0.5f;

    float camFar = _camera->GetFar();
    for (float t = 0.f; t < camFar; t += step)
    {
        Vec3 p = nearP + dir * t;
        float terrainY = vb->Sample_Height(p.x - landscapePos.x, p.z - landscapePos.z) + landscapePos.y;
        if (p.y <= terrainY)
        {
            outHit = p;
            return true;
        }
    }

    return false;
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

bool PanelViewport::ConsumeSculpt(ImVec2& outMousePos, ImVec2& outVpPos, ImVec2& outVpSize)
{
    if (!_sculptRequested)
    {
        return false;
    }

    _sculptRequested = false;
    outMousePos = _sculptMousePos;
    outVpPos = _sculptVpPos;
    outVpSize = _sculptVpSize;

    return true;
}

Shared<PanelViewport> PanelViewport::Create(Shared<MECamera> camera, Shared<RenderTarget> rt, SceneContext* ctx)
{
    return make_shared<PanelViewport>(camera, rt, ctx);
}
