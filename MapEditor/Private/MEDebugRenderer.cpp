#include "MEDebugRenderer.h"
#include "SceneContext.h"
#include "GameInstance.h"

void MapEditor::MEDebugRenderer::Draw(const SceneContext& scene)
{
    for (auto& obj : scene._objects)
    {
        const bool isSelected = (obj == scene._selected);
        const Color color = isSelected ? Color(1.f, 1.f, 0.f, 1.f) : Color(0.f, 1.f, 0.f, 1.f);

        switch (obj->_type)
        {
        case EObjectType::EncounterZone:
        case EObjectType::TriggerVolume:
        case EObjectType::CameraZone:
        case EObjectType::PostProcessVolume:
        case EObjectType::ShaftVolume:
        {
            if (obj->_zoneShape == EZoneShape::Box)
            {
                DebugBoxDesc desc;
                desc.center = obj->_position;
                desc.extents = obj->_zoneExtent;
                desc.rotation = obj->_rotation;
                desc.style.color = color;
                desc.style.depthEnabled = false;
                GAME->Draw_DebugBox(desc);
            }
            else
            {
                DebugSphereDesc desc;
                desc.center = obj->_position;
                desc.radius = obj->_zoneRadius;
                desc.style.color = color;
                desc.style.depthEnabled = false;
                GAME->Draw_DebugSphere(desc);
            }
            break;
        }
        case EObjectType::SpawnPoint:
        {
            DebugSphereDesc sphere;
            sphere.center = obj->_position;
            sphere.radius = 0.3f;
            sphere.style.color = isSelected ? Color(1.f, 1.f, 0.f, 1.f) : Color(0.f, 0.5f, 1.f, 1.f);
            sphere.style.depthEnabled = false;
            GAME->Draw_DebugSphere(sphere);

            DebugLineDesc line;
            line.start = obj->_position;
            line.end = obj->_position + Vec3(0.f, 1.f, 0.f);
            line.style.color = sphere.style.color;
            line.style.depthEnabled = false;
            GAME->Draw_DebugLine(line);
            break;
        }
        case EObjectType::Light:
        {
            DebugSphereDesc sphere;
            sphere.center = obj->_position;
            sphere.radius = 0.2f;
            sphere.style.color = isSelected ? Color(1.f, 1.f, 0.f, 1.f) : Color(1.f, 0.9f, 0.3f, 1.f);
            sphere.style.depthEnabled = false;
            GAME->Draw_DebugSphere(sphere);

            if (obj->_lightType == LightType::Directional)
            {
                DebugLineDesc line;
                line.start = obj->_position;
                line.end = obj->_position + Vec3(obj->_lightDirection.x, obj->_lightDirection.y, obj->_lightDirection.z) * 2.f;
                line.style.color = sphere.style.color;
                line.style.depthEnabled = false;
                GAME->Draw_DebugLine(line);
            }
            else
            {
                DebugSphereDesc range;
                range.center = obj->_position;
                range.radius = obj->_lightRange;
                range.style.color = Color(1.f, 0.9f, 0.3f, 0.3f);
                range.style.depthEnabled = false;
                GAME->Draw_DebugSphere(range);
            }
            break;
        }
        default:
            break;
        }
    }

    if (scene._cursorVisible)
    {
        auto line = [&](Vec3 a, Vec3 b)
            {
                DebugLineDesc d;
                d.start = a;
                d.end = b;
                d.style.color = _lineCol;
                d.style.depthEnabled = false;
                GAME->Draw_DebugLine(d);
            };

        line(scene._cursorPos - Vec3(_lineLen, 0.f, 0.f), scene._cursorPos + Vec3(_lineLen, 0.f, 0.f));
        line(scene._cursorPos - Vec3(0.f, _lineLen, 0.f), scene._cursorPos + Vec3(0.f, _lineLen, 0.f));
        line(scene._cursorPos - Vec3(0.f, 0.f, _lineLen), scene._cursorPos + Vec3(0.f, 0.f, _lineLen));
    }
}

Unique<MapEditor::MEDebugRenderer> MapEditor::MEDebugRenderer::Create()
{
    return make_unique<MEDebugRenderer>();
}
