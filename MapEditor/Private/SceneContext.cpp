#include "SceneContext.h"
#include "Helper_String.h"
#include "IEditorCommand.h"

void SceneContext::SaveToFile(const string& filePath)
{
    json root;
    json& map = root["MapData"];

    map["MapName"] = String::ToString(_mapName);
    map["ModelFolder"] = String::ToString(_modelFolder);

    json staticMeshes = json::array();
    json spawnPoints = json::array();
    json encounterZones = json::array();
    json triggerVolumes = json::array();
    json cameraZones = json::array();
    json postProcessVols = json::array();
    json lights = json::array();

    for (auto& obj : _objects)
    {
        json e;
        e["Id"] = String::ToString(obj->_name);
        e["Position"] = { obj->_position.x, obj->_position.y, obj->_position.z };
        e["Rotation"] = { obj->_rotation.x, obj->_rotation.y, obj->_rotation.z, obj->_rotation.w };

        switch (obj->_type)
        {
        case EObjectType::StaticMesh:
        {
            wstring modelFile = obj->_modelPath;
            const wstring prefix = L"../../../Client/Bin/Resources/Map/" + _modelFolder + L"/";
            if (modelFile.substr(0, prefix.size()) == prefix)
            {
                modelFile = modelFile.substr(prefix.size());
            }
            e["ModelPath"] = String::ToString(modelFile);
            e["Scale"] = { obj->_scale.x, obj->_scale.y, obj->_scale.z };
            staticMeshes.push_back(e);
            break;
        }

        case EObjectType::SpawnPoint:
            spawnPoints.push_back(e);
            break;

        case EObjectType::EncounterZone:
            e["Shape"] = (obj->_zoneShape == EZoneShape::Box) ? "Box" : "Sphere";
            e["Extent"] = { obj->_zoneExtent.x, obj->_zoneExtent.y, obj->_zoneExtent.z };
            e["Radius"] = obj->_zoneRadius;
            e["EnemyGroupId"] = obj->_zoneId;
            encounterZones.push_back(e);
            break;

        case EObjectType::TriggerVolume:
            e["Shape"] = (obj->_zoneShape == EZoneShape::Box) ? "Box" : "Sphere";
            e["Extent"] = { obj->_zoneExtent.x, obj->_zoneExtent.y, obj->_zoneExtent.z };
            e["Radius"] = obj->_zoneRadius;
            e["EventId"] = obj->_zoneId;
            triggerVolumes.push_back(e);
            break;

        case EObjectType::CameraZone:
            e["Shape"] = (obj->_zoneShape == EZoneShape::Box) ? "Box" : "Sphere";
            e["Extent"] = { obj->_zoneExtent.x, obj->_zoneExtent.y, obj->_zoneExtent.z };
            e["Radius"] = obj->_zoneRadius;
            e["FOV"] = obj->_cameraFov;
            e["FixedAngle"] = obj->_cameraFixedAngle;
            e["TransitionTime"] = obj->_cameraTransitionTime;
            cameraZones.push_back(e);
            break;

        case EObjectType::PostProcessVolume:
            e["Shape"] = (obj->_zoneShape == EZoneShape::Box) ? "Box" : "Sphere";
            e["Extent"] = { obj->_zoneExtent.x, obj->_zoneExtent.y, obj->_zoneExtent.z };
            e["Radius"] = obj->_zoneRadius;
            e["BloomIntensity"] = obj->_ppBloomIntensity;
            e["ExposureMin"] = obj->_ppExposureMin;
            e["ExposureMax"] = obj->_ppExposureMax;
            e["FogDensity"] = obj->_ppFogDensity;
            e["FogColor"] = { obj->_ppFogColor.x, obj->_ppFogColor.y, obj->_ppFogColor.z };
            postProcessVols.push_back(e);
            break;

        case EObjectType::Light:
            e["LightType"] = (obj->_lightType == LightType::Directional) ? "Directional" : "Point";
            e["Direction"] = { obj->_lightDirection.x, obj->_lightDirection.y, obj->_lightDirection.z, obj->_lightDirection.w };
            e["Range"] = obj->_lightRange;
            e["Diffuse"] = { obj->_lightDiffuse.x,  obj->_lightDiffuse.y,  obj->_lightDiffuse.z,  obj->_lightDiffuse.w };
            e["Ambient"] = { obj->_lightAmbient.x,  obj->_lightAmbient.y,  obj->_lightAmbient.z,  obj->_lightAmbient.w };
            e["Specular"] = { obj->_lightSpecular.x, obj->_lightSpecular.y, obj->_lightSpecular.z, obj->_lightSpecular.w };
            lights.push_back(e);
            break;

        default:
            break;
        }
    }

    map["StaticMeshes"] = staticMeshes;
    map["SpawnPoints"] = spawnPoints;
    map["EncounterZones"] = encounterZones;
    map["TriggerVolumes"] = triggerVolumes;
    map["CameraZones"] = cameraZones;
    map["PostProcessVolumes"] = postProcessVols;
    map["Lights"] = lights;

    ofstream ofs(filePath);
    ofs << root.dump(4);
}

void SceneContext::LoadFromFile(const string& filePath)
{
    ifstream ifs(filePath);
    if (!ifs.is_open())
    {
        return;
    }

    json root = json::parse(ifs);
    const json& map = root["MapData"];

    _mapName = String::ToWString(map["MapName"].get<string>());
    _modelFolder = String::ToWString(map.value("ModelFolder", string{}));

    _objects.clear();
    _dirtyRenderHandles = true;
    _selected = nullptr;

    _undoStack.clear();
    _redoStack.clear();

    auto LoadTransform = [](const json& e, EditorObject& obj) {
        auto& pos = e["Position"];
        obj._position = { pos[0], pos[1], pos[2] };
        auto& rot = e["Rotation"];
        obj._rotation = { rot[0], rot[1], rot[2], rot[3] };
        if (e.contains("Scale"))
        {
            auto& s = e["Scale"];
            obj._scale = { s[0], s[1], s[2] };
        }
        obj.UpdateWorldMatrix();
        };

    auto LoadZoneShape = [](const json& e, EditorObject& obj) {
        string shape = e.value("Shape", "Box");
        obj._zoneShape = magic_enum::enum_cast<EZoneShape>(shape).value_or(EZoneShape::Box);
        if (e.contains("Extent"))
        {
            auto& ext = e["Extent"];
            obj._zoneExtent = { ext[0], ext[1], ext[2] };
        }
        obj._zoneRadius = e.value("Radius", 5.f);
        };

    if (map.contains("StaticMeshes"))
    {
        for (auto& e : map["StaticMeshes"])
        {
            auto obj = make_shared<EditorObject>();
            obj->_type = EObjectType::StaticMesh;
            obj->_name = String::ToWString(e["Id"].get<string>());

            string modelPathStr = e.value("ModelPath", string{});
            if (modelPathStr.empty())
            {
                continue;
            }
            obj->_modelPath = L"../../../Client/Bin/Resources/Map/"
                + _modelFolder + L"/"
                + String::ToWString(modelPathStr);

            LoadTransform(e, *obj);
            _objects.push_back(obj);
        }
    }

    if (map.contains("SpawnPoints"))
    {
        for (auto& e : map["SpawnPoints"])
        {
            auto obj = make_shared<EditorObject>();
            obj->_type = EObjectType::SpawnPoint;
            obj->_name = String::ToWString(e["Id"].get<string>());
            LoadTransform(e, *obj);
            _objects.push_back(obj);
        }
    }

    if (map.contains("EncounterZones"))
    {
        for (auto& e : map["EncounterZones"])
        {
            auto obj = make_shared<EditorObject>();
            obj->_type = EObjectType::EncounterZone;
            obj->_name = String::ToWString(e["Id"].get<string>());
            obj->_zoneId = e.value("EnemyGroupId", string{});
            LoadTransform(e, *obj);
            LoadZoneShape(e, *obj);
            _objects.push_back(obj);
        }
    }

    if (map.contains("TriggerVolumes"))
    {
        for (auto& e : map["TriggerVolumes"])
        {
            auto obj = make_shared<EditorObject>();
            obj->_type = EObjectType::TriggerVolume;
            obj->_name = String::ToWString(e["Id"].get<string>());
            obj->_zoneId = e.value("EventId", string{});
            LoadTransform(e, *obj);
            LoadZoneShape(e, *obj);
            _objects.push_back(obj);
        }
    }

    if (map.contains("CameraZones"))
    {
        for (auto& e : map["CameraZones"])
        {
            auto obj = make_shared<EditorObject>();
            obj->_type = EObjectType::CameraZone;
            obj->_name = String::ToWString(e["Id"].get<string>());
            obj->_cameraFov = e.value("FOV", 60.f);
            obj->_cameraFixedAngle = e.value("FixedAngle", false);
            obj->_cameraTransitionTime = e.value("TransitionTime", 0.5f);
            LoadTransform(e, *obj);
            LoadZoneShape(e, *obj);
            _objects.push_back(obj);
        }
    }

    if (map.contains("PostProcessVolumes"))
    {
        for (auto& e : map["PostProcessVolumes"])
        {
            auto obj = make_shared<EditorObject>();
            obj->_type = EObjectType::PostProcessVolume;
            obj->_name = String::ToWString(e["Id"].get<string>());
            obj->_ppBloomIntensity = e.value("BloomIntensity", 1.f);
            obj->_ppExposureMin = e.value("ExposureMin", 0.f);
            obj->_ppExposureMax = e.value("ExposureMax", 2.f);
            obj->_ppFogDensity = e.value("FogDensity", 0.f);
            if (e.contains("FogColor"))
            {
                auto& c = e["FogColor"];
                obj->_ppFogColor = { c[0], c[1], c[2] };
            }
            LoadTransform(e, *obj);
            LoadZoneShape(e, *obj);
            _objects.push_back(obj);
        }
    }

    if (map.contains("Lights"))
    {
        for (auto& e : map["Lights"])
        {
            auto obj = make_shared<EditorObject>();
            obj->_type = EObjectType::Light;
            obj->_name = String::ToWString(e["Id"].get<string>());
            string lt = e.value("LightType", "Directional");
            obj->_lightType = (lt == "Point") ? LightType::Point : LightType::Directional;
            obj->_lightRange = e.value("Range", 10.f);
            if (e.contains("Direction"))
            {
                auto& d = e["Direction"];
                obj->_lightDirection = { d[0], d[1], d[2], d[3] };
            }
            if (e.contains("Diffuse"))
            {
                auto& d = e["Diffuse"];
                obj->_lightDiffuse = { d[0], d[1], d[2], d[3] };
            }
            if (e.contains("Ambient"))
            {
                auto& a = e["Ambient"];
                obj->_lightAmbient = { a[0], a[1], a[2], a[3] };
            }
            if (e.contains("Specular"))
            {
                auto& s = e["Specular"];
                obj->_lightSpecular = { s[0], s[1], s[2], s[3] };
            }
            LoadTransform(e, *obj);
            _objects.push_back(obj);
        }
    }
    _octreeDirty = true;
}

Shared<EditorObject> SceneContext::SpawnObject(const wstring& modelPath, const Vec3& pos)
{
    auto obj = make_shared<EditorObject>();

    obj->_type = EObjectType::StaticMesh;
    obj->_modelPath = modelPath;
    obj->_position = pos;
    obj->_scale = Vec3(1.f, 1.f, 1.f);
    obj->_rotation = Quat::Identity;

    const wstring filename = fs::path(modelPath).stem().wstring();
    obj->_name = filename.empty() ? L"SM_New" : filename;

    obj->_worldMatrix = Matrix::CreateScale(obj->_scale)
        * Matrix::CreateFromQuaternion(obj->_rotation)
        * Matrix::CreateTranslation(obj->_position);

    _dirtyRenderHandles = true;
    Add(obj);
    return obj;
}

Shared<EditorObject> SceneContext::SpawnFromPrefab(const string& prefabPath, const Vec3& pos)
{
    ifstream ifs(prefabPath);
    if (!ifs.is_open())
    {
        return nullptr;
    }

    json j = json::parse(ifs);

    string typeName = j.value("Type", "StaticMesh");
    EObjectType type = ObjectTypeFromName(typeName);
    if(type == EObjectType::Fail)
    {
        return nullptr;
    }

    string nameStr = j.value("Name", typeName);
    wstring name = String::ToWString(nameStr) + L"_" + to_wstring(_objects.size());

    auto obj = EditorObject::Create(name, type);
    obj->_position = pos;

    switch (type)
    {
    case MapEditor::EObjectType::Light:
    {
        string ltStr = j.value("LightType", "Directional");
        obj->_lightType = (ltStr == "Point") ? LightType::Point : LightType::Directional;

        if (j.contains("Direction"))
        {
            auto& d = j["Direction"];
            obj->_lightDirection = { d[0], d[1], d[2], d[3] };
        }

        obj->_lightRange = j.value("Range", 10.f);
        if (j.contains("Diffuse"))
        {
            auto& d = j["Diffuse"];
            obj->_lightDiffuse = { d[0], d[1], d[2], d[3] };
        }

        if (j.contains("Ambient"))
        {
            auto& d = j["Ambient"];
            obj->_lightAmbient = { d[0], d[1], d[2], d[3] };
        }

        if (j.contains("Specular"))
        {
            auto& d = j["Specular"];
            obj->_lightSpecular = { d[0], d[1], d[2], d[3] };
        }
        break;
    }
    case MapEditor::EObjectType::EncounterZone:
    case MapEditor::EObjectType::TriggerVolume:
    case MapEditor::EObjectType::CameraZone:
    case MapEditor::EObjectType::PostProcessVolume:
    {
        string shapeStr = j.value("Shape", "Box");
        obj->_zoneShape = (shapeStr == "Sphere") ? EZoneShape::Sphere : EZoneShape::Box;

        if (j.contains("Extent"))
        {
            auto& ext = j["Extent"];
            obj->_zoneExtent = { ext[0], ext[1], ext[2] };
        }
        obj->_zoneRadius = j.value("Radius", 5.f);

        if (type == EObjectType::EncounterZone)
        {
            obj->_zoneId = j.value("EnemyGroupId", "");
        }
        else if (type == EObjectType::TriggerVolume)
        {
            obj->_zoneId = j.value("EventId", "");
        }
        else if (type == EObjectType::CameraZone)
        {
            obj->_cameraFov = j.value("FOV", 60.f);
            obj->_cameraFixedAngle = j.value("FixedAngle", false);
            obj->_cameraTransitionTime = j.value("TransitionTime", 0.5f);
        }
        else if (type == EObjectType::PostProcessVolume)
        {
            obj->_ppBloomIntensity = j.value("BloomIntensity", 1.f);
            obj->_ppExposureMin = j.value("ExposureMin", 0.f);
            obj->_ppExposureMax = j.value("ExposureMax", 2.f);
            obj->_ppFogDensity = j.value("FogDensity", 0.f);
            if (j.contains("FogColor"))
            {
                auto& fc = j["FogColor"];
                obj->_ppFogColor = { fc[0], fc[1], fc[2] };
            }
        }
        break;
    }
    case MapEditor::EObjectType::Fail:
        break;
    default:
        break;
    }

    obj->UpdateWorldMatrix();
    Add(obj);
    return obj;
}

void SceneContext::PushCommand(Shared<IEditorCommand> cmd)
{
    if (!cmd)
    {
        return;
    }

    cmd->Execute(*this);

    _undoStack.push_back(move(cmd));
    if (_undoStack.size() > 50)
    {
        _undoStack.erase(_undoStack.begin());
    }

    _redoStack.clear();
}

void SceneContext::UndoCommand()
{
    if (_undoStack.empty())
    {
        return;
    }

    auto cmd = _undoStack.back();
    _undoStack.pop_back();
    cmd->Undo(*this);
    _redoStack.push_back(move(cmd));
}

void SceneContext::RedoCommand()
{
    if (_redoStack.empty())
    {
        return;
    }

    auto cmd = _redoStack.back();
    _redoStack.pop_back();
    cmd->Execute(*this);
    _undoStack.push_back(move(cmd));
}

void SceneContext::Add(Shared<EditorObject> obj)
{
    if (obj)
    {
        _objects.push_back(move(obj));
        _dirtyRenderHandles = true;
        _octreeDirty = true;
    }
}

void SceneContext::Remove(const Shared<EditorObject>& obj)
{
    if (!obj)
    {
        return;
    }
    auto it = find(_objects.begin(), _objects.end(), obj);
    if (it == _objects.end())
    {
        return;
    }

    if (obj == _selected)
    {
        _selected = nullptr;
    }
    _objects.erase(it);
    _octreeDirty = true;
}

Shared<EditorObject> SceneContext::Duplicate(const Shared<EditorObject>& src)
{
    if (!src)
    {
        return nullptr;
    }
    auto copy = make_shared<EditorObject>(*src);
    copy->_name = src->_name + L"_Copy";
    _objects.push_back(copy);
    _dirtyRenderHandles = true;
    _octreeDirty = true;
    return copy;
}

