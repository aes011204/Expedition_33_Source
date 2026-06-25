#include "pch.h"
#include "PanelInspector.h"
#include "SceneContext.h"
#include "MECamera.h"
#include "CmdTransformObject.h"
#include "CmdDeleteObject.h"
#include "CmdDeleteInstance.h"
#include "CmdAddInstance.h"
#include "CmdEraseInstances.h"
#include "MEWaterActor.h"
#include "MEStaticMeshActor.h"
#include "MEInstancedMeshActor.h"
#include "CmdConvertToInstanced.h"
#include "CmdConvertAllToStatic.h"
#include "CmdConvertOneToStatic.h"

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

void PanelInspector::Render()
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

        EditorObject* curSelected = _ctx->_selected.get();
        if (curSelected != _prevSelectedShared.get())
        {
            if (_ctx->_boxScatterMode &&
                _ctx->_pendingBoxCount > 0 &&
                _ctx->_pendingBoxTarget != nullptr &&
                _ctx->_pendingBoxTarget != curSelected)
            {
                _boxRestoreTarget = _prevSelectedShared;
                ImGui::OpenPopup("BoxTargetChangedWarning");
            }

            if (curSelected &&
                curSelected->_type == EObjectType::InstancedMesh &&
                !curSelected->_instancedTransforms.empty())
            {
                if (curSelected->_selectedInstanceIdx < 0)
                {
                    curSelected->_selectedInstanceIdx = 0;
                }
            }

            _prevSelectedShared = _ctx->_selected;
        }

        if (ImGui::BeginPopupModal("BoxTargetChangedWarning", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("다른 오브젝트를 선택했습니다.\n대기 중인 인스턴스 %d개를 어떻게 처리할까요?", _ctx->_pendingBoxCount);
            ImGui::Separator();

            if (ImGui::Button("Apply", ImVec2(100, 0)))
            {
                _ctx->ApplyBoxScatter();
                _ctx->_boxScatterMode = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete Object", ImVec2(100, 0)))
            {
                _ctx->DeleteBoxScatter(_ctx->_pendingBoxTarget);
                _ctx->_boxScatterMode = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("취소", ImVec2(100, 0)))
            {
                _ctx->_selected = _boxRestoreTarget;
                _prevSelectedShared = _boxRestoreTarget;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (ImGui::Button("Focus"))
        {
            if (obj->_type == EObjectType::InstancedMesh && obj->_selectedInstanceIdx >= 0 && obj->_selectedInstanceIdx < static_cast<int>(obj->_instancedTransforms.size()))
            {
                _camera->FocusOn(obj->_instancedTransforms[obj->_selectedInstanceIdx]._position);
            }
            else
            {
                _camera->FocusOn(obj->_position);
            }
        }

        if (!obj->_isDefaultLight)
        {
            ImGui::SameLine();
            if (ImGui::Button("오브젝트 지우기"))
            {
                auto it = find(_ctx->_objects.begin(), _ctx->_objects.end(), _ctx->_selected);
                if (it != _ctx->_objects.end())
                {
                    int index = static_cast<int>(distance(_ctx->_objects.begin(), it));
                    _ctx->PushCommand(CmdDeleteObject::Create(_ctx->_selected, index));
                }
                ImGui::End();
                SetOpen(open);
                return;
            }
        }

        DrawTransform(obj);
        DrawZoneShape(obj);
        DrawTypeParams(obj);
    }
    ImGui::End();
    SetOpen(open);
}

void PanelInspector::DrawTransform(Shared<EditorObject>& obj)
{
    if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    if (obj->_type == EObjectType::InstancedMesh)
    {
        int32 instSize = static_cast<int>(obj->_instancedTransforms.size());
        ImGui::Text("Instance Count: %d", instSize);
        ImGui::SameLine();
        if (ImGui::SmallButton("오브젝트 지우기") && !obj->_instancedTransforms.empty())
        {
            obj->_instancedTransforms.clear();
            obj->_instancedParamsDirty = true;
            obj->_selectedInstanceIdx = -1;
        }

        if (obj->_selectedInstanceIdx < 0 || obj->_selectedInstanceIdx >= instSize)
        {
            ImGui::TextDisabled("(Click instance mesh from viewport)");
            return;
        }

        auto& t = obj->_instancedTransforms[obj->_selectedInstanceIdx];
        if (_editInstanceTarget != obj || _editInstanceIndex != obj->_selectedInstanceIdx)
        {
            _instanceTransformEditing = false;
            _editInstanceTarget = nullptr;
            _editInstanceIndex = -1;
        }

        InstanceTransform frameStartTransform = t;
        bool instanceChanged = false;
        ImGui::Text("Selected: [%d]", obj->_selectedInstanceIdx);

        if (ImGui::SmallButton("인스턴스 지우기"))
        {
            _ctx->PushCommand(CmdDeleteInstance::Create(obj, obj->_selectedInstanceIdx));
            return;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("인스턴스 복사하기"))
        {
            const InstanceTransform copied = t;
            _ctx->PushCommand(CmdAddInstance::Create(obj, copied, obj->_selectedInstanceIdx + 1));
            return;
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("정적으로 변환"))
        {
            if (auto cmd = CmdConvertOneToStatic::Create(obj, obj->_selectedInstanceIdx))
            {
                _ctx->PushCommand(cmd);
                return;
            }
        }

        float pos[3] = { t._position.x, t._position.y, t._position.z };
        if (ImGui::DragFloat3("Position", pos, 0.1f))
        {
            t._position = Vec3(pos[0], pos[1], pos[2]);
            _ctx->UpdateSingleInstance(obj.get(), obj->_selectedInstanceIdx);
            instanceChanged = true;
        }
        bool posActive = ImGui::IsItemActive();
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            obj->_instancedParamsDirty = true;
            _ctx->_dirtyRenderHandles  = true;
        }

        Quat q(t._rotation.x, t._rotation.y, t._rotation.z, t._rotation.w);
        Matrix rotMat = Matrix::CreateFromQuaternion(q);
        float pitch = asinf(clamp(-rotMat._32, -1.f, 1.f));
        float yaw = {}, roll = {};
        if (cosf(pitch) > 1e-6f)
        {
            yaw = atan2f(rotMat._31, rotMat._33);
            roll = atan2f(rotMat._12, rotMat._22);
        }
        else
        {
            yaw = atan2f(-rotMat._13, rotMat._11);
        }
        float euler[3] = { XMConvertToDegrees(pitch), XMConvertToDegrees(yaw), XMConvertToDegrees(roll) };
        if (ImGui::DragFloat3("Rotation", euler, 0.5f))
        {
            Quat newQ = Quat::CreateFromYawPitchRoll(
                XMConvertToRadians(euler[1]),
                XMConvertToRadians(euler[0]),
                XMConvertToRadians(euler[2]));
            t._rotation = Vec4(newQ.x, newQ.y, newQ.z, newQ.w);
            _ctx->UpdateSingleInstance(obj.get(), obj->_selectedInstanceIdx);
            instanceChanged = true;
        }
        bool rotActive = ImGui::IsItemActive();
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            obj->_instancedParamsDirty = true;
            _ctx->_dirtyRenderHandles  = true;
        }

        float scl[3] = { t._scale.x, t._scale.y, t._scale.z };
        if (ImGui::DragFloat3("Scale", scl, 0.01f))
        {
            t._scale = Vec3(scl[0], scl[1], scl[2]);
            _ctx->UpdateSingleInstance(obj.get(), obj->_selectedInstanceIdx);
            instanceChanged = true;
        }
        bool sclActive = ImGui::IsItemActive();
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            obj->_instancedParamsDirty = true;
            _ctx->_dirtyRenderHandles  = true;
        }

        const bool anyActive = posActive || rotActive || sclActive;
        if (anyActive && !_instanceTransformEditing)
        {
            _editPrevInstanceTransform = frameStartTransform;
            _editInstanceTarget = obj;
            _editInstanceIndex = obj->_selectedInstanceIdx;
            _instanceTransformEditing = true;
        }
        else if (!anyActive && _instanceTransformEditing &&
            _editInstanceTarget == obj && _editInstanceIndex == obj->_selectedInstanceIdx)
        {
            const bool samePosition =
                _editPrevInstanceTransform._position.x == t._position.x &&
                _editPrevInstanceTransform._position.y == t._position.y &&
                _editPrevInstanceTransform._position.z == t._position.z;
            const bool sameRotation =
                _editPrevInstanceTransform._rotation.x == t._rotation.x &&
                _editPrevInstanceTransform._rotation.y == t._rotation.y &&
                _editPrevInstanceTransform._rotation.z == t._rotation.z &&
                _editPrevInstanceTransform._rotation.w == t._rotation.w;
            const bool sameScale =
                _editPrevInstanceTransform._scale.x == t._scale.x &&
                _editPrevInstanceTransform._scale.y == t._scale.y &&
                _editPrevInstanceTransform._scale.z == t._scale.z;

            if (instanceChanged ||
                !samePosition ||
                !sameRotation ||
                !sameScale)
            {
                _ctx->PushCommand(CmdTransformObject::Create(
                    obj, obj->_selectedInstanceIdx, _editPrevInstanceTransform, t));
            }

            _instanceTransformEditing = false;
            _editInstanceTarget = nullptr;
            _editInstanceIndex = -1;
        }

        return;
    }

    if (_ctx->_selected != _editTarget)
    {
        _transformEditing = false;
        _editTarget = nullptr;
    }

    Vec3 frameStartPos = obj->_position;
    Quat frameStartRot = obj->_rotation;
    Vec3 frameStartScale = obj->_scale;

    bool changed = false;

    float pos[3] = { obj->_position.x, obj->_position.y, obj->_position.z };
    if (ImGui::DragFloat3("Position", pos, 0.1f))
    {
        obj->_position = Vec3(pos[0], pos[1], pos[2]);
        changed = true;
    }
    bool posActive = ImGui::IsItemActive();

    Matrix rotMat = Matrix::CreateFromQuaternion(obj->_rotation);

    float pitch = {}, yaw = {}, roll = {};
    pitch = asinf(clamp(-rotMat._32, -1.f, 1.f));

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

    float euler[3] = { XMConvertToDegrees(pitch), XMConvertToDegrees(yaw), XMConvertToDegrees(roll) };
    if (ImGui::DragFloat3("Rotation", euler, 0.5f))
    {
        obj->_rotation = Quat::CreateFromYawPitchRoll(
            XMConvertToRadians(euler[1]),
            XMConvertToRadians(euler[0]),
            XMConvertToRadians(euler[2]));
        changed = true;
    }
    bool rotActive = ImGui::IsItemActive();

    float scl[3] = { obj->_scale.x, obj->_scale.y, obj->_scale.z };
    if (ImGui::DragFloat3("Scale", scl, 0.01f))
    {
        obj->_scale = Vec3(scl[0], scl[1], scl[2]);
        changed = true;
    }
    bool sclActive = ImGui::IsItemActive();

    bool anyActive = posActive || rotActive || sclActive;
    if (anyActive && !_transformEditing)
    {
        _editPrevPos = frameStartPos;
        _editPrevRot = frameStartRot;
        _editPrevScale = frameStartScale;
        _editTarget = obj;
        _transformEditing = true;
    }
    else if (!anyActive && _transformEditing)
    {
        _ctx->PushCommand(CmdTransformObject::Create(
            obj,
            _editPrevPos, _editPrevRot, _editPrevScale,
            obj->_position, obj->_rotation, obj->_scale));
        _transformEditing = false;
        _editTarget = nullptr;

        if (obj->_type == EObjectType::StaticMesh)
        {
            _ctx->_octreeDirty = true;
            _ctx->_octreeRebuildTimer = SceneContext::_octreeRebuildDelay;
        }
    }

    if (changed)
    {
        obj->UpdateWorldMatrix();
        if (obj->_type == EObjectType::Light)
        {
            _ctx->_lightDirty = true;
        }
    }
}

void PanelInspector::DrawZoneShape(Shared<EditorObject>& obj)
{
    if (obj->_type != EObjectType::EncounterZone &&
        obj->_type != EObjectType::TriggerVolume &&
        obj->_type != EObjectType::CameraZone &&
        obj->_type != EObjectType::PostProcessVolume &&
        obj->_type != EObjectType::ShaftVolume)
    {
        return;
    }

    if (!ImGui::CollapsingHeader("Zone Shape", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    if (obj->_type == EObjectType::PostProcessVolume)
    {
        int shapeIdx = (obj->_zoneShape == EZoneShape::OBB) ? 1 : 0;
        if (ImGui::Combo("Shape", &shapeIdx, "AABB\0OBB\0"))
        {
            obj->_zoneShape = (shapeIdx == 1) ? EZoneShape::OBB : EZoneShape::Box;
        }
    }
    else
    {
        if (obj->_zoneShape == EZoneShape::OBB)
            obj->_zoneShape = EZoneShape::Box;

        int shapeIdx = (obj->_zoneShape == EZoneShape::Sphere) ? 1 : 0;
        if (ImGui::Combo("Shape", &shapeIdx, "Box\0Sphere\0"))
        {
            obj->_zoneShape = (shapeIdx == 1) ? EZoneShape::Sphere : EZoneShape::Box;
        }
    }

    if (obj->_zoneShape != EZoneShape::Sphere)
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

void PanelInspector::DrawTypeParams(Shared<EditorObject>& obj)
{
    auto InputZoneId = [](const char* label, string& id)
        {
            char buf[256] = {};
            strncpy_s(buf, id.c_str(), sizeof(buf) - 1);
            if (ImGui::InputText(label, buf, sizeof(buf)))
            {
                id = buf;
            }
        };

    switch (obj->_type)
    {
    case EObjectType::StaticMesh:
        if (ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen))
        {
            string pathStr = String::ToString(obj->_modelPath);
            ImGui::TextDisabled("%s", pathStr.empty() ? "(없음)" : pathStr.c_str());

            if (!obj->_modelPath.empty())
            {
                if (ImGui::Button("인스턴스로 변환"))
                {
                    if (auto cmd = CmdConvertToInstanced::Create(obj))
                    {
                        _ctx->PushCommand(cmd);
                        return;
                    }
                }
            }

            DrawMaterialOverrides(obj);
        }

        if (ImGui::CollapsingHeader("Render Settings##static", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::Checkbox("Visible##static", &obj->_visible))
            {
                if (obj->_renderHandle)
                {
                    obj->_renderHandle->Set_Visible(obj->_visible);
                }
            }
            ImGui::Checkbox("Cull Enabled##static", &obj->_cullEnabled);
            ImGui::Checkbox("Cast Shadow##static", &obj->_castShadow);
        }
        break;

    case EObjectType::EncounterZone:
        if (ImGui::CollapsingHeader("Encounter Zone", ImGuiTreeNodeFlags_DefaultOpen))
        {
            InputZoneId("EnemyGroupId", obj->_zoneId);
        }
        break;

    case EObjectType::TriggerVolume:
        if (ImGui::CollapsingHeader("Trigger Volume", ImGuiTreeNodeFlags_DefaultOpen))
        {
            InputZoneId("EventId", obj->_zoneId);
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
            ImGui::DragFloat("Base Height", &obj->_ppFogBaseHeight, 0.01f, 0.f, 1000.f);
            ImGui::DragFloat("Top Fade Start", &obj->_ppFogTopFadeStart, 0.01f, 0.f, 1000.f);
            ImGui::DragFloat("Top Height", &obj->_ppFogTopHeight, 0.01f, 0.f, 1000.f);
            ImGui::DragFloat("Edge Fade", &obj->_ppFogEdgeFade, 0.01f, 0.f, 1000.f);
            ImGui::DragFloat("Noise Scale", &obj->_ppFogNoiseScale, 0.0001f, 0.f, 10.f, "%.4f");

            float noiseSpeed[2] = { obj->_ppFogNoiseSpeed.x, obj->_ppFogNoiseSpeed.y };
            if (ImGui::DragFloat2("Noise Speed", noiseSpeed, 0.0001f, -10.f, 10.f, "%.4f"))
            {
                obj->_ppFogNoiseSpeed = { noiseSpeed[0], noiseSpeed[1] };
            }

            ImGui::DragFloat("Noise Strength", &obj->_ppFogNoiseStrength, 0.01f, 0.f, 1.f);
            ImGui::DragFloat("Noise Min", &obj->_ppFogNoiseMin, 0.01f, 0.f, 1.f);
            ImGui::SeparatorText("Global Ground Fog");
            ImGui::DragFloat("Max Distance", &obj->_ppGroundFogMaxDistance, 0.1f, 0.f, 10000.f);
            ImGui::DragFloat("Min Accum Distance", &obj->_ppGroundFogMinAccumulationDistance, 0.1f, 0.f, 10000.f);
            ImGui::DragFloat("Distance Influence", &obj->_ppGroundFogDistanceInfluence, 0.01f, 0.f, 1.f);
            ImGui::DragFloat("Near Fade Distance", &obj->_ppGroundFogNearFadeDistance, 0.1f, 0.f, 10000.f);
            ImGui::DragFloat("Near Min Factor", &obj->_ppGroundFogNearMinFactor, 0.01f, 0.f, 1.f);

            float fogColor[3] = { obj->_ppFogColor.x, obj->_ppFogColor.y, obj->_ppFogColor.z };
            if (ImGui::ColorEdit3("Fog Color", fogColor))
            {
                obj->_ppFogColor = { fogColor[0], fogColor[1], fogColor[2] };
            }
        }
        break;

    case EObjectType::ShaftVolume:
        if (ImGui::CollapsingHeader("Froxel Shaft", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat("Density", &obj->_shaftDensity, 0.01f, 0.f, 10.f);
            ImGui::DragFloat("Edge Fade", &obj->_shaftEdgeFade, 0.01f, 0.f, 1000.f);
            ImGui::DragFloat("Intensity", &obj->_shaftIntensity, 0.01f, 0.f, 100.f);

            float shaftColor[4] = { obj->_shaftLightColor.x, obj->_shaftLightColor.y, obj->_shaftLightColor.z, obj->_shaftLightColor.w };
            if (ImGui::ColorEdit4("Light Color", shaftColor))
            {
                obj->_shaftLightColor = Vec4(shaftColor[0], shaftColor[1], shaftColor[2], shaftColor[3]);
            }
        }
        break;

    case EObjectType::Light:
        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
        {
            int ltIdx = (obj->_lightType == LightType::Point) ? 1 : 0;
            if (obj->_isDefaultLight)
            {
                ImGui::BeginDisabled();
                ImGui::Combo("Type", &ltIdx, _lights, 2);
                ImGui::EndDisabled();
            }
            else
            {
                const char* pointOnly[] = { "Point" };
                int ptIdx = 0;
                ImGui::Combo("Type", &ptIdx, pointOnly, 1);
                obj->_lightType = LightType::Point;
            }

            float diffuse[4] = { obj->_lightDiffuse.x,  obj->_lightDiffuse.y,  obj->_lightDiffuse.z,  obj->_lightDiffuse.w };
            float ambient[4] = { obj->_lightAmbient.x,  obj->_lightAmbient.y,  obj->_lightAmbient.z,  obj->_lightAmbient.w };
            float specular[4] = { obj->_lightSpecular.x, obj->_lightSpecular.y, obj->_lightSpecular.z, obj->_lightSpecular.w };

            if (ImGui::ColorEdit4("Diffuse", diffuse))
            {
                obj->_lightDiffuse = Vec4{ diffuse[0],  diffuse[1],  diffuse[2],  diffuse[3] };
                _ctx->_lightDirty = true;
            }

            if (ImGui::ColorEdit4("Ambient", ambient))
            {
                obj->_lightAmbient = Vec4{ ambient[0],  ambient[1],  ambient[2],  ambient[3] };
                _ctx->_lightDirty = true;
            }

            if (ImGui::ColorEdit4("Specular", specular))
            {
                obj->_lightSpecular = Vec4{ specular[0], specular[1], specular[2], specular[3] };
                _ctx->_lightDirty = true;
            }

            if (obj->_lightType == LightType::Directional)
            {
                float dir[3] = { obj->_lightDirection.x, obj->_lightDirection.y, obj->_lightDirection.z };
                if (ImGui::DragFloat3("Direction", dir, 0.01f, -1.f, 1.f))
                {
                    obj->_lightDirection = Vec4{ dir[0], dir[1], dir[2], 0.f };
                    _ctx->_lightDirty = true;
                }
            }
            else
            {
                if (ImGui::DragFloat("Range", &obj->_lightRange, 0.1f, 0.1f, 1000.f))
                {
                    _ctx->_lightDirty = true;
                }
            }
        }
        break;

    case EObjectType::InstancedMesh:
        if (ImGui::CollapsingHeader("Instanced Mesh", ImGuiTreeNodeFlags_DefaultOpen))
        {
            wstring displayPath = fs::path(obj->_instancedModelPath).filename().wstring();
            string displayStr = String::ToString(displayPath);

            char pathBuf[256] = {};
            strncpy_s(pathBuf, displayStr.c_str(), sizeof(pathBuf) - 1);

            if (ImGui::InputText("Model Path", pathBuf, sizeof(pathBuf)))
            {
                if (strlen(pathBuf) > 0)
                {
                    obj->_instancedModelPath = L"../../../Client/Bin/Resources/Map/"
                        + _ctx->_modelFolder + L"/"
                        + String::ToWString(string(pathBuf));
                }
                else
                {
                    obj->_instancedModelPath = {};
                }
                obj->_instancedParamsDirty = true;
            }

            ImGui::Checkbox("Has Collision", &obj->_instancedHasCollision);

            if (ImGui::Checkbox("Visible##instanced", &obj->_visible))
            {
                for (auto& handle : obj->_instancedChunkHandles)
                {
                    if (handle)
                    {
                        handle->Set_Visible(obj->_visible);
                    }
                }
            }
            ImGui::Checkbox("Cull Enabled##instanced", &obj->_cullEnabled);
            ImGui::Checkbox("Cast Shadow##instanced", &obj->_castShadow);

            ImGui::Separator();
            int32 instCount = static_cast<int32>(obj->_instancedTransforms.size());
            if (ImGui::Button("전체를 정적으로 변환"))
            {
                if (instCount > 50)
                {
                    ImGui::OpenPopup("ConvertAllConfirm");
                }
                else if (auto cmd = CmdConvertAllToStatic::Create(obj))
                {
                    _ctx->PushCommand(cmd);
                    ImGui::End();
                    SetOpen(_isOpen);
                    return;
                }
            }

            if (ImGui::BeginPopupModal("ConvertAllConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("인스턴스 %d개를 정적 메쉬로 변환합니다.\n오브젝트가 많아 성능에 영향을 줄 수 있습니다.\n계속하시겠습니까?", instCount);
                ImGui::Separator();
                if (ImGui::Button("확인", ImVec2(100, 0)))
                {
                    if (auto cmd = CmdConvertAllToStatic::Create(obj))
                    {
                        ImGui::CloseCurrentPopup();
                        ImGui::EndPopup();
                        _ctx->PushCommand(cmd);
                        return;
                    }
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("취소", ImVec2(100, 0)))
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        if (ImGui::CollapsingHeader("Scatter Zone"))
        {
            bool boxModeChanged = ImGui::Checkbox("Box Mode", &_ctx->_boxScatterMode);

            if (boxModeChanged && _ctx->_boxScatterMode)
            {
                _pendingBoxZone = {};
                _pendingBoxZone.center = _ctx->_cursorPos;
                _ctx->_pendingBox = {};
                _ctx->_pendingBox.center = _ctx->_cursorPos;
            }

            if (boxModeChanged && !_ctx->_boxScatterMode)
            {
                if (_ctx->_pendingBoxCount > 0 && _ctx->_pendingBoxTarget == obj.get())
                {
                    ImGui::OpenPopup("BoxModeOffWarning");
                    _ctx->_boxScatterMode = true;
                }
            }

            if (ImGui::BeginPopupModal("BoxModeOffWarning", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("대기 중인 인스턴스 %d개가 삭제됩니다.\n계속하시겠습니까?", _ctx->_pendingBoxCount);
                ImGui::Separator();

                if (ImGui::Button("확인", ImVec2(100, 0)))
                {
                    _ctx->DeleteBoxScatter(obj.get());
                    _ctx->_boxScatterMode = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("취소", ImVec2(100, 0)))
                {
                    _ctx->_boxScatterMode = true;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
            ImGui::Separator();

            if (!_ctx->_boxScatterMode)
            {
                ImGui::DragFloat("Radius", &_pendingZone.radius, 0.5f, 0.5f, 200.f);
                ImGui::DragFloat("Min Spacing", &_pendingZone.minSpacing, 0.1f, 0.f, 50.f);
                ImGui::DragInt("Count", &_pendingZone.count, 1, 1, 500);
                ImGui::DragInt("Seed", &_pendingZone.seed, 1, 0, 99999);
                ImGui::DragFloat("Scale Min", &_pendingZone.scaleMin, 0.01f, 0.01f, 10.f);
                ImGui::DragFloat("Scale Max", &_pendingZone.scaleMax, 0.01f, 0.01f, 10.f);
                ImGui::DragFloat("Y Rot Max", &_pendingZone.yRotMax, 1.f, 0.f, 360.f);

                if (ImGui::Button("Use Cursor as Center"))
                {
                    _pendingZone.center = _ctx->_cursorPos;
                }
                ImGui::SameLine();
                ImGui::Text("(%.1f, %.1f, %1.f)", _pendingZone.center.x, _pendingZone.center.y, _pendingZone.center.z);

                if (ImGui::Button("Generate"))
                {
                    _ctx->ScatterAt(obj.get(), _pendingZone);
                }
            }

            else
            {
                _pendingBoxZone.center = _ctx->_pendingBox.center;
                _ctx->_pendingBox.halfExtentX = _pendingBoxZone.halfExtentX;
                _ctx->_pendingBox.halfExtentZ = _pendingBoxZone.halfExtentZ;

                ImGui::DragFloat("Half Extent X", &_pendingBoxZone.halfExtentX, 0.5f, 0.5f, 500.f);
                ImGui::DragFloat("Half Extent Z", &_pendingBoxZone.halfExtentZ, 0.5f, 0.5f, 500.f);
                ImGui::DragFloat("Min Spacing", &_pendingBoxZone.minSpacing, 0.1f, 0.f, 50.f);
                ImGui::DragInt("Count", &_pendingBoxZone.count, 1, 1, 500);
                ImGui::DragInt("Seed", &_pendingBoxZone.seed, 1, 0, 99999);
                ImGui::DragFloat("Scale Min", &_pendingBoxZone.scaleMin, 0.01f, 0.01f, 10.f);
                ImGui::DragFloat("Scale Max", &_pendingBoxZone.scaleMax, 0.01f, 0.01f, 10.f);
                ImGui::DragFloat("Y Rot Max", &_pendingBoxZone.yRotMax, 1.f, 0.f, 360.f);

                if (ImGui::Button("Use Cursor as Center"))
                {
                    _pendingBoxZone.center = _ctx->_cursorPos;
                    if (_ctx->_pendingBoxCount > 0 && _ctx->_pendingBoxTarget == obj.get())
                    {
                        _ctx->_pendingBox.center = _pendingBoxZone.center;
                        _ctx->SyncBoxScatter(obj.get());
                    }
                }
                ImGui::SameLine();
                ImGui::Text("(%.1f, %.1f, %.1f)", _pendingBoxZone.center.x, _pendingBoxZone.center.y, _pendingBoxZone.center.z);

                if (ImGui::Button("Generate"))
                {
                    _ctx->ScatterAtBox(obj.get(), _pendingBoxZone);
                }

                if (_ctx->_pendingBoxCount > 0 && _ctx->_pendingBoxTarget == obj.get())
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Apply"))
                    {
                        _ctx->ApplyBoxScatter();
                        _ctx->_boxScatterMode = false;
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Clear##box"))
                    {
                        _ctx->DeleteBoxScatter(obj.get());
                    }
                }

            }
        }

        if (ImGui::CollapsingHeader("Erase Zone"))
        {
            ImGui::DragFloat("Radius##erase", &_eraseRadius, 0.5f, 0.5f, 500.f);

            if (ImGui::Button("Use Cursor as Center##erase"))
            {
                _eraseCenter = _ctx->_cursorPos;
            }
            ImGui::SameLine();
            ImGui::Text("(%.1f, %.1f, %.1f)", _eraseCenter.x, _eraseCenter.y, _eraseCenter.z);

            if (ImGui::Button("Erase"))
            {
                _ctx->PushCommand(CmdEraseInstances::Create(obj, _eraseRadius, _eraseCenter));
            }
        }

        break;

    case EObjectType::Terrain:
        if (ImGui::CollapsingHeader("Terrain", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::LabelText("Tiles", "%d", (int)_ctx->_terrainConfig.tiles.size());
            ImGui::LabelText("WorldOffsetX", "%.4f", _ctx->_terrainConfig.worldOffsetX);
            ImGui::LabelText("WorldOffsetY", "%.4f", _ctx->_terrainConfig.worldOffsetY);
            ImGui::LabelText("WorldOffsetZ", "%.4f", _ctx->_terrainConfig.worldOffsetZ);
            ImGui::LabelText("ScaleXZ", "%.6f", _ctx->_terrainConfig.scaleXZ);
            ImGui::LabelText("ScaleY", "%.4f", _ctx->_terrainConfig.scaleY);
        }
        break;

    case EObjectType::Water:
        if (ImGui::CollapsingHeader("Water", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto& desc = obj->_waterDesc;
            auto& p = obj->_waterParams;
            bool rebuildMesh = false;
            bool reloadTex = false;

            if (ImGui::CollapsingHeader("Grid", ImGuiTreeNodeFlags_DefaultOpen))
            {
                int cx = static_cast<int>(desc.countX), cz = static_cast<int>(desc.countZ);
                if (ImGui::InputInt("Count X", &cx))
                {
                    desc.countX = (uint32)max(1, cx);
                    rebuildMesh = true;
                }

                if (ImGui::InputInt("Count Z", &cz))
                {
                    desc.countZ = (uint32)max(1, cz);
                    rebuildMesh = true;
                }

                if (ImGui::DragFloat("Size X", &desc.sizeX, 0.1f, 0.01f, 100.f))
                {
                    rebuildMesh = true;
                }

                if (ImGui::DragFloat("Size Z", &desc.sizeZ, 0.1f, 0.01f, 100.f))
                {
                    rebuildMesh = true;
                }

                int cs = (int)desc.chunkSize;
                if (ImGui::InputInt("Chunk Size", &cs))
                {
                    desc.chunkSize = static_cast<uint32>(max(1, cs));
                    rebuildMesh = true;
                }
            }

            auto DrawTexPath = [&](const char* label, wstring& pathField)
                {
                    string filename = String::ToString(fs::path(pathField).filename().wstring());
                    ImGui::TextDisabled("%s: %s", label, filename.empty() ? "(없음)" : filename.c_str());
                    ImGui::SameLine();

                    string btnLabel = string("Browse##") + label;
                    if (ImGui::SmallButton(btnLabel.c_str()))
                    {
                        ImGui::GetIO().AppFocusLost = true;
                        WCHAR buf[MAX_PATH] = {};
                        OPENFILENAME ofn{};
                        ofn.lStructSize = sizeof(ofn);
                        ofn.lpstrFilter = L"Texture Files\0*.dds;*.png;*.jpg\0All Files\0*.*\0";
                        ofn.lpstrFile = buf;
                        ofn.nMaxFile = MAX_PATH;
                        ofn.lpstrInitialDir = _waterPath;
                        ofn.Flags = OFN_FILEMUSTEXIST;
                        if (GetOpenFileNameW(&ofn))
                        {
                            pathField = buf;
                            reloadTex = true;
                        }
                    }
                };

            if (ImGui::CollapsingHeader("Textures"))
            {
                DrawTexPath("Large Wave Normal", obj->_waterLargeWaveNormalPath);
                DrawTexPath("Small Wave Normal", obj->_waterSmallWaveNormalPath);
                DrawTexPath("Seafoam", obj->_waterSeafoamPath);
                DrawTexPath("Cubemap", obj->_waterCubemapPath);
            }

            if (ImGui::CollapsingHeader("Parameters"))
            {
                bool paramsChanged = false;
                paramsChanged |= ImGui::DragFloat("Large Wave Scale", &p.largeWaveScale, 1.f, 1.f, 1000.f);
                paramsChanged |= ImGui::DragFloat("Large Wave Speed", &p.largeWaveSpeed, 0.001f, 0.f, 10.f);
                paramsChanged |= ImGui::DragFloat("Large Wave Amplify", &p.largeWaveAmplify, 0.01f, 0.f, 5.f);
                paramsChanged |= ImGui::DragFloat("Small Wave Scale", &p.smallWaveScale, 1.f, 1.f, 2000.f);
                paramsChanged |= ImGui::DragFloat("Small Wave Speed", &p.smallWaveSpeed, 0.01f, 0.f, 10.f);
                paramsChanged |= ImGui::DragFloat("Small Wave Amplify", &p.smallWaveAmplify, 0.01f, 0.f, 5.f);
                paramsChanged |= ImGui::DragFloat("Seafoam Scale", &p.seafoamScale, 1.f, 1.f, 2000.f);
                paramsChanged |= ImGui::DragFloat("Seafoam Speed", &p.seafoamSpeed, 0.001f, 0.f, 1.f);
                paramsChanged |= ImGui::DragFloat("Seafoam Height Power", &p.seafoamHeightPower, 0.01f, 0.f, 10.f);
                paramsChanged |= ImGui::DragFloat("Seafoam Height Multiply", &p.seafoamHeightMultiply, 0.01f, 0.f, 20.f);
                paramsChanged |= ImGui::DragFloat("Foam Distortion", &p.foamDistortion, 0.001f, 0.f, 1.f);
                paramsChanged |= ImGui::DragFloat("Depth Fade", &p.depthFade, 0.1f, 0.f, 100.f);
                paramsChanged |= ImGui::DragFloat("Camera Distance Fade", &p.cameraDistanceFade, 1.f, 0.f, 1000.f);
                paramsChanged |= ImGui::DragFloat("Reflection Amount", &p.reflectionAmount, 0.01f, 0.f, 1.f);
                paramsChanged |= ImGui::DragFloat("Luminance Bias", &p.luminanceBias, 0.001f, -1.f, 1.f);
                paramsChanged |= ImGui::ColorEdit4("Water Color", reinterpret_cast<float*>(&p.waterColor));
                paramsChanged |= ImGui::ColorEdit4("Fresnel Color", reinterpret_cast<float*>(&p.fresnelColor));

                if (paramsChanged && obj->_waterRenderHandle)
                {
                    obj->_waterRenderHandle->Set_WaterParams(p);
                }
            }

            if (rebuildMesh || reloadTex)
            {
                obj->_waterRenderHandle = nullptr;
                obj->_waterRenderHandleLoaded = false;
                _ctx->_dirtyRenderHandles = true;
            }
        }
        break;

    default:
        break;
    }
}

Shared<PanelInspector> PanelInspector::Create(SceneContext* ctx, MECamera* camera)
{
    return make_shared<PanelInspector>(ctx, camera);
}

void PanelInspector::DrawMaterialOverrides(Shared<EditorObject>& obj)
{
    if (obj->_type != EObjectType::StaticMesh && obj->_type != EObjectType::InstancedMesh)
    {
        return;
    }

    string pathStr = String::ToString((obj->_type == EObjectType::StaticMesh) ? obj->_modelPath : obj->_instancedModelPath);
    if (pathStr.empty())
    {
        return;
    }

    string filename = fs::path(pathStr).filename().string();

    ImGui::Separator();
    ImGui::Text("Material Overrides");
    
    auto it = _ctx->_modelOverrides.find(filename);
    bool hasOverride = (it != _ctx->_modelOverrides.end() && it->second.hasMaterialCB);
    
    if (ImGui::Checkbox("Enable Custom Material", &hasOverride))
    {
        _ctx->_modelOverrides[filename].hasMaterialCB = hasOverride;
        _ctx->_dirtyPassOverrides = true;
    }
    
    if (hasOverride)
    {
        auto& cb = _ctx->_modelOverrides[filename].materialCB;
        bool matChanged = false;
        
        float baseColor[4] = { cb.baseColorTint.x, cb.baseColorTint.y, cb.baseColorTint.z, cb.baseColorTint.w };
        if (ImGui::ColorEdit4("Base Tint", baseColor))
        {
            cb.baseColorTint = Vec4(baseColor[0], baseColor[1], baseColor[2], baseColor[3]);
            matChanged = true;
        }
        
        float emissiveColor[4] = { cb.emissiveColor.x, cb.emissiveColor.y, cb.emissiveColor.z, cb.emissiveColor.w };
        if (ImGui::ColorEdit4("Emissive Color", emissiveColor))
        {
            cb.emissiveColor = Vec4(emissiveColor[0], emissiveColor[1], emissiveColor[2], emissiveColor[3]);
            matChanged = true;
        }
        
        if (ImGui::DragFloat("Emissive Intensity", &cb.emissiveIntensity, 0.1f, 0.f, 100.f)) matChanged = true;
        if (ImGui::DragFloat("Roughness Scale", &cb.roughnessScale, 0.05f, 0.f, 5.f)) matChanged = true;
        if (ImGui::DragFloat("Metallic Scale", &cb.metalnessScale, 0.05f, 0.f, 5.f)) matChanged = true;
        
        if (matChanged)
        {
            _ctx->_dirtyPassOverrides = true;
        }

        ImGui::Separator();
        if (ImGui::Button("Export to .material.json", ImVec2(-1, 0)))
        {
            const wstring& exportPath = (obj->_type == EObjectType::InstancedMesh) ? obj->_instancedModelPath : obj->_modelPath;
            _ctx->_modelOverrides[filename].ExportToNativeMaterial(String::ToString(exportPath), String::ToString(_ctx->_mapName));
        }
    }
}
