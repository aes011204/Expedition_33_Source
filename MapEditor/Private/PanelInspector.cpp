#include "pch.h"
#include "PanelInspector.h"
#include "SceneContext.h"

PanelInspector::PanelInspector(SceneContext* ctx)
    : MEWindow{ L"Inspector" }, _ctx{ ctx }
{

}

void MapEditor::PanelInspector::Render()
{
    bool open = IsOpen();
    if (ImGui::Begin("Inspector", &open))
    {
        if (!_ctx || !_ctx->_selected)
        {
            ImGui::TextDisabled("(선택된 오브젝트 없음)");
            ImGui::End();
            SetOpen(open);
            return;
        }

        auto& obj = _ctx->_selected;
        bool changed = false;

        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            float pos[3] = { obj->_position.x, obj->_position.y, obj->_position.z };
            if (ImGui::DragFloat3("Position", pos, 0.1f))
            {
                obj->_position = Vec3(pos[0], pos[1], pos[2]);
                changed = true;
            }

            Matrix rotMat = Matrix::CreateFromQuaternion(obj->_rotation);

            float pitch = {}, yaw = {}, roll = {};
            pitch = asinf(-rotMat._32);
            if (cosf(pitch) > 1e-6f)
            {
                yaw = atan2f(rotMat._31, rotMat._33);
                roll = atan2f(rotMat._12, rotMat._22);
            }
            else
            {
                yaw = atan2f(-rotMat._13, rotMat._11);
                roll = 0.f;
            }

            float euler[3] = {
                XMConvertToDegrees(pitch),
                XMConvertToDegrees(yaw),
                XMConvertToDegrees(roll)
            };

            if (ImGui::DragFloat3("Rotation", euler, 0.5f))
            {
                obj->_rotation = Quat::CreateFromYawPitchRoll(
                    XMConvertToRadians(euler[1]),
                    XMConvertToRadians(euler[0]),
                    XMConvertToRadians(euler[2]));

                changed = true;
            }

            float scl[3] = { obj->_scale.x,obj->_scale.y, obj->_scale.z };
            if (ImGui::DragFloat3("Scale", scl, 0.01f))
            {
                obj->_scale = Vec3(scl[0], scl[1], scl[2]);
                changed = true;
            }
        }

        if (changed)
        {
            obj->UpdateWorldMatrix();
        }
    }
    ImGui::End();
    SetOpen(open);
}

Shared<PanelInspector> PanelInspector::Create(SceneContext* ctx)
{
    return make_shared<PanelInspector>(ctx);
}
