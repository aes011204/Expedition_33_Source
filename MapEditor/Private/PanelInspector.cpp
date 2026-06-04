#include "pch.h"
#include "PanelInspector.h"
#include "SceneContext.h"
#include "MECamera.h"
#include "CmdTransformObject.h"
#include "CmdDeleteObject.h"
#include "MEWaterActor.h"


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

        if (ImGui::Button("Focus"))
        {
            _camera->FocusOn(obj->_position);
        }

        if (!obj->_isDefaultLight)
        {
            ImGui::SameLine();
            if (ImGui::Button("Delete"))
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
        obj->_type != EObjectType::PostProcessVolume)
    {
        return;
    }

    if (!ImGui::CollapsingHeader("Zone Shape", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

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
            
            DrawMaterialOverrides(obj);
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
            ImGui::DragFloat("Fog Start", &obj->_ppFogStart, 0.1f, 0.f, 1000.f);

            const char* zoneTypes[] = { "AABB", "Sphere" };
            ImGui::Combo("Zone Type", &obj->_ppFogZoneType, zoneTypes, 2);

            if (obj->_ppFogZoneType == 1)
            {
                ImGui::DragFloat("Fog Radius", &obj->_ppFogRadius, 0.1f, 0.f, 1000.f);
            }

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

            int instanceCount = static_cast<int>(obj->_instancedTransforms.size());
            ImGui::LabelText("Instance Count", "%d", instanceCount);

            if (!obj->_instancedTransforms.empty())
            {
                const auto& t = obj->_instancedTransforms[0];
                ImGui::TextDisabled("Instance[0] Pos: (%.2f, %.2f, %.2f)", t._position.x, t._position.y, t._position.z);

                if (ImGui::Button("Focus Instance[0]"))
                {
                    _camera->FocusOn(t._position);
                }
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
            _ctx->_modelOverrides[filename].ExportToNativeMaterial(String::ToString(obj->_modelPath), String::ToString(_ctx->_mapName));
        }
    }
}
