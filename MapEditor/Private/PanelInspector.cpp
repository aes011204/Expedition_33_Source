#include "pch.h"
#include "PanelInspector.h"
#include "SceneContext.h"
#include "MECamera.h"

PanelInspector::PanelInspector(SceneContext* ctx, MECamera* camera)
    : MEWindow{ L"Inspector" }, _ctx{ ctx }, _camera{ camera }
{
    for (int i = 0; i < _shapeNum; ++i)
    {
        _shapes[i] = magic_enum::enum_name(static_cast<EZoneShape>(i)).data();
    }

    for (int i = 0; i < _lightNum; ++i)
    {
        _lights[i] = magic_enum::enum_name(static_cast<LightType>(i)).data();
    }
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

        if (ImGui::Button("Focus"))
        {
            _camera->FocusOn(obj->_position);
        }
        ImGui::SameLine();

        if (ImGui::Button("Delete"))
        {
            _ctx->Remove(_ctx->_selected);
            ImGui::End();
            SetOpen(open);
            return;
        }

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

        if (obj->_type == EObjectType::EncounterZone ||
            obj->_type == EObjectType::TriggerVolume ||
            obj->_type == EObjectType::CameraZone ||
            obj->_type == EObjectType::PostProcessVolume)
        {
            if (ImGui::CollapsingHeader("Zone Shape", ImGuiTreeNodeFlags_DefaultOpen))
            {
                int shapeIdx = (obj->_zoneShape == EZoneShape::Sphere) ? 1 : 0;
                if (ImGui::Combo("Shape", &shapeIdx, _shapes, _shapeNum))
                {
                    obj->_zoneShape = (shapeIdx == 1) ? EZoneShape::Sphere : EZoneShape::Box;
                }

                if (obj->_zoneShape == EZoneShape::Box)
                {
                    float ext[3] = { obj->_zoneExtent.x, obj->_zoneExtent.y, obj->_zoneExtent.z };
                    if (ImGui::DragFloat3("Extent", ext, 0.1f, 0.01f, 1000.f))
                    {
                        obj->_zoneExtent = { ext[0], ext[1], ext[2] };
                    }
                }
                else
                {
                    ImGui::DragFloat("Radius", &obj->_zoneRadius, 0.1f, 0.01f, 1000.f);
                }
            }
        }

        switch (obj->_type)
        {
        case EObjectType::StaticMesh:
            if (ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen))
            {
                string pathStr = String::ToString(obj->_modelPath);
                ImGui::TextDisabled("%s", pathStr.empty() ? "(없음)" : pathStr.c_str());
            }
            break;

        case EObjectType::EncounterZone:
            if (ImGui::CollapsingHeader("Encounter Zone", ImGuiTreeNodeFlags_DefaultOpen))
            {
                char buf[256] = {};
                strncpy_s(buf, obj->_zoneId.c_str(), sizeof(buf) - 1);
                if (ImGui::InputText("EnemyGroupId", buf, sizeof(buf)))
                {
                    obj->_zoneId = buf;
                }
            }
            break;

        case EObjectType::TriggerVolume:
            if (ImGui::CollapsingHeader("Trigger Volume", ImGuiTreeNodeFlags_DefaultOpen))
            {
                char buf[256] = {};
                strncpy_s(buf, obj->_zoneId.c_str(), sizeof(buf) - 1);
                if (ImGui::InputText("EventId", buf, sizeof(buf)))
                {
                    obj->_zoneId = buf;
                }
            }
            break;

        case EObjectType::CameraZone:
            if (ImGui::CollapsingHeader("Camera Zone", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::DragFloat("FOV", &obj->_cameraFov, 0.5f, 1.f, 170.f);
                ImGui::Checkbox("Fixed Angle", &obj->_cameraFixedAngle);
                ImGui::DragFloat("Transition Time", &obj->_cameraTransitionTime, 0.01f, 0.f, 10.f);
            }
            break;

        case EObjectType::PostProcessVolume:
            if (ImGui::CollapsingHeader("Post Process", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::DragFloat("Bloom Intensity", &obj->_ppBloomIntensity, 0.01f, 0.f, 10.f);
                ImGui::DragFloat("Exposure Min", &obj->_ppExposureMin, 0.01f, -10.f, 10.f);
                ImGui::DragFloat("Exposure Max", &obj->_ppExposureMax, 0.01f, -10.f, 10.f);
                ImGui::DragFloat("Fog Density", &obj->_ppFogDensity, 0.001f, 0.f, 1.f);
                float fogColor[3] = { obj->_ppFogColor.x, obj->_ppFogColor.y, obj->_ppFogColor.z };
                if (ImGui::ColorEdit3("Fog Color", fogColor))
                {
                    obj->_ppFogColor = { fogColor[0], fogColor[1], fogColor[2] };
                }
            }
            break;

        case EObjectType::Light:
            if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
            {
                int ltIdx = (obj->_lightType == LightType::Point) ? 1 : 0;
                if (ImGui::Combo("Type", &ltIdx, _lights, 2))
                {
                    obj->_lightType = (ltIdx == 1) ? LightType::Point : LightType::Directional;
                }

                float diffuse[4] = { obj->_lightDiffuse.x,  obj->_lightDiffuse.y,  obj->_lightDiffuse.z,  obj->_lightDiffuse.w };
                float ambient[4] = { obj->_lightAmbient.x,  obj->_lightAmbient.y,  obj->_lightAmbient.z,  obj->_lightAmbient.w };
                float specular[4] = { obj->_lightSpecular.x, obj->_lightSpecular.y, obj->_lightSpecular.z, obj->_lightSpecular.w };

                if (ImGui::ColorEdit4("Diffuse", diffuse))
                    obj->_lightDiffuse = { diffuse[0],  diffuse[1],  diffuse[2],  diffuse[3] };
                if (ImGui::ColorEdit4("Ambient", ambient))
                    obj->_lightAmbient = { ambient[0],  ambient[1],  ambient[2],  ambient[3] };
                if (ImGui::ColorEdit4("Specular", specular))
                    obj->_lightSpecular = { specular[0], specular[1], specular[2], specular[3] };

                if (obj->_lightType == LightType::Directional)
                {
                    float dir[3] = { obj->_lightDirection.x, obj->_lightDirection.y, obj->_lightDirection.z };
                    if (ImGui::DragFloat3("Direction", dir, 0.01f, -1.f, 1.f))
                        obj->_lightDirection = { dir[0], dir[1], dir[2], 0.f };
                }
                else
                {
                    ImGui::DragFloat("Range", &obj->_lightRange, 0.1f, 0.1f, 1000.f);
                }
            }
            break;

        default:
            break;
        }

        if (changed)
        {
            obj->UpdateWorldMatrix();
        }
    }
    ImGui::End();
    SetOpen(open);
}

Shared<PanelInspector> PanelInspector::Create(SceneContext* ctx, MECamera* camera)
{
    return make_shared<PanelInspector>(ctx, camera);
}
