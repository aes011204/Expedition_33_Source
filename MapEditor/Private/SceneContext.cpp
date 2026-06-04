#include "SceneContext.h"
#include "Helper_String.h"
#include "IEditorCommand.h"
#include "GameInstance.h"
#include "PhysX_Manager.h"
#include "MapStaticCollisionBuilder.h"
#include "StaticCollisionData.h"
#include "MEStaticMeshActor.h"
#include "MEInstancedMeshActor.h"
#include <iostream>

namespace
{
    constexpr size_t kCookedCollisionWarnBytes = 256 * 1024;

    Vec4 ToVec4(const json& arr)
    {
        return Vec4(arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>(), arr[3].get<float>());
    }

    json Serialize_CollisionProxy(const ModelCollisionProxy& proxy)
    {
        json node = json::object();
        node["Type"] = static_cast<uint32>(proxy.type);
        node["Name"] = proxy.name;
        node["Center"] = { proxy.center.x, proxy.center.y, proxy.center.z };
        node["Rotation"] = { proxy.rotation.x, proxy.rotation.y, proxy.rotation.z, proxy.rotation.w };
        node["Extents"] = { proxy.extents.x, proxy.extents.y, proxy.extents.z };
        node["Radius"] = proxy.radius;
        node["HalfHeight"] = proxy.halfHeight;

        json positions = json::array();
        for (const Vec3& position : proxy.positions)
            positions.push_back({ position.x, position.y, position.z });
        node["Positions"] = positions;

        node["Indices"] = proxy.indices;
        return node;
    }

    void Write_MapBinaryFile(const string& filePath, const json& sourceRoot)
    {
        fs::path jsonPath(filePath);
        if (jsonPath.extension() != ".json")
            return;

        json binRoot = sourceRoot;
        if (binRoot.contains("MapData") && binRoot["MapData"].contains("ModelCollisions"))
        {
            auto& modelCollisions = binRoot["MapData"]["ModelCollisions"];
            for (auto it = modelCollisions.begin(); it != modelCollisions.end(); ++it)
            {
                auto& collisionEntry = it.value();
                if (!collisionEntry.is_object() ||
                    !collisionEntry.contains("CookedTriangleMeshes") ||
                    !collisionEntry["CookedTriangleMeshes"].is_array())
                {
                    continue;
                }

                json binaryMeshArray = json::array();
                for (auto& base64Mesh : collisionEntry["CookedTriangleMeshes"])
                {
                    if (!base64Mesh.is_string())
                        continue;

                    vector<uint8_t> binaryData = String::Base64_Decode(base64Mesh.get<string>());
                    binaryMeshArray.push_back(json::binary(binaryData));
                }

                collisionEntry["CookedTriangleMeshes"] = std::move(binaryMeshArray);
            }
        }

        fs::path binPath = jsonPath;
        binPath.replace_extension(".bin");

        ofstream ofs(binPath, ios::binary);
        if (!ofs.is_open())
            return;

        vector<uint8_t> msgpack = json::to_msgpack(binRoot);
        ofs.write(reinterpret_cast<const char*>(msgpack.data()), msgpack.size());
    }

    void Serialize_MaterialCB(json& outJson, const Engine::MaterialCB& cb)
    {
        outJson["HasMaterialCB"] = true;
        outJson["BaseColorTint"] = { cb.baseColorTint.x, cb.baseColorTint.y, cb.baseColorTint.z, cb.baseColorTint.w };
        outJson["EmissiveColor"] = { cb.emissiveColor.x, cb.emissiveColor.y, cb.emissiveColor.z, cb.emissiveColor.w };
        outJson["EmissiveIntensity"] = cb.emissiveIntensity;
        outJson["EmissiveMaskMul"] = cb.emissiveMaskMul;
        outJson["NormalStrength"] = cb.normalStrength;
        outJson["OpacityMaskClipValue"] = cb.opacityMaskClipValue;
        outJson["AoScale"] = cb.aoScale;
        outJson["RoughnessScale"] = cb.roughnessScale;
        outJson["MetalnessScale"] = cb.metalnessScale;
        outJson["UvScale"] = { cb.uvScale.x, cb.uvScale.y };
        outJson["UvOffset"] = { cb.uvOffset.x, cb.uvOffset.y };
        outJson["UvRotation"] = cb.uvRotation;
    }

    void Deserialize_MaterialCB(const json& data, Engine::MaterialCB& cb)
    {
        if (data.contains("BaseColorTint"))
        {
            auto& c = data["BaseColorTint"];
            cb.baseColorTint = { c[0], c[1], c[2], c[3] };
        }
        if (data.contains("EmissiveColor"))
        {
            auto& c = data["EmissiveColor"];
            cb.emissiveColor = { c[0], c[1], c[2], c[3] };
        }
        cb.emissiveIntensity = data.value("EmissiveIntensity", 0.f);
        cb.emissiveMaskMul = data.value("EmissiveMaskMul", 1.f);
        cb.normalStrength = data.value("NormalStrength", 1.f);
        cb.opacityMaskClipValue = data.value("OpacityMaskClipValue", 0.3333f);
        cb.aoScale = data.value("AoScale", 1.f);
        cb.roughnessScale = data.value("RoughnessScale", 1.f);
        cb.metalnessScale = data.value("MetalnessScale", 1.f);
        if (data.contains("UvScale"))
        {
            auto& c = data["UvScale"];
            cb.uvScale = { c[0], c[1] };
        }
        if (data.contains("UvOffset"))
        {
            auto& c = data["UvOffset"];
            cb.uvOffset = { c[0], c[1] };
        }
        cb.uvRotation = data.value("UvRotation", 0.f);
    }

    void Export_ObjectJson(const fs::path& baseDir, int32 index, const json& e)
    {
        string objName = "Object_" + to_string(index);
        if (e.contains("Id"))
        {
            string idStr = e["Id"].get<string>();
            if (!idStr.empty())
            {
                objName = idStr;
            }
        }
        string fileName = objName + ".json";
        fs::path outPath = baseDir / fileName;

        ofstream ofs(outPath);
        if (ofs.is_open())
        {
            ofs << e.dump(4);
        }
    }
}

void SceneContext::SaveToFile(const string& filePath)
{
    _currentFilePath = filePath;
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
    json instancedMeshes = json::array();

    unordered_map<string, Shared<ModelCom>> uniqueModels;

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
            if (modelFile.size() >= prefix.size() && modelFile.substr(0, prefix.size()) == prefix)
            {
                modelFile = modelFile.substr(prefix.size());
            }
            else
            {
                modelFile = fs::path(modelFile).filename().wstring();
            }
            e["ModelPath"] = String::ToString(modelFile);
            e["Scale"] = { obj->_scale.x, obj->_scale.y, obj->_scale.z };
            staticMeshes.push_back(e);

            string relativePath = String::ToString(modelFile);
            if (obj->_renderHandle && obj->_renderHandle->GetModel())
            {
                uniqueModels[relativePath] = obj->_renderHandle->GetModel();
            }
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
            if (obj->_isDefaultLight)
            {
                map["DefaultLight"]["Direction"] = { obj->_lightDirection.x, obj->_lightDirection.y, obj->_lightDirection.z, obj->_lightDirection.w };
                map["DefaultLight"]["Diffuse"] = { obj->_lightDiffuse.x,  obj->_lightDiffuse.y,  obj->_lightDiffuse.z,  obj->_lightDiffuse.w };
                map["DefaultLight"]["Ambient"] = { obj->_lightAmbient.x,  obj->_lightAmbient.y,  obj->_lightAmbient.z,  obj->_lightAmbient.w };
                map["DefaultLight"]["Specular"] = { obj->_lightSpecular.x, obj->_lightSpecular.y, obj->_lightSpecular.z, obj->_lightSpecular.w };
            }
            else
            {
                e["LightType"] = (obj->_lightType == LightType::Directional) ? "Directional" : "Point";
                e["Direction"] = { obj->_lightDirection.x, obj->_lightDirection.y, obj->_lightDirection.z, obj->_lightDirection.w };
                e["Range"] = obj->_lightRange;
                e["Diffuse"] = { obj->_lightDiffuse.x,  obj->_lightDiffuse.y,  obj->_lightDiffuse.z,  obj->_lightDiffuse.w };
                e["Ambient"] = { obj->_lightAmbient.x,  obj->_lightAmbient.y,  obj->_lightAmbient.z,  obj->_lightAmbient.w };
                e["Specular"] = { obj->_lightSpecular.x, obj->_lightSpecular.y, obj->_lightSpecular.z, obj->_lightSpecular.w };
                lights.push_back(e);
            }
            break;

        case EObjectType::InstancedMesh:
        {
            wstring modelFile = obj->_instancedModelPath;
            const wstring prefix = L"../../../Client/Bin/Resources/Map/" + _modelFolder + L"/";
            if (modelFile.size() >= prefix.size() && modelFile.substr(0, prefix.size()) == prefix)
            {
                modelFile = modelFile.substr(prefix.size());
            }
            else
            {
                modelFile = fs::path(modelFile).filename().wstring();
            }
            json instances = json::array();
            for (auto& t : obj->_instancedTransforms)
            {
                json inst;
                inst["Position"] = { t._position.x, t._position.y, t._position.z };
                inst["Rotation"] = { t._rotation.x, t._rotation.y, t._rotation.z, t._rotation.w };
                inst["Scale"] = { t._scale.x, t._scale.y, t._scale.z };
                instances.push_back(inst);
            }
            e["ModelPath"] = String::ToString(modelFile);
            e["Instances"] = instances;
            instancedMeshes.push_back(e);

            string relativePath = String::ToString(modelFile);
            if (obj->_instancedRenderHandle && obj->_instancedRenderHandle->GetModel())
            {
                uniqueModels[relativePath] = obj->_instancedRenderHandle->GetModel();
            }
            break;
        }

        default:
            break;
        }
    }

    json modelOverrides = json::object();
    for (auto& [filename, entry] : _modelOverrides)
    {
        json slots = json::object();
        for (auto& [slotIdx, texPath] : entry.textureSlots)
        {
            slots[to_string(slotIdx)] = texPath;
        }

        json overrideJson = {
        {"PassIndex",  entry.passIndex},
        {"ShaderType", static_cast<int>(entry.shaderType)},
        {"TextureSlots", slots},
        };

        if (entry.hasMaterialCB)
        {
            Serialize_MaterialCB(overrideJson, entry.materialCB);
        }

        modelOverrides[filename] = overrideJson;
    }

    json modelCollisions = json::object();
    auto physxManager = GAME->Get_PhysXManager();
    if (physxManager)
    {
        for (auto& [relativePath, model] : uniqueModels)
        {
            Shared<StaticCollisionData> colData = MapStaticCollisionBuilder::Build(model);
            if (!colData)
                continue;

            json collisionEntry = json::object();

            json cookedMeshArray = json::array();
            size_t totalCookedBytes = 0;
            for (auto& mesh : colData->triangleMeshes) // triangle mesh collision은 기존처럼 cooked binary를 저장한다.
            {
                vector<uint8_t> cookedData;
                if (physxManager->Cook_TriangleMesh(mesh, colData->preLocalTransform, cookedData))
                {
                    totalCookedBytes += cookedData.size();
                    string base64Str = String::Base64_Encode(cookedData);
                    cookedMeshArray.push_back(base64Str);
                }
            }

            json primitiveProxyArray = json::array();
            for (const ModelCollisionProxy& proxy : colData->primitiveProxies)
            {
                primitiveProxyArray.push_back(Serialize_CollisionProxy(proxy));
            }

            if (primitiveProxyArray.empty() &&
                totalCookedBytes >= kCookedCollisionWarnBytes)
            {
                LOG_WARN(
                    "[SceneContext::SaveToFile] Large cooked collision saved without primitive proxies. model={}, cookedBytes={}, cookedMeshCount={}",
                    relativePath,
                    totalCookedBytes,
                    cookedMeshArray.size());
            }

            if (cookedMeshArray.empty() && primitiveProxyArray.empty())
            {
                continue;
            }

            collisionEntry["CookedTriangleMeshes"] = cookedMeshArray;
            collisionEntry["PrimitiveProxies"] = primitiveProxyArray;
            modelCollisions[relativePath] = std::move(collisionEntry);
        }
    }

    map["ModelCollisions"] = modelCollisions;
    map["ModelOverrides"] = modelOverrides;
    map["StaticMeshes"] = staticMeshes;
    map["SpawnPoints"] = spawnPoints;
    map["EncounterZones"] = encounterZones;
    map["TriggerVolumes"] = triggerVolumes;
    map["CameraZones"] = cameraZones;
    map["PostProcessVolumes"] = postProcessVols;
    map["Lights"] = lights;
    map["InstancedMeshes"] = instancedMeshes;

    ofstream ofs(filePath);
    ofs << root.dump(4);
    ofs.close();

    Write_MapBinaryFile(filePath, root); // 런타임이 .bin을 우선 로드하므로 저장 시 json/bin을 같이 갱신한다.
}

void SceneContext::LoadFromFile(const string& filePath, bool bMerge)
{
    ifstream ifs(filePath);
    if (!ifs.is_open())
    {
        return;
    }

    json root = json::parse(ifs);
    const json& map = root["MapData"];

    wstring fileModelFolder = String::ToWString(map.value("ModelFolder", string{}));

    if (!bMerge)
    {
        _currentFilePath = filePath;
        _mapName = String::ToWString(map["MapName"].get<string>());
        _modelFolder = fileModelFolder;

        _objects.erase(remove_if(_objects.begin(), _objects.end(), [](const Shared<EditorObject>& o) { return !o->_isDefaultLight; }), _objects.end());
        _staticMeshes.clear();
        _lights.erase(remove_if(_lights.begin(), _lights.end(), [](const Shared<EditorObject>& o) { return !o->_isDefaultLight; }), _lights.end());
        _instancedMeshes.clear();
        _postProcessVolumes.clear();
        _modelOverrides.clear();
        _undoStack.clear();
        _redoStack.clear();
        _selected = nullptr;
    }

    _dirtyRenderHandles = true;
    _lightDirty = true;

    wstring prevFolder = _modelFolder;
    if (bMerge)
    {
        _modelFolder = fileModelFolder;
    }

    if (map.contains("ModelOverrides"))
    {
        for (auto& [filename, data] : map["ModelOverrides"].items())
        {
            ModelOverrideEntry entry;
            entry.passIndex = data.value("PassIndex", -1);
            entry.shaderType = static_cast<ShaderType>(data.value("ShaderType", static_cast<int>(ShaderType::VtxMesh)));
            if (data.contains("TextureSlots"))
            {
                for (auto& [slotStr, texPath] : data["TextureSlots"].items())
                {
                    entry.textureSlots[stoi(slotStr)] = texPath.get<string>();
                }
            }
            if (data.value("HasMaterialCB", false))
            {
                entry.hasMaterialCB = true;
                Deserialize_MaterialCB(data, entry.materialCB);
            }
            _modelOverrides[filename] = entry;
        }
    }

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
            Add(obj);
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
            Add(obj);
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
            Add(obj);
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
            Add(obj);
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
            Add(obj);
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
            Add(obj);
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
                obj->_lightDirection = ToVec4(d);
            }
            if (e.contains("Diffuse"))
            {
                auto& d = e["Diffuse"];
                obj->_lightDiffuse = ToVec4(d);
            }
            if (e.contains("Ambient"))
            {
                auto& a = e["Ambient"];
                obj->_lightAmbient = ToVec4(a);
            }
            if (e.contains("Specular"))
            {
                auto& s = e["Specular"];
                obj->_lightSpecular = ToVec4(s);
            }
            LoadTransform(e, *obj);
            Add(obj);
        }
    }

    if (!bMerge && map.contains("DefaultLight"))
    {
        const json& dl = map["DefaultLight"];
        auto it = find_if(_objects.begin(), _objects.end(), [](const Shared<EditorObject>& o) { return o->_isDefaultLight; });
        if (it != _objects.end())
        {
            auto& obj = *it;
            if (dl.contains("Direction"))
            {
                auto& d = dl["Direction"];
                obj->_lightDirection = ToVec4(d);
            }

            if (dl.contains("Diffuse"))
            {
                auto& d = dl["Diffuse"];
                obj->_lightDiffuse = ToVec4(d);
            }

            if (dl.contains("Ambient"))
            {
                auto& a = dl["Ambient"];
                obj->_lightAmbient = ToVec4(a);
            }

            if (dl.contains("Specular"))
            {
                auto& s = dl["Specular"];
                obj->_lightSpecular = ToVec4(s);
            }
        }
    }

    if (map.contains("InstancedMeshes"))
    {
        for (auto& e : map["InstancedMeshes"])
        {
            auto obj = make_shared<EditorObject>();
            obj->_type = EObjectType::InstancedMesh;
            obj->_name = String::ToWString(e["Id"].get<string>());

            string modelPathStr = e.value("ModelPath", string{});
            if (!modelPathStr.empty())
            {
                obj->_instancedModelPath = L"../../../Client/Bin/Resources/Map/"
                    + _modelFolder + L"/"
                    + String::ToWString(modelPathStr);
            }

            if (e.contains("Instances"))
            {
                for (auto& inst : e["Instances"])
                {
                    InstanceTransform t{};
                    auto p = inst["Position"];
                    t._position = { p[0], p[1], p[2] };

                    auto r = inst["Rotation"];
                    t._rotation = { r[0], r[1], r[2], r[3] };

                    auto s = inst["Scale"];
                    t._scale = { s[0], s[1], s[2] };
                    obj->_instancedTransforms.push_back(t);
                }
            }

            if (e.contains("Position"))
            {
                LoadTransform(e, *obj);
            }
            Add(obj);
        }
    }

    if (bMerge)
    {
        _modelFolder = prevFolder;
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
    case EObjectType::Light:
    {
        string ltStr = j.value("LightType", "Directional");
        obj->_lightType = (ltStr == "Point") ? LightType::Point : LightType::Directional;

        if (obj->_lightType == LightType::Directional)
        {
            return nullptr;
        }

        if (j.contains("Direction"))
        {
            auto& d = j["Direction"];
            obj->_lightDirection = ToVec4(d);
        }

        obj->_lightRange = j.value("Range", 10.f);
        if (j.contains("Diffuse"))
        {
            auto& d = j["Diffuse"];
            obj->_lightDiffuse = ToVec4(d);
        }

        if (j.contains("Ambient"))
        {
            auto& d = j["Ambient"];
            obj->_lightAmbient = ToVec4(d);
        }

        if (j.contains("Specular"))
        {
            auto& d = j["Specular"];
            obj->_lightSpecular = ToVec4(d);
        }
        break;
    }
    case EObjectType::EncounterZone:
    case EObjectType::TriggerVolume:
    case EObjectType::CameraZone:
    case EObjectType::PostProcessVolume:
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

    case MapEditor::EObjectType::InstancedMesh:
    {
        string modelPathStr = j.value("ModelPath", string{});
        if (!modelPathStr.empty())
        {
            obj->_instancedModelPath = L"../../../Client/Bin/Resources/Map/"
                + _modelFolder + L"/"
                + String::ToWString(modelPathStr);
        }
        break;
    }


    case EObjectType::Fail:
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
    if (!obj)
    {
        return;
    }

    _objects.push_back(obj);

    if (obj->_type == EObjectType::StaticMesh)
    {
        _staticMeshes.push_back(obj);
    }
    else if (obj->_type == EObjectType::Light)
    {
        _lights.push_back(obj);
        _lightDirty = true;
    }
    else if (obj->_type == EObjectType::PostProcessVolume)
    {
        _postProcessVolumes.push_back(obj);
    }
    else if (obj->_type == EObjectType::InstancedMesh)
    {
        _instancedMeshes.push_back(obj);
    }

    _dirtyRenderHandles = true;
    _octreeDirty = true;
}

void SceneContext::Remove(const Shared<EditorObject>& obj)
{
    if (!obj || obj->_isDefaultLight)
    {
        return;
    }

    auto erase = [](auto& vec, const Shared<EditorObject>& o)
        {
            auto it = find(vec.begin(), vec.end(), o);
            if (it != vec.end())
            {
                vec.erase(it);
            }
        };

    erase(_objects, obj);

    if (obj->_type == EObjectType::StaticMesh)
    {
        erase(_staticMeshes, obj);
    }
    else if (obj->_type == EObjectType::Light)
    {
        erase(_lights, obj);
        _lightDirty = true;
    }
    else if (obj->_type == EObjectType::PostProcessVolume)
    {
        erase(_postProcessVolumes, obj);
    }
    else if (obj->_type == EObjectType::InstancedMesh)
    {
        erase(_instancedMeshes, obj);
    }


    if (obj == _selected)
    {
        _selected = nullptr;
    }

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
    Add(copy);
    return copy;
}

void SceneContext::ClearScene()
{
    _currentFilePath.clear();
    _objects.erase(remove_if(_objects.begin(), _objects.end(), [](const Shared<EditorObject>& o) { return !o->_isDefaultLight; }), _objects.end());
    _staticMeshes.clear();
    _lights.erase(remove_if(_lights.begin(), _lights.end(), [](const Shared<EditorObject>& o) { return !o->_isDefaultLight; }), _lights.end());
    _instancedMeshes.clear();
    _postProcessVolumes.clear();
    _landscapes.clear();
    _activeLandscapeIdx = -1;
    _selected = nullptr;
    _undoStack.clear();
    _redoStack.clear();
    _dirtyRenderHandles = true;
    _octreeDirty = true;
    _lightDirty = true;
}

void SceneContext::SaveCamData(const string& filePath)
{
    json root;
    json bookmarks = json::array();
    for (int i = 0; i < CameraBookmarkCount; ++i)
    {
        const auto& bm = _cameraBookmarks[i];
        json b;
        b["Valid"] = bm.valid;
        b["Position"] = { bm.position.x, bm.position.y, bm.position.z };
        b["Yaw"] = bm.yaw;
        b["Pitch"] = bm.pitch;
        bookmarks.push_back(b);
    }
    root["CameraBookmarks"] = bookmarks;

    fs::create_directories(fs::path(filePath).parent_path());
    ofstream ofs(filePath);
    ofs << root.dump(4);
}

void SceneContext::LoadCamData(const string& filePath)
{
    ifstream ifs(filePath);
    if (!ifs.is_open())
    {
        return;
    }

    json root = json::parse(ifs);
    if (!root.contains("CameraBookmarks"))
    {
        return;
    }

    const auto& bms = root["CameraBookmarks"];
    for (int i = 0; i < CameraBookmarkCount && i < static_cast<int>(bms.size()); ++i)
    {
        const auto& b = bms[i];
        _cameraBookmarks[i].valid = b.value("Valid", false);
        auto& pos = b["Position"];
        _cameraBookmarks[i].position = { pos[0], pos[1], pos[2] };
        _cameraBookmarks[i].yaw = b.value("Yaw", 0.f);
        _cameraBookmarks[i].pitch = b.value("Pitch", 0.f);
    }
}

void SceneContext::LoadTerrainConfig(const string& filePath)
{
    ifstream ifs(filePath);
    if (!ifs.is_open())
    {
        return;
    }

    json root = json::parse(ifs);
    if (!root.contains("Terrain"))
    {
        return;
    }

    const json& t = root["Terrain"];
    _terrainConfig = {};
    _terrainConfig.heightmapFolder = t.value("HeightmapFolder", string{});
    _terrainConfig.worldOffsetX = t.value("WorldOffsetX", 0.f);
    _terrainConfig.worldOffsetY = t.value("WorldOffsetY", 0.f);
    _terrainConfig.worldOffsetZ = t.value("WorldOffsetZ", 0.f);
    _terrainConfig.scaleXZ = t.value("ScaleXZ", 1.f);
    _terrainConfig.scaleY = t.value("ScaleY", 1.f);

    if (t.contains("Tiles"))
    {
        for (const auto& tile : t["Tiles"])
        {
            TerrainTile tt;
            tt.file = tile.value("File", string{});
            tt.gridX = tile.value("GridX", 0);
            tt.gridY = tile.value("GridY", 0);
            _terrainConfig.tiles.push_back(tt);
        }
    }

    _objects.erase(remove_if(_objects.begin(), _objects.end(),
        [](const Shared<EditorObject>& o) { return o->_type == EObjectType::Terrain; }), _objects.end());

    auto terrainObj = make_shared<EditorObject>();
    terrainObj->_type = EObjectType::Terrain;
    terrainObj->_name = L"Terrain";
    terrainObj->_position = Vec3(_terrainConfig.worldOffsetX, _terrainConfig.worldOffsetY, _terrainConfig.worldOffsetZ);
    terrainObj->_scale = Vec3(_terrainConfig.scaleXZ, _terrainConfig.scaleY, _terrainConfig.scaleXZ);
    terrainObj->_rotation = Quat::Identity;
    terrainObj->UpdateWorldMatrix();
    Add(terrainObj);

    _terrainDirty = true;
}

void SceneContext::Export_MapInstance(const string& filePath)
{
    fs::path jsonPath(filePath);
    string mapName = jsonPath.stem().string();

    fs::path root = fs::path(_mapSavePath) / "MapInstance" / mapName;

    fs::path staticDir = root / "StaticMesh";
    fs::path instanceDir = root / "InstanceMesh";
    fs::path waterDir = root / "Water";
    fs::path fogDir = root / "Fog";
    fs::path etcDir = root / "ETC";

    if(!fs::exists(root))
        fs::create_directories(root);

    if (fs::exists(root))
    {
        if (!fs::exists(staticDir))
            fs::create_directories(staticDir);

        if (!fs::exists(instanceDir))
            fs::create_directories(instanceDir);

        if (!fs::exists(waterDir))
            fs::create_directories(waterDir);

        if (!fs::exists(fogDir))
            fs::create_directories(fogDir);

        if (!fs::exists(etcDir))
            fs::create_directories(etcDir);
    }

    int32 st_idx{}, inst_idx{}, water_idx{}, fog_idx{}, etc_idx{};

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
            const wstring prefix = String::ToWString(_mapSavePath) + _modelFolder + L"/";

            if (modelFile.size() >= prefix.size() && modelFile.substr(0, prefix.size()) == prefix)
            {
                modelFile = modelFile.substr(prefix.size());
            }
            else
            {
                modelFile = fs::path(modelFile).filename().wstring();
            }
            e["ModelPath"] = String::ToString(modelFile);
            e["Scale"] = { obj->_scale.x, obj->_scale.y, obj->_scale.z };
            Export_ObjectJson(staticDir, st_idx++, e);
            break;
        }
        case EObjectType::InstancedMesh:
        {
            wstring modelFile = obj->_instancedModelPath;
            const wstring prefix = String::ToWString(_mapSavePath) + _modelFolder + L"/";
            if (modelFile.size() >= prefix.size() && modelFile.substr(0, prefix.size()) == prefix)
            {
                modelFile = modelFile.substr(prefix.size());
            }
            else
            {
                modelFile = fs::path(modelFile).filename().wstring();
            }
            e["ModelPath"] = String::ToString(modelFile);
            json instArr = json::array();
            for (auto& transform : obj->_instancedTransforms)
            {
                json instJson;
                instJson["Position"] = { transform._position.x, transform._position.y, transform._position.z };
                instJson["Rotation"] = { transform._rotation.x, transform._rotation.y, transform._rotation.z, transform._rotation.w };
                instJson["Scale"] = { transform._scale.x, transform._scale.y, transform._scale.z };
                instArr.push_back(instJson);
            }
            e["Instances"] = instArr;
            Export_ObjectJson(instanceDir, inst_idx++, e);
            break;
        }
        default:
            Export_ObjectJson(etcDir, etc_idx++, e);
            break;
        }
    }

}

