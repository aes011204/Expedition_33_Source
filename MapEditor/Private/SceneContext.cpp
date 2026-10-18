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

    fs::path Resolve_MapResourceRoot()
    {
        fs::path repoRoot = fs::path(__FILE__).parent_path().parent_path().parent_path();
        fs::path mapRoot = repoRoot / "Client" / "Bin" / "Resources" / "Map";

        error_code ec;
        fs::path resolved = fs::weakly_canonical(mapRoot, ec);
        return (!ec && !resolved.empty()) ? resolved : mapRoot;
    }

    string Resolve_MapResourceRootString()
    {
        return Resolve_MapResourceRoot().generic_string() + "/";
    }

    wstring Resolve_MapResourceRootWString()
    {
        return Resolve_MapResourceRoot().wstring() + L"\\";
    }

    string Make_SplitInstanceModelPath(const wstring& modelPathW)
    {
        if (modelPathW.empty())
            return {};

        const fs::path modelPath(modelPathW);
        const wstring mapSavePrefix = String::ToWString(_mapSavePath);
        if (modelPathW.size() >= mapSavePrefix.size() && modelPathW.substr(0, mapSavePrefix.size()) == mapSavePrefix)
            return fs::path(modelPathW.substr(mapSavePrefix.size())).generic_string();

        const wstring absolutePrefix = Resolve_MapResourceRootWString();
        if (modelPathW.size() >= absolutePrefix.size() && modelPathW.substr(0, absolutePrefix.size()) == absolutePrefix)
            return fs::path(modelPathW.substr(absolutePrefix.size())).generic_string();

        const fs::path mapRoot = Resolve_MapResourceRoot();
        error_code ec;
        const fs::path normalizedMapRoot = fs::weakly_canonical(mapRoot, ec);
        ec.clear();
        fs::path absoluteModelPath = fs::absolute(modelPath, ec);
        if (ec || absoluteModelPath.empty())
            absoluteModelPath = modelPath;

        ec.clear();
        const fs::path normalizedModelPath = fs::weakly_canonical(absoluteModelPath, ec);
        if (!ec && !normalizedMapRoot.empty() && !normalizedModelPath.empty())
        {
            const fs::path rel = normalizedModelPath.lexically_relative(normalizedMapRoot);
            if (!rel.empty() && rel.begin()->wstring() != L"..")
                return rel.generic_string();
        }

        return modelPath.filename().generic_string();
    }

    wstring Resolve_SplitInstanceImportModelPath(const fs::path& mapRoot, const string& mapName, const string& modelPathStr)
    {
        if (modelPathStr.empty())
            return {};

        fs::path modelPath(modelPathStr);
        if (modelPath.is_absolute())
            return modelPath.wstring();

        if (modelPath.has_parent_path())
        {
            auto first = modelPath.begin();
            if (first != modelPath.end() && first->string() == mapName)
                return (mapRoot / modelPath).wstring();

            error_code ec;
            if (fs::exists(mapRoot / modelPath, ec))
                return (mapRoot / modelPath).wstring();

            ec.clear();
            if (fs::exists(mapRoot / mapName / modelPath, ec))
                return (mapRoot / mapName / modelPath).wstring();
        }

        error_code ec;
        const fs::path mapNamePath = mapRoot / mapName / modelPath;
        if (fs::exists(mapNamePath, ec))
            return mapNamePath.wstring();

        return mapNamePath.wstring();
    }

    Vec4 ToVec4(const json& arr)
    {
        return Vec4(arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>(), arr[3].get<float>());
    }

    const char* ZoneShapeName(EZoneShape shape)
    {
        switch (shape)
        {
        case EZoneShape::OBB:
            return "OBB";
        case EZoneShape::Sphere:
            return "Sphere";
        case EZoneShape::Box:
        default:
            return "Box";
        }
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
        if (binRoot.contains("MapData") && binRoot["MapData"].is_object())
            binRoot["MapData"].erase("ModelCollisions");

        fs::path binPath = jsonPath;
        binPath.replace_extension(".bin");

        ofstream ofs(binPath, ios::binary);
        if (!ofs.is_open())
            return;

        vector<uint8_t> msgpack = json::to_msgpack(binRoot);
        ofs.write(reinterpret_cast<const char*>(msgpack.data()), msgpack.size());
    }

    fs::path Make_ModelCollisionBinaryPath(const wstring& modelPath)
    {
        fs::path collisionPath(modelPath);
        collisionPath.replace_extension(".collision.bin");
        return collisionPath;
    }

    bool Write_ModelCollisionBinary(
        const wstring& modelPath,
        const Shared<ModelCom>& model)
    {
        if (modelPath.empty() || model == nullptr)
            return false;

        const fs::path collisionPath = Make_ModelCollisionBinaryPath(modelPath);
        if (fs::exists(collisionPath))
            return true;

        auto physxManager = GAME->Get_PhysXManager();
        if (!physxManager)
            return false;

        Shared<StaticCollisionData> colData = MapStaticCollisionBuilder::Build(model);
        if (!colData)
            return false;

        json cookedMeshArray = json::array();
        size_t totalCookedBytes = 0;
        for (auto& mesh : colData->triangleMeshes)
        {
            vector<uint8_t> cookedData;
            if (physxManager->Cook_TriangleMesh(mesh, colData->preLocalTransform, cookedData))
            {
                totalCookedBytes += cookedData.size();
                cookedMeshArray.push_back(json::binary(cookedData));
            }
        }

        json primitiveProxyArray = json::array();
        for (const ModelCollisionProxy& proxy : colData->primitiveProxies)
            primitiveProxyArray.push_back(Serialize_CollisionProxy(proxy));

        if (cookedMeshArray.empty() && primitiveProxyArray.empty())
            return false;

        const string modelPathA = Make_SplitInstanceModelPath(modelPath);
        if (primitiveProxyArray.empty() &&
            totalCookedBytes >= kCookedCollisionWarnBytes)
        {
            LOG_WARN(
                "[SceneContext::ModelCollision] Large cooked collision saved without primitive proxies. model={}, cookedBytes={}, cookedMeshCount={}",
                modelPathA,
                totalCookedBytes,
                cookedMeshArray.size());
        }

        json root = json::object();
        root["schema"] = "model_collision";
        root["schema_version"] = 1;
        root["model_path"] = modelPathA;
        root["cooked_format"] = "physx_triangle_mesh";
        root["CookedTriangleMeshes"] = std::move(cookedMeshArray);
        root["PrimitiveProxies"] = std::move(primitiveProxyArray);

        ofstream ofs(collisionPath, ios::binary);
        if (!ofs.is_open())
            return false;

        const vector<uint8_t> msgpack = json::to_msgpack(root);
        ofs.write(reinterpret_cast<const char*>(msgpack.data()), msgpack.size());
        return true;
    }

    void Ensure_StaticModelCollisionBinaries(const std::map<wstring, Shared<ModelCom>>& staticModels)
    {
        for (const auto& [modelPath, model] : staticModels)
        {
            try
            {
                (void)Write_ModelCollisionBinary(modelPath, model);
            }
            catch (const fs::filesystem_error& e)
            {
                LOG_WARN(
                    "[SceneContext::ModelCollision] failed to write collision. model='{}' error='{}'",
                    String::ToString(modelPath),
                    e.what());
            }
            catch (...)
            {
                LOG_WARN(
                    "[SceneContext::ModelCollision] failed to write collision. model='{}'",
                    String::ToString(modelPath));
            }
        }
    }

    bool HasSuffixInsensitive(const string& value, const string& suffix)
    {
        if (value.size() < suffix.size())
            return false;

        return String::ToLowerCopy(value.substr(value.size() - suffix.size())) ==
            String::ToLowerCopy(suffix);
    }

    void Remove_JsonFilesInDirectory(const fs::path& dir)
    {
        error_code ec;
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec))
            return;

        for (const fs::directory_entry& entry : fs::directory_iterator(dir, ec))
        {
            if (ec)
                break;

            if (!entry.is_regular_file(ec) || entry.path().extension() != ".json")
                continue;

            fs::remove(entry.path(), ec);
            ec.clear();
        }
    }

    void Remove_MaterialJsonFilesInDirectory(const fs::path& dir)
    {
        error_code ec;
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec))
            return;

        for (const fs::directory_entry& entry : fs::directory_iterator(dir, ec))
        {
            if (ec)
                break;

            if (!entry.is_regular_file(ec))
                continue;

            const string filename = entry.path().filename().string();
            if (!HasSuffixInsensitive(filename, ".material.json"))
                continue;

            fs::remove(entry.path(), ec);
            ec.clear();
        }
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

    void Serialize_PostProcessVolume(json& e, const EditorObject& obj)
    {
        e["Shape"] = obj._zoneShape == EZoneShape::OBB ? "OBB" : "Box";
        e["Extent"] = { obj._zoneExtent.x, obj._zoneExtent.y, obj._zoneExtent.z };
        e["Radius"] = obj._zoneRadius;
        e["BloomIntensity"] = obj._ppBloomIntensity;
        e["ExposureMin"] = obj._ppExposureMin;
        e["ExposureMax"] = obj._ppExposureMax;
        e["FogDensity"] = obj._ppFogDensity;
        e["FogColor"] = { obj._ppFogColor.x, obj._ppFogColor.y, obj._ppFogColor.z };
        e["BaseHeight"] = obj._ppFogBaseHeight;
        e["TopFadeStart"] = obj._ppFogTopFadeStart;
        e["TopHeight"] = obj._ppFogTopHeight;
        e["EdgeFade"] = obj._ppFogEdgeFade;
        e["NoiseScale"] = obj._ppFogNoiseScale;
        e["NoiseSpeed"] = { obj._ppFogNoiseSpeed.x, obj._ppFogNoiseSpeed.y };
        e["NoiseStrength"] = obj._ppFogNoiseStrength;
        e["NoiseMin"] = obj._ppFogNoiseMin;
        e["GroundFogMaxDistance"] = obj._ppGroundFogMaxDistance;
        e["GroundFogMinAccumulationDistance"] = obj._ppGroundFogMinAccumulationDistance;
        e["GroundFogDistanceInfluence"] = obj._ppGroundFogDistanceInfluence;
        e["GroundFogNearFadeDistance"] = obj._ppGroundFogNearFadeDistance;
        e["GroundFogNearMinFactor"] = obj._ppGroundFogNearMinFactor;
        e["GlobalFogIntensity"] = obj._ppGlobalFogIntensity;
    }

    void Deserialize_PostProcessVolume(const json& e, EditorObject& obj)
    {
        string shape = e.value("Shape", "Box");
        obj._zoneShape = magic_enum::enum_cast<EZoneShape>(shape).value_or(EZoneShape::Box);
        if (e.contains("Extent"))
        {
            auto& ext = e["Extent"];
            obj._zoneExtent = { ext[0], ext[1], ext[2] };
        }
        obj._zoneRadius = e.value("Radius", 5.f);
        obj._ppBloomIntensity = e.value("BloomIntensity", 1.f);
        obj._ppExposureMin = e.value("ExposureMin", 0.f);
        obj._ppExposureMax = e.value("ExposureMax", 2.f);
        obj._ppFogDensity = e.value("FogDensity", 0.f);
        if (e.contains("FogColor"))
        {
            auto& c = e["FogColor"];
            obj._ppFogColor = { c[0], c[1], c[2] };
        }
        obj._ppFogBaseHeight = e.value("BaseHeight", 0.f);
        obj._ppFogTopFadeStart = e.value("TopFadeStart", 0.4f);
        obj._ppFogTopHeight = e.value("TopHeight", 0.9f);
        obj._ppFogEdgeFade = e.value("EdgeFade", 1.f);
        obj._ppFogNoiseScale = e.value("NoiseScale", 0.0025f);
        if (e.contains("NoiseSpeed"))
        {
            auto& speed = e["NoiseSpeed"];
            obj._ppFogNoiseSpeed = { speed[0], speed[1] };
        }
        obj._ppFogNoiseStrength = e.value("NoiseStrength", 0.9f);
        obj._ppFogNoiseMin = e.value("NoiseMin", 0.38f);
        obj._ppGroundFogMaxDistance = e.value("GroundFogMaxDistance", 100.f);
        obj._ppGroundFogMinAccumulationDistance = e.value("GroundFogMinAccumulationDistance", 10.f);
        obj._ppGroundFogDistanceInfluence = e.value("GroundFogDistanceInfluence", 0.15f);
        obj._ppGroundFogNearFadeDistance = e.value("GroundFogNearFadeDistance", 12.f);
        obj._ppGroundFogNearMinFactor = e.value("GroundFogNearMinFactor", 0.65f);
        obj._ppGlobalFogIntensity = e.value("GlobalFogIntensity", 0.5f);
    }

    void Serialize_ShaftVolume(json& e, const EditorObject& obj)
    {
        e["Shape"] = obj._zoneShape == EZoneShape::Sphere ? "Sphere" : "Box";
        e["Extent"] = { obj._zoneExtent.x, obj._zoneExtent.y, obj._zoneExtent.z };
        e["Radius"] = obj._zoneRadius;
        e["Density"] = obj._shaftDensity;
        e["LightColor"] = { obj._shaftLightColor.x, obj._shaftLightColor.y, obj._shaftLightColor.z, obj._shaftLightColor.w };
        e["EdgeFade"] = obj._shaftEdgeFade;
        e["Intensity"] = obj._shaftIntensity;
    }

    void Deserialize_ShaftVolume(const json& e, EditorObject& obj)
    {
        string shape = e.value("Shape", "Box");
        obj._zoneShape = magic_enum::enum_cast<EZoneShape>(shape).value_or(EZoneShape::Box);
        if (e.contains("Extent"))
        {
            auto& ext = e["Extent"];
            obj._zoneExtent = { ext[0], ext[1], ext[2] };
        }
        obj._zoneRadius = e.value("Radius", 5.f);
        obj._shaftDensity = e.value("Density", 1.f);
        if (e.contains("LightColor"))
        {
            auto& c = e["LightColor"];
            obj._shaftLightColor = Vec4(c[0], c[1], c[2], c.size() > 3 ? static_cast<float>(c[3]) : 1.f);
        }
        obj._shaftEdgeFade = e.value("EdgeFade", 0.5f);
        obj._shaftIntensity = e.value("Intensity", 1.f);
    }

    string Strip_WaterTexturePath(const wstring& fullPath)
    {
        const wstring waterTexPrefix = _waterPath;
        if (fullPath.size() >= waterTexPrefix.size() && fullPath.substr(0, waterTexPrefix.size()) == waterTexPrefix)
        {
            return String::ToString(fullPath.substr(waterTexPrefix.size()));
        }
        return String::ToString(fs::path(fullPath).filename().wstring());
    }

    wstring Make_WaterTexturePath(const string& filename)
    {
        if (filename.empty())
        {
            return {};
        }
        return wstring(_waterPath) + String::ToWString(filename);
    }

    void Serialize_Water(json& e, const EditorObject& obj)
    {
        e["CountX"] = obj._waterDesc.countX;
        e["CountZ"] = obj._waterDesc.countZ;
        e["SizeX"] = obj._waterDesc.sizeX;
        e["SizeZ"] = obj._waterDesc.sizeZ;
        e["ChunkSize"] = obj._waterDesc.chunkSize;

        e["LargeWaveNormal"] = Strip_WaterTexturePath(obj._waterLargeWaveNormalPath);
        e["SmallWaveNormal"] = Strip_WaterTexturePath(obj._waterSmallWaveNormalPath);
        e["SeafoamTexture"] = Strip_WaterTexturePath(obj._waterSeafoamPath);
        e["Cubemap"] = Strip_WaterTexturePath(obj._waterCubemapPath);

        auto& p = obj._waterParams;
        e["LargeWaveScale"] = p.largeWaveScale;
        e["LargeWaveSpeed"] = p.largeWaveSpeed;
        e["LargeWaveAmplify"] = p.largeWaveAmplify;
        e["SmallWaveScale"] = p.smallWaveScale;
        e["SmallWaveSpeed"] = p.smallWaveSpeed;
        e["SmallWaveAmplify"] = p.smallWaveAmplify;
        e["SeafoamScale"] = p.seafoamScale;
        e["SeafoamSpeed"] = p.seafoamSpeed;
        e["SeafoamHeightPower"] = p.seafoamHeightPower;
        e["SeafoamHeightMultiply"] = p.seafoamHeightMultiply;
        e["FoamDistortion"] = p.foamDistortion;
        e["DepthFade"] = p.depthFade;
        e["CameraDistanceFade"] = p.cameraDistanceFade;
        e["ReflectionAmount"] = p.reflectionAmount;
        e["LuminanceBias"] = p.luminanceBias;
        e["WaterColor"] = { p.waterColor.x, p.waterColor.y, p.waterColor.z, p.waterColor.w };
        e["FresnelColor"] = { p.fresnelColor.x, p.fresnelColor.y, p.fresnelColor.z, p.fresnelColor.w };
    }

    void Deserialize_Water(const json& e, EditorObject& obj)
    {
        obj._waterDesc.countX = e.value("CountX", 64u);
        obj._waterDesc.countZ = e.value("CountZ", 64u);
        obj._waterDesc.sizeX = e.value("SizeX", 1.f);
        obj._waterDesc.sizeZ = e.value("SizeZ", 1.f);
        obj._waterDesc.chunkSize = e.value("ChunkSize", 16u);

        obj._waterLargeWaveNormalPath = Make_WaterTexturePath(e.value("LargeWaveNormal", string{}));
        obj._waterSmallWaveNormalPath = Make_WaterTexturePath(e.value("SmallWaveNormal", string{}));
        obj._waterSeafoamPath = Make_WaterTexturePath(e.value("SeafoamTexture", string{}));
        obj._waterCubemapPath = Make_WaterTexturePath(e.value("Cubemap", string{}));

        auto& p = obj._waterParams;
        p.largeWaveScale = e.value("LargeWaveScale", 64.f);
        p.largeWaveSpeed = e.value("LargeWaveSpeed", 0.1f);
        p.largeWaveAmplify = e.value("LargeWaveAmplify", 1.f);
        p.smallWaveScale = e.value("SmallWaveScale", 512.f);
        p.smallWaveSpeed = e.value("SmallWaveSpeed", 1.f);
        p.smallWaveAmplify = e.value("SmallWaveAmplify", 1.f);
        p.seafoamScale = e.value("SeafoamScale", 512.f);
        p.seafoamSpeed = e.value("SeafoamSpeed", 0.015f);
        p.seafoamHeightPower = e.value("SeafoamHeightPower", 2.f);
        p.seafoamHeightMultiply = e.value("SeafoamHeightMultiply", 3.f);
        p.foamDistortion = e.value("FoamDistortion", 0.f);
        p.depthFade = e.value("DepthFade", 0.f);
        p.cameraDistanceFade = e.value("CameraDistanceFade", 0.f);
        p.reflectionAmount = e.value("ReflectionAmount", 0.f);
        p.luminanceBias = e.value("LuminanceBias", 0.f);

        if (e.contains("WaterColor"))
        {
            auto& c = e["WaterColor"];
            p.waterColor = { c[0], c[1], c[2], c[3] };
        }
        if (e.contains("FresnelColor"))
        {
            auto& c = e["FresnelColor"];
            p.fresnelColor = { c[0], c[1], c[2], c[3] };
        }
    }

    string Make_SafeObjectJsonStem(string value)
    {
        if (value.empty())
            return "Object";

        for (char& ch : value)
        {
            switch (ch)
            {
            case '<':
            case '>':
            case ':':
            case '"':
            case '/':
            case '\\':
            case '|':
            case '?':
            case '*':
                ch = '_';
                break;
            default:
                break;
            }
        }

        if (value == "." || value == "..")
            return "Object";

        return value;
    }

    fs::path Make_UniqueObjectJsonPath(const fs::path& baseDir, const string& objName, int32 index)
    {
        const string safeName = Make_SafeObjectJsonStem(objName);
        fs::path outPath = baseDir / (safeName + ".json");

        error_code ec;
        if (!fs::exists(outPath, ec))
            return outPath;

        outPath = baseDir / (safeName + "_" + to_string(index) + ".json");
        int32 suffix = 1;
        while (fs::exists(outPath, ec))
        {
            outPath = baseDir / (safeName + "_" + to_string(index) + "_" + to_string(suffix++) + ".json");
            ec.clear();
        }

        return outPath;
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

        fs::path outPath = Make_UniqueObjectJsonPath(baseDir, objName, index);

        ofstream ofs(outPath);
        if (ofs.is_open())
        {
            ofs << e.dump(4);
        }
    }

    void Merge_LegacyPassIntoMaterialBindings(ModelOverrideEntry& entry, int32 legacyPassIndex)
    {
        if (legacyPassIndex < 0)
        {
            return;
        }

        entry.hasMaterialBindings = true;
        if (!entry.materialBindingsJson.is_array())
        {
            entry.materialBindingsJson = json::array();
        }

        if (entry.materialBindingsJson.empty())
        {
            json binding = json::object();
            binding["material_index"] = 0u;
            if (!entry.baseBundleKey.empty())
            {
                binding["base_bundle_key"] = entry.baseBundleKey;
            }
            binding["instance_override"]["force_pass_index"] = legacyPassIndex;
            entry.materialBindingsJson.push_back(std::move(binding));
            return;
        }

        for (json& binding : entry.materialBindingsJson)
        {
            if (!binding.is_object())
            {
                continue;
            }

            if (!binding.contains("instance_override") || !binding["instance_override"].is_object())
            {
                binding["instance_override"] = json::object();
            }

            json& instJson = binding["instance_override"];
            if (!instJson.contains("force_pass_index") && !instJson.contains("forcePassIndex"))
            {
                instJson["force_pass_index"] = legacyPassIndex;
            }
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
    json shaftVols = json::array();
    json lights = json::array();
    json instancedMeshes = json::array();
    json waters = json::array();
    std::map<string, string> modelOverrideModelPaths;

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
            if (!obj->_modelPath.empty())
            {
                modelOverrideModelPaths[fs::path(obj->_modelPath).filename().string()] = String::ToString(obj->_modelPath);
            }

            e["ModelPath"] = Make_SplitInstanceModelPath(obj->_modelPath);

            e["Scale"] = { obj->_scale.x, obj->_scale.y, obj->_scale.z };
            e["Visible"] = obj->_visible;
            e["CullEnabled"] = obj->_cullEnabled;
            e["CastShadow"] = obj->_castShadow;
            staticMeshes.push_back(e);

            break;
        }

        case EObjectType::SpawnPoint:
            spawnPoints.push_back(e);
            break;

        case EObjectType::EncounterZone:
            e["Shape"] = ZoneShapeName(obj->_zoneShape);
            e["Extent"] = { obj->_zoneExtent.x, obj->_zoneExtent.y, obj->_zoneExtent.z };
            e["Radius"] = obj->_zoneRadius;
            e["EnemyGroupId"] = obj->_zoneId;
            encounterZones.push_back(e);
            break;

        case EObjectType::TriggerVolume:
            e["Shape"] = ZoneShapeName(obj->_zoneShape);
            e["Extent"] = { obj->_zoneExtent.x, obj->_zoneExtent.y, obj->_zoneExtent.z };
            e["Radius"] = obj->_zoneRadius;
            e["EventId"] = obj->_zoneId;
            triggerVolumes.push_back(e);
            break;

        case EObjectType::CameraZone:
            e["Shape"] = ZoneShapeName(obj->_zoneShape);
            e["Extent"] = { obj->_zoneExtent.x, obj->_zoneExtent.y, obj->_zoneExtent.z };
            e["Radius"] = obj->_zoneRadius;
            e["FOV"] = obj->_cameraFov;
            e["FixedAngle"] = obj->_cameraFixedAngle;
            e["TransitionTime"] = obj->_cameraTransitionTime;
            cameraZones.push_back(e);
            break;

        case EObjectType::PostProcessVolume:
            Serialize_PostProcessVolume(e, *obj);
            postProcessVols.push_back(e);
            break;

        case EObjectType::ShaftVolume:
            Serialize_ShaftVolume(e, *obj);
            shaftVols.push_back(e);
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
                e["AttenuationPower"] = obj->_lightAttenuationPower;
                e["Diffuse"] = { obj->_lightDiffuse.x,  obj->_lightDiffuse.y,  obj->_lightDiffuse.z,  obj->_lightDiffuse.w };
                e["Ambient"] = { obj->_lightAmbient.x,  obj->_lightAmbient.y,  obj->_lightAmbient.z,  obj->_lightAmbient.w };
                e["Specular"] = { obj->_lightSpecular.x, obj->_lightSpecular.y, obj->_lightSpecular.z, obj->_lightSpecular.w };
                lights.push_back(e);
            }
            break;

        case EObjectType::InstancedMesh:
        {
            if (!obj->_instancedModelPath.empty())
            {
                modelOverrideModelPaths[fs::path(obj->_instancedModelPath).filename().string()] = String::ToString(obj->_instancedModelPath);
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
            e["ModelPath"] = Make_SplitInstanceModelPath(obj->_instancedModelPath);

            e["HasCollision"] = obj->_instancedHasCollision;
            e["Visible"] = obj->_visible;
            e["CullEnabled"] = obj->_cullEnabled;
            e["CastShadow"] = obj->_castShadow;
            e["CullDist"] = obj->_instancedCullDist;
            e["Instances"] = instances;
            instancedMeshes.push_back(e);

            break;
        }

        case EObjectType::Water:
        {
            Serialize_Water(e, *obj);
            waters.push_back(e);
            break;
        }

        default:
            break;
        }
    }

    json modelOverrides = json::object();
    for (auto& [filename, entry] : _modelOverrides)
    {
        if (!entry.HasAnyMaterialOverride())
        {
            continue;
        }

        json slots = json::object();
        for (auto& [slotIdx, texPath] : entry.textureSlots)
        {
            slots[to_string(slotIdx)] = texPath;
        }

        json overrideJson = {
        {"ShaderType", static_cast<int>(entry.shaderType)},
        {"TextureSlots", slots},
        };

        if (entry.hasMaterialCB)
        {
            Serialize_MaterialCB(overrideJson, entry.materialCB);
        }

        if (entry.hasMaterialBindings && entry.materialBindingsJson.is_array())
        {
            const auto modelPathIt = modelOverrideModelPaths.find(filename);
            const string modelPath = modelPathIt != modelOverrideModelPaths.end()
                ? modelPathIt->second
                : filename;
            const json materialJson = entry.BuildMaterialJson(modelPath, String::ToString(_mapName));

            overrideJson["HasMaterialBindings"] = true;
            overrideJson["MapObjectKind"] = MapMaterialObjectKindName(entry.materialObjectKind);
            overrideJson["BaseBundleKey"] = materialJson.value("base_bundle_key", entry.baseBundleKey);
            overrideJson["MaterialBindings"] = materialJson.value("material_bindings", json::array());
            if (materialJson.contains("material_meta") && materialJson["material_meta"].is_object())
            {
                overrideJson["MaterialMeta"] = materialJson["material_meta"];
            }
        }

        modelOverrides[filename] = overrideJson;
    }

    map["ModelOverrides"] = modelOverrides;
    map["StaticMeshes"] = staticMeshes;
    map["SpawnPoints"] = spawnPoints;
    map["EncounterZones"] = encounterZones;
    map["TriggerVolumes"] = triggerVolumes;
    map["CameraZones"] = cameraZones;
    map["PostProcessVolumes"] = postProcessVols;
    map["ShaftVolumes"] = shaftVols;
    map["Lights"] = lights;
    map["InstancedMeshes"] = instancedMeshes;
    map["Waters"] = waters;

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
        _shaftVolumes.clear();
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
            const int32 legacyPassIndex = data.value("PassIndex", -1);
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
            if (data.value("HasMaterialBindings", false) && data.contains("MaterialBindings"))
            {
                entry.hasMaterialBindings = true;
                entry.materialObjectKind = MapMaterialObjectKindFromName(data.value("MapObjectKind", string{}));
                entry.baseBundleKey = data.value("BaseBundleKey", string{});
                entry.materialBindingsJson = data["MaterialBindings"];
                entry.materialMetaJson = data.value("MaterialMeta", json::object());
            }
            Merge_LegacyPassIntoMaterialBindings(entry, legacyPassIndex);

            _modelOverrides[filename] = entry;
        }
        _dirtyPassOverrides = true;
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
            obj->_modelPath = String::ToWString(_mapSavePath)
                + _modelFolder + L"/"
                + String::ToWString(modelPathStr);

            obj->_visible = e.value("Visible", true);
            obj->_cullEnabled = e.value("CullEnabled", true);
            obj->_castShadow = e.value("CastShadow", true);
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
            Deserialize_PostProcessVolume(e, *obj);
            LoadTransform(e, *obj);
            LoadZoneShape(e, *obj);
            Add(obj);
        }
    }

    if (map.contains("ShaftVolumes"))
    {
        for (auto& e : map["ShaftVolumes"])
        {
            auto obj = make_shared<EditorObject>();
            obj->_type = EObjectType::ShaftVolume;
            obj->_name = String::ToWString(e.value("Id", "ShaftVolume"));

            if (e.contains("Position")) { auto& pos = e["Position"]; obj->_position = { pos[0], pos[1], pos[2] }; }
            if (e.contains("Rotation")) { auto& rot = e["Rotation"]; obj->_rotation = { rot[0], rot[1], rot[2], rot[3] }; }

            Deserialize_ShaftVolume(e, *obj);
            obj->UpdateWorldMatrix();
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
            obj->_lightAttenuationPower = e.value("AttenuationPower", 1.f);
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
            obj->_instancedHasCollision = e.value("HasCollision", false);
            obj->_visible = e.value("Visible", true);
            obj->_cullEnabled = e.value("CullEnabled", true);
            obj->_castShadow = e.value("CastShadow", true);
            obj->_instancedCullDist = e.value("CullDist", 125.f);

            string modelPathStr = e.value("ModelPath", string{});
            if (!modelPathStr.empty())
            {
                obj->_instancedModelPath = String::ToWString(_mapSavePath)
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

    if (map.contains("Waters"))
    {
        for (auto& e : map["Waters"])
        {
            auto obj = make_shared<EditorObject>();
            obj->_type = EObjectType::Water;
            obj->_name = String::ToWString(e.value("Id", string{ "Water" }));
            Deserialize_Water(e, *obj);
            LoadTransform(e, *obj);
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

Shared<EditorObject> SceneContext::SpawnInstancedObject(const wstring& modelPath, const Vec3& pos)
{
    for (auto& obj : _instancedMeshes)
    {
        if (obj->_instancedModelPath == modelPath)
        {
            InstanceTransform t{};
            t._position = pos;
            t._scale = Vec3(1.f, 1.f, 1.f);
            t._rotation = Quat::Identity;
            obj->_instancedTransforms.push_back(t);
            obj->_instancedParamsDirty = true;
            obj->_selectedInstanceIdx = static_cast<int32>(obj->_instancedTransforms.size()) - 1;
            _dirtyRenderHandles = true;
            _selected = obj;
            return obj;
        }
    }

    auto obj = make_shared<EditorObject>();
    obj->_type = EObjectType::InstancedMesh;
    obj->_instancedModelPath = modelPath;
    obj->_position = Vec3::Zero;
    obj->_scale = Vec3(1.f, 1.f, 1.f);
    obj->_rotation = Quat::Identity;

    const wstring filename = fs::path(modelPath).stem().wstring();
    obj->_name = filename.empty() ? L"IM_NEW" : filename;

    InstanceTransform t{};
    t._position = pos;
    t._scale = Vec3(1.f, 1.f, 1.f);
    t._rotation = Quat::Identity;
    obj->_instancedTransforms.push_back(t);
    obj->_selectedInstanceIdx = 0;

    obj->UpdateWorldMatrix();
    obj->_instancedParamsDirty = true;

    _dirtyRenderHandles = true;
    Add(obj);
    return obj;
}

Shared<EditorObject> SceneContext::FindInstancedObjectByModelPath(const wstring& modelPath)
{
    for (auto& obj : _instancedMeshes)
    {
        if (obj && obj->_instancedModelPath == modelPath)
        {
            return obj;
        }
    }

    return nullptr;
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
    if (j.contains("Rotation"))
    {
        auto& rot = j["Rotation"];
        obj->_rotation = { rot[0], rot[1], rot[2], rot[3] };
    }
    if (j.contains("Scale"))
    {
        auto& scale = j["Scale"];
        obj->_scale = { scale[0], scale[1], scale[2] };
    }

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
        obj->_lightAttenuationPower = j.value("AttenuationPower", 1.f);
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
    case EObjectType::ShaftVolume:
    {
        string shapeStr = j.value("Shape", "Box");
        obj->_zoneShape = magic_enum::enum_cast<EZoneShape>(shapeStr).value_or(EZoneShape::Box);

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
            obj->_ppFogBaseHeight = j.value("BaseHeight", 0.f);
            obj->_ppFogTopFadeStart = j.value("TopFadeStart", 0.4f);
            obj->_ppFogTopHeight = j.value("TopHeight", 0.9f);
            obj->_ppFogEdgeFade = j.value("EdgeFade", 1.f);
            obj->_ppFogNoiseScale = j.value("NoiseScale", 0.0025f);
            if (j.contains("NoiseSpeed"))
            {
                auto& speed = j["NoiseSpeed"];
                obj->_ppFogNoiseSpeed = { speed[0], speed[1] };
            }
            obj->_ppFogNoiseStrength = j.value("NoiseStrength", 0.9f);
            obj->_ppFogNoiseMin = j.value("NoiseMin", 0.38f);
            obj->_ppGroundFogMaxDistance = j.value("GroundFogMaxDistance", 100.f);
            obj->_ppGroundFogMinAccumulationDistance = j.value("GroundFogMinAccumulationDistance", 10.f);
            obj->_ppGroundFogDistanceInfluence = j.value("GroundFogDistanceInfluence", 0.15f);
            obj->_ppGroundFogNearFadeDistance = j.value("GroundFogNearFadeDistance", 12.f);
            obj->_ppGroundFogNearMinFactor = j.value("GroundFogNearMinFactor", 0.65f);
            obj->_ppGlobalFogIntensity = j.value("GlobalFogIntensity", 0.5f);
        }
        else if (type == EObjectType::ShaftVolume)
        {
            obj->_shaftDensity = j.value("Density", 1.f);
            if (j.contains("LightColor"))
            {
                auto& c = j["LightColor"];
                obj->_shaftLightColor = Vec4(c[0], c[1], c[2], c.size() > 3 ? static_cast<float>(c[3]) : 1.f);
            }
            obj->_shaftEdgeFade = j.value("EdgeFade", 0.5f);
            obj->_shaftIntensity = j.value("Intensity", 1.f);
        }
        break;
    }

    case MapEditor::EObjectType::InstancedMesh:
    {
        string modelPathStr = j.value("ModelPath", string{});
        if (!modelPathStr.empty())
        {
            obj->_instancedModelPath = String::ToWString(_mapSavePath)
                + _modelFolder + L"/"
                + String::ToWString(modelPathStr);
        }
        break;
    }

    case EObjectType::Water:
    {
        obj->_waterDesc.countX = j.value("CountX", 64u);
        obj->_waterDesc.countZ = j.value("CountZ", 64u);
        obj->_waterDesc.sizeX = j.value("SizeX", 1.f);
        obj->_waterDesc.sizeZ = j.value("SizeZ", 1.f);
        obj->_waterDesc.chunkSize = j.value("ChunkSize", 16u);
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
    else if (obj->_type == EObjectType::ShaftVolume)
    {
        _shaftVolumes.push_back(obj);
    }
    else if (obj->_type == EObjectType::InstancedMesh)
    {
        _instancedMeshes.push_back(obj);
    }
    else if (obj->_type == EObjectType::Water)
    {
        _waters.push_back(obj);
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
    else if (obj->_type == EObjectType::ShaftVolume)
    {
        erase(_shaftVolumes, obj);
    }
    else if (obj->_type == EObjectType::InstancedMesh)
    {
        erase(_instancedMeshes, obj);
    }
    else if (obj->_type == EObjectType::Water)
    {
        erase(_waters, obj);
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
    for (int32 i = 1;; ++i)
    {
        const wstring candidate = src->_name + L"_" + to_wstring(i);
        const bool exists = any_of(_objects.begin(), _objects.end(),
            [&candidate](const Shared<EditorObject>& obj) { return obj && obj->_name == candidate; });
        if (!exists)
        {
            copy->_name = candidate;
            break;
        }
    }
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
    _waters.clear();
    _postProcessVolumes.clear();
    _shaftVolumes.clear();
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

bool SceneContext::SampleTerrainY(float worldX, float worldZ, float& outY) const
{
    for (auto& actor : _landscapes)
    {
        auto vb = actor->Get_VIBuffer();
        if (!vb)
        {
            continue;
        }

        Vec3 pos = actor->Get_Transform()->Get_Position();
        Vec3 scale = actor->Get_Transform()->Get_Scale();

        float localX = (worldX - pos.x) / scale.x;
        float localZ = (worldZ - pos.z) / scale.z;

        float totalSizeX = vb->Get_SizeX() * static_cast<float>(vb->Get_CountX());
        float totalSizeZ = vb->Get_SizeZ() * static_cast<float>(vb->Get_CountZ());

        if (localX < 0.f || localX > totalSizeX || localZ < 0.f || localZ > totalSizeZ)
        {
            continue;
        }

        outY = vb->Sample_Height(localX, localZ) * scale.y + pos.y;
        return true;
    }

    return false;
}

void SceneContext::ScatterAt(EditorObject* obj, const ScatterZone& zone)
{
    if (!obj || obj->_type != EObjectType::InstancedMesh)
    {
        return;
    }

    mt19937 rng(static_cast<uint32>(zone.seed));

    int placed = 0;
    int attempts = 0;
    const int maxAttempts = zone.count * 20;

    while (placed < zone.count && attempts < maxAttempts)
    {
        ++attempts;

        float angle = Math::RandNum(0.f, XM_2PI, rng);
        float r = zone.radius * sqrtf(Math::RandNum(0.f, 1.f, rng));
        float wx = zone.center.x + cosf(angle) * r;
        float wz = zone.center.z + sinf(angle) * r;

        if (zone.minSpacing > 0.f)
        {
            bool tooClose = false;
            for (auto& t : obj->_instancedTransforms)
            {
                float dx = t._position.x - wx;
                float dz = t._position.z - wz;
                if (sqrtf(dx * dx + dz * dz) < zone.minSpacing)
                {
                    tooClose = true;
                    break;
                }
            }

            if (tooClose)
            {
                continue;
            }
        }

        float wy = zone.center.y;
        if (!SampleTerrainY(wx, wz, wy))
        {
            continue;
        }

        float scale = Math::RandNum(zone.scaleMin, zone.scaleMax, rng);
        float yRot = Math::RandNum(0.f, zone.yRotMax * (XM_PI / 180.f), rng);

        InstanceTransform t{};
        t._position = Vec3(wx, wy, wz);
        t._scale = Vec3(scale, scale, scale);
        t._rotation = Vec4(0.f, sinf(yRot * 0.5f), 0.f, cosf(yRot * 0.5f));
        obj->_instancedTransforms.push_back(t);
        ++placed;
    }

    obj->_instancedParamsDirty = true;
    _dirtyRenderHandles = true;
}

void SceneContext::EraseAt(EditorObject* obj, float radius, const Vec3& center)
{
    if (!obj || obj->_type != EObjectType::InstancedMesh)
    {
        return;
    }

    const float radiusSq = radius * radius;

    auto& transforms = obj->_instancedTransforms;
    transforms.erase(
        remove_if(transforms.begin(), transforms.end(),
            [&](const InstanceTransform& t)
            {
                float dx = t._position.x - center.x;
                float dz = t._position.z - center.z;
                return (dx * dx + dz * dz) <= radiusSq;
            }),
        transforms.end());

    obj->_instancedParamsDirty = true;
    _dirtyRenderHandles = true;
}

void SceneContext::ScatterAtBox(EditorObject* obj, const ScatterBoxZone& box)
{
    if (!obj || obj->_type != EObjectType::InstancedMesh)
    {
        return;
    }

    if (_pendingBoxCount > 0 && _pendingBoxTarget == obj)
    {
        DeleteBoxScatter(obj);
    }

    _pendingBox = box;
    _pendingBoxTarget = obj;
    _pendingBoxLocalXZ.clear();

    mt19937 rng(static_cast<uint32>(box.seed));
    int placed = 0;
    int attempts = 0;
    const int maxAttempts = box.count * 20;

    while (placed < box.count && attempts < maxAttempts)
    {
        ++attempts;

        float localX = Math::RandNum(-box.halfExtentX, box.halfExtentX, rng);
        float localZ = Math::RandNum(-box.halfExtentZ, box.halfExtentZ, rng);

        if (box.minSpacing > 0.f)
        {
            bool tooClose = false;
            for (auto& off : _pendingBoxLocalXZ)
            {
                float dx = off.x - localX;
                float dz = off.y - localZ;
                if (sqrtf(dx * dx + dz * dz) < box.minSpacing)
                {
                    tooClose = true;
                    break;
                }
            }
            if (tooClose)
            {
                continue;
            }
        }

        float scale = Math::RandNum(box.scaleMin, box.scaleMax, rng);
        float yRot = Math::RandNum(0.f, box.yRotMax * (XM_PI / 180.f), rng);

        InstanceTransform t{};
        t._position = Vec3(box.center.x + localX, box.center.y, box.center.z + localZ);
        t._scale = Vec3(scale, scale, scale);
        t._rotation = Vec4(0.f, sinf(yRot * 0.5f), 0.f, cosf(yRot * 0.5f));

        obj->_instancedTransforms.push_back(t);
        _pendingBoxLocalXZ.push_back(Vec2(localX, localZ));
        ++placed;
    }

    _pendingBoxCount = placed;
    obj->_instancedParamsDirty = true;
    _dirtyRenderHandles = true;
}

void SceneContext::SyncBoxScatter(EditorObject* obj)
{
    if (!obj || _pendingBoxCount == 0 || _pendingBoxTarget != obj)
    {
        return;
    }

    int base = static_cast<int>(obj->_instancedTransforms.size()) - _pendingBoxCount;
    if (base < 0)
    {
        return;
    }

    for (int i = 0; i < _pendingBoxCount; ++i)
    {
        auto& t = obj->_instancedTransforms[base + i];
        auto& off = _pendingBoxLocalXZ[i];
        t._position.x = _pendingBox.center.x + off.x;
        t._position.y = _pendingBox.center.y;
        t._position.z = _pendingBox.center.z + off.y;
    }

    obj->_instancedParamsDirty = true;
    _dirtyRenderHandles = true;
}

void SceneContext::ApplyBoxScatter()
{
    _pendingBoxCount = 0;
    _pendingBoxTarget = nullptr;
    _pendingBoxLocalXZ.clear();
}

void SceneContext::DeleteBoxScatter(EditorObject* obj)
{
    if (!obj || _pendingBoxCount == 0 || _pendingBoxTarget != obj)
    {
        return;
    }

    int base = static_cast<int>(obj->_instancedTransforms.size()) - _pendingBoxCount;
    if (base >= 0)
    {
        obj->_instancedTransforms.erase(
            obj->_instancedTransforms.begin() + base,
            obj->_instancedTransforms.end());
    }

    _pendingBoxCount = 0;
    _pendingBoxTarget = nullptr;
    _pendingBoxLocalXZ.clear();

    obj->_instancedParamsDirty = true;
    _dirtyRenderHandles = true;
}

void SceneContext::UpdateSingleInstance(EditorObject* obj, int32 instanceIdx)
{
    if (!obj || instanceIdx < 0 || instanceIdx >= static_cast<int32>(obj->_instancedTransforms.size()))
    {
        return;
    }

    for (int32 ci = 0; ci < static_cast<int32>(obj->_instancedChunkGlobalIndices.size()); ++ci)
    {
        auto& globalIndices = obj->_instancedChunkGlobalIndices[ci];
        auto it = find(globalIndices.begin(), globalIndices.end(), static_cast<uint32>(instanceIdx));
        if (it == globalIndices.end())
        {
            continue;
        }

        vector<InstanceTransform> chunkTransforms;
        chunkTransforms.reserve(globalIndices.size());
        for (uint32 gIdx : globalIndices)
        {
            chunkTransforms.push_back(obj->_instancedTransforms[gIdx]);
        }

        auto& actor = obj->_instancedChunkHandles[ci];
        if (actor)
        {
            actor->UpdateInstances(obj->_instancedModelPath, chunkTransforms, nullptr);
        }

        return;
    }
}

void SceneContext::Export_MapInstance(const string& mapName)
{
    if (mapName.empty() || mapName == "MapInstance")
    {
        LOG_WARN("[SceneContext::Export_MapInstance] invalid map name. mapName='{}'", mapName);
        return;
    }

    const fs::path mapRoot = Resolve_MapResourceRoot();
    fs::path root = mapRoot / "MapInstance" / mapName;

    fs::path staticDir = root / "StaticMesh";
    fs::path instanceDir = root / "InstanceMesh";
    fs::path waterDir = root / "Water";
    fs::path postProcessVolumeDir = root / "PostProcessVolume";
    fs::path shaftVolumeDir = root / "ShaftVolume";
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

        if (!fs::exists(postProcessVolumeDir))
            fs::create_directories(postProcessVolumeDir);

        if (!fs::exists(shaftVolumeDir))
            fs::create_directories(shaftVolumeDir);

        if (!fs::exists(etcDir))
            fs::create_directories(etcDir);
    }

    Remove_JsonFilesInDirectory(staticDir);
    Remove_JsonFilesInDirectory(instanceDir);
    Remove_JsonFilesInDirectory(waterDir);
    Remove_JsonFilesInDirectory(postProcessVolumeDir);
    Remove_JsonFilesInDirectory(shaftVolumeDir);
    Remove_JsonFilesInDirectory(etcDir);

    int32 st_idx{}, inst_idx{}, water_idx{}, ppv_idx{}, shaft_idx{}, etc_idx{};
    std::map<wstring, Shared<ModelCom>> staticCollisionModels;

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
            e["ModelPath"] = Make_SplitInstanceModelPath(obj->_modelPath);
            e["Scale"] = { obj->_scale.x, obj->_scale.y, obj->_scale.z };
            e["Visible"] = obj->_visible;
            e["CullEnabled"] = obj->_cullEnabled;
            e["CastShadow"] = obj->_castShadow;
            Export_ObjectJson(staticDir, st_idx++, e);
            if (obj->_renderHandle && obj->_renderHandle->GetModel())
                staticCollisionModels[obj->_modelPath] = obj->_renderHandle->GetModel();
            break;
        }
        case EObjectType::InstancedMesh:
        {
            e["ModelPath"] = Make_SplitInstanceModelPath(obj->_instancedModelPath);
            e["HasCollision"] = obj->_instancedHasCollision;
            e["Visible"] = obj->_visible;
            e["CullEnabled"] = obj->_cullEnabled;
            e["CastShadow"] = obj->_castShadow;
            e["CullDist"] = obj->_instancedCullDist;
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
        case EObjectType::PostProcessVolume:
            e["Type"] = "PostProcessVolume";
            Serialize_PostProcessVolume(e, *obj);
            Export_ObjectJson(postProcessVolumeDir, ppv_idx++, e);
            break;

        case EObjectType::Light:
        {
            if (obj->_isDefaultLight)
            {
                break;
            }

            e["Type"] = "Light";
            e["LightType"] = (obj->_lightType == LightType::Directional) ? "Directional" : "Point";
            e["Range"] = obj->_lightRange;
            e["AttenuationPower"] = obj->_lightAttenuationPower;
            e["Diffuse"] = { obj->_lightDiffuse.x, obj->_lightDiffuse.y, obj->_lightDiffuse.z, obj->_lightDiffuse.w };
            e["Ambient"] = { obj->_lightAmbient.x, obj->_lightAmbient.y, obj->_lightAmbient.z, obj->_lightAmbient.w };
            e["Specular"] = { obj->_lightSpecular.x, obj->_lightSpecular.y, obj->_lightSpecular.z, obj->_lightSpecular.w };
            Export_ObjectJson(etcDir, etc_idx++, e);
            break;
        }

        case EObjectType::ShaftVolume:
            e["Type"] = "ShaftVolume";
            Serialize_ShaftVolume(e, *obj);
            Export_ObjectJson(shaftVolumeDir, shaft_idx++, e);
            break;

        case EObjectType::Water:
            e["Type"] = "Water";
            Serialize_Water(e, *obj);
            Export_ObjectJson(waterDir, water_idx++, e);
            break;

        default:
            e["Type"] = ObjectTypeName(obj->_type);
            Export_ObjectJson(etcDir, etc_idx++, e);
            break;
        }
    }

    Ensure_StaticModelCollisionBinaries(staticCollisionModels);

    map<wstring, Shared<ModelCom>> instancedCollisionModels;
    for (auto& obj : _instancedMeshes)
    {
        if (obj->_instancedHasCollision && !obj->_instancedModelPath.empty())
        {
            instancedCollisionModels[obj->_instancedModelPath] =
                obj->_instancedChunkHandles.empty() ? nullptr : obj->_instancedChunkHandles[0]->GetModel();
        }
    }
    Ensure_StaticModelCollisionBinaries(instancedCollisionModels);

    Remove_MaterialJsonFilesInDirectory(fs::path(_materialsPath) / mapName);

    map<wstring, Shared<Engine::ModelCom>> uniqueModels;
    for (auto& obj : _objects)
    {
        if (obj->_type == EObjectType::StaticMesh && !obj->_modelPath.empty())
        {
            uniqueModels[obj->_modelPath] = obj->_renderHandle ? obj->_renderHandle->GetModel() : nullptr;
        }
        else if (obj->_type == EObjectType::InstancedMesh && !obj->_instancedModelPath.empty())
        {
            uniqueModels[obj->_instancedModelPath] = obj->_instancedChunkHandles.empty() ? nullptr : obj->_instancedChunkHandles[0]->GetModel();
        }
    }

    for (const auto& [modelPathW, model] : uniqueModels)
    {
        string modelPath = String::ToString(modelPathW);
        string filename = fs::path(modelPath).filename().string();

        for (const string& key : { filename })
        {
            auto overrideIt = _modelOverrides.find(key);
            if (overrideIt != _modelOverrides.end() && overrideIt->second.HasAnyMaterialOverride())
            {
                overrideIt->second.ExportToNativeMaterial(modelPath, mapName);
            }
        }
    }

    const vector<pair<string, function<bool(const json&)>>> subfolders = {
    { "StaticMesh", nullptr },
    { "InstanceMesh", nullptr },
    { "PostProcessVolume", nullptr },
    { "ShaftVolume", nullptr },
    { "Water", nullptr },
    { "ETC", [](const json& j) { return j.value("LightType", "Directional") == "Point"; } },
    };

    for (const auto& [folderName, filter] : subfolders)
    {
        fs::path dir = root / folderName;
        if (!fs::exists(dir))
        {
            continue;
        }

        json arr = json::array();

        for (const fs::directory_entry& entry : fs::directory_iterator(dir))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
            {
                continue;
            }

            ifstream ifs(entry.path());
            if (!ifs.is_open())
            {
                continue;
            }

            try
            {
                json item = json::parse(ifs);
                if (!item.is_object())
                {
                    continue;
                }

                if (filter && !filter(item))
                {
                    continue;
                }

                arr.push_back(std::move(item));
            }
            catch (...) {}
        }

        fs::path binPath = root / (folderName + ".bin");
        ofstream ofs(binPath, ios::binary);
        if (!ofs.is_open())
        {
            continue;
        }

        try
        {
            vector<uint8_t> msgpack = json::to_msgpack(arr);
            ofs.write(reinterpret_cast<const char*>(msgpack.data()), msgpack.size());
        }
        catch (...) {}
    }
}

void SceneContext::Import_MapInstance(const string& mapName)
{
    ClearScene();
    _mapName = String::ToWString(mapName);
    const fs::path mapRoot = Resolve_MapResourceRoot();
    _modelFolder = String::ToWString(mapName);
    _currentFilePath = (mapRoot / "SaveJson" / (mapName + ".json")).generic_string();

    fs::path root = mapRoot / "MapInstance" / mapName;
    fs::path staticDir = root / "StaticMesh";
    fs::path instanceDir = root / "InstanceMesh";
    fs::path waterDir = root / "Water";
    fs::path postProcessVolumeDir = root / "PostProcessVolume";
    fs::path shaftVolumeDir = root / "ShaftVolume";

    auto MaterialFileToModelKey = [](const fs::path& materialPath) -> string
    {
        string filename = materialPath.filename().string();
        const string suffix = ".material.json";
        if (filename.size() > suffix.size() &&
            String::ToLowerCopy(filename.substr(filename.size() - suffix.size())) == suffix)
        {
            filename.resize(filename.size() - suffix.size());
            return filename + ".model";
        }

        return materialPath.stem().string() + ".model";
    };

    fs::path matDir = fs::path(_materialsPath) / mapName;
    if (fs::exists(matDir))
    {
        for (const auto& entry : fs::directory_iterator(matDir))
        {
            if (entry.path().extension() == ".json")
            {
                ifstream ifs(entry.path());
                if (ifs.is_open())
                {
                    json rootJson = json::parse(ifs);
                    if (rootJson.contains("material_bindings") && rootJson["material_bindings"].is_array())
                    {
                        string modelKey = rootJson.value("editor_modelPath", "");
                        if (!modelKey.empty())
                        {
                            modelKey = fs::path(modelKey).filename().string();
                        }
                        else
                        {
                            modelKey = MaterialFileToModelKey(entry.path());
                        }

                        if (!modelKey.empty())
                        {
                            ModelOverrideEntry overrideEntry;
                            overrideEntry.LoadMaterialBindingsJson(rootJson);
                            ShaderType loadedType = static_cast<ShaderType>(rootJson.value("editor_shaderType", static_cast<int>(ShaderType::VtxMesh)));
                            _modelOverrides[modelKey] = overrideEntry;
                        }
                        continue;
                    }

                    if (rootJson.contains("instances") && rootJson["instances"].is_array() && rootJson["instances"].size() > 0)
                    {
                        const json& instJson = rootJson["instances"][0];
                        
                        string modelKey = instJson.value("editor_modelPath", "");
                        if (!modelKey.empty())
                        {
                            fs::path pk(modelKey);
                            modelKey = pk.filename().string();
                        }
                        else 
                        {
                            modelKey = MaterialFileToModelKey(entry.path()); 
                        }

                        ModelOverrideEntry overrideEntry;
                        overrideEntry.LoadLegacyMaterialJson(rootJson);

                        _modelOverrides[modelKey] = overrideEntry;
                    }
                }
            }
        }
    }

    if (fs::exists(staticDir))
    {
        for (const auto& entry : fs::directory_iterator(staticDir))
        {
            if (entry.path().extension() == ".json")
            {
                ifstream ifs(entry.path());
                if (ifs.is_open())
                {
                    json e = json::parse(ifs);
                    auto obj = make_shared<EditorObject>();
                    obj->_type = EObjectType::StaticMesh;
                    obj->_name = String::ToWString(e.value("Id", "Unknown"));
                    
                    if (e.contains("Position")) { auto& pos = e["Position"]; obj->_position = { pos[0], pos[1], pos[2] }; }
                    if (e.contains("Rotation")) { auto& rot = e["Rotation"]; obj->_rotation = { rot[0], rot[1], rot[2], rot[3] }; }
                    if (e.contains("Scale"))    { auto& scl = e["Scale"];    obj->_scale = { scl[0], scl[1], scl[2] }; }
                    
                    string modelPathStr = e.value("ModelPath", "");
                    if (!modelPathStr.empty())
                    {
                        obj->_modelPath = Resolve_SplitInstanceImportModelPath(mapRoot, mapName, modelPathStr);
                    }

                    obj->_visible = e.value("Visible", true);
                    obj->_cullEnabled = e.value("CullEnabled", true);
                    obj->_castShadow = e.value("CastShadow", true);
                    obj->UpdateWorldMatrix();
                    Add(obj);
                }
            }
        }
    }
    
    if (fs::exists(instanceDir))
    {
        for (const auto& entry : fs::directory_iterator(instanceDir))
        {
            if (entry.path().extension() == ".json")
            {
                ifstream ifs(entry.path());
                if (ifs.is_open())
                {
                    json e = json::parse(ifs);
                    auto obj = make_shared<EditorObject>();
                    obj->_type = EObjectType::InstancedMesh;
                    obj->_name = String::ToWString(e.value("Id", "Unknown"));
                    
                    if (e.contains("Position")) { auto& pos = e["Position"]; obj->_position = { pos[0], pos[1], pos[2] }; }
                    if (e.contains("Rotation")) { auto& rot = e["Rotation"]; obj->_rotation = { rot[0], rot[1], rot[2], rot[3] }; }
                    if (e.contains("Scale"))    { auto& scl = e["Scale"];    obj->_scale = { scl[0], scl[1], scl[2] }; }
                    
                    string modelPathStr = e.value("ModelPath", "");
                    if (!modelPathStr.empty())
                    {
                        fs::path resolved = Resolve_SplitInstanceImportModelPath(mapRoot, mapName, modelPathStr);
                        error_code ec;
                        fs::path canonical = fs::weakly_canonical(resolved, ec);
                        obj->_instancedModelPath = (!ec && !canonical.empty()) ? canonical.wstring() : resolved.wstring();
                    }
                    obj->_instancedHasCollision = e.value("HasCollision", false);
                    obj->_visible = e.value("Visible", true);
                    obj->_cullEnabled = e.value("CullEnabled", true);
                    obj->_castShadow = e.value("CastShadow", true);
                    obj->_instancedCullDist = e.value("CullDist", 125.f);

                    if (e.contains("Instances"))
                    {
                        for (const auto& instJson : e["Instances"])
                        {
                            InstanceTransform t;
                            if (instJson.contains("Position")) { auto& pos = instJson["Position"]; t._position = { pos[0], pos[1], pos[2] }; }
                            if (instJson.contains("Rotation")) { auto& rot = instJson["Rotation"]; t._rotation = { rot[0], rot[1], rot[2], rot[3] }; }
                            if (instJson.contains("Scale"))    { auto& scl = instJson["Scale"];    t._scale = { scl[0], scl[1], scl[2] }; }
                            obj->_instancedTransforms.push_back(t);
                        }
                    }
                    obj->UpdateWorldMatrix();
                    Add(obj);
                }
            }
        }
    }

    if (fs::exists(postProcessVolumeDir))
    {
        for (const auto& entry : fs::directory_iterator(postProcessVolumeDir))
        {
            if (entry.path().extension() == ".json")
            {
                ifstream ifs(entry.path());
                if (ifs.is_open())
                {
                    json e = json::parse(ifs);
                    auto obj = make_shared<EditorObject>();
                    obj->_type = EObjectType::PostProcessVolume;
                    obj->_name = String::ToWString(e.value("Id", "PostProcessVolume"));

                    if (e.contains("Position")) { auto& pos = e["Position"]; obj->_position = { pos[0], pos[1], pos[2] }; }
                    if (e.contains("Rotation")) { auto& rot = e["Rotation"]; obj->_rotation = { rot[0], rot[1], rot[2], rot[3] }; }

                    Deserialize_PostProcessVolume(e, *obj);
                    obj->UpdateWorldMatrix();
                    Add(obj);
                }
            }
        }
    }

    if (fs::exists(shaftVolumeDir))
    {
        for (const auto& entry : fs::directory_iterator(shaftVolumeDir))
        {
            if (entry.path().extension() == ".json")
            {
                ifstream ifs(entry.path());
                if (ifs.is_open())
                {
                    json e = json::parse(ifs);
                    auto obj = make_shared<EditorObject>();
                    obj->_type = EObjectType::ShaftVolume;
                    obj->_name = String::ToWString(e.value("Id", "ShaftVolume"));

                    if (e.contains("Position")) { auto& pos = e["Position"]; obj->_position = { pos[0], pos[1], pos[2] }; }
                    if (e.contains("Rotation")) { auto& rot = e["Rotation"]; obj->_rotation = { rot[0], rot[1], rot[2], rot[3] }; }

                    Deserialize_ShaftVolume(e, *obj);
                    obj->UpdateWorldMatrix();
                    Add(obj);
                }
            }
        }
    }

    if (fs::exists(waterDir))
    {
        for (const auto& entry : fs::directory_iterator(waterDir))
        {
            if (entry.path().extension() == ".json")
            {
                ifstream ifs(entry.path());
                if (ifs.is_open())
                {
                    json e = json::parse(ifs);
                    auto obj = make_shared<EditorObject>();
                    obj->_type = EObjectType::Water;
                    obj->_name = String::ToWString(e.value("Id", "Water"));

                    if (e.contains("Position")) { auto& pos = e["Position"]; obj->_position = { pos[0], pos[1], pos[2] }; }
                    if (e.contains("Rotation")) { auto& rot = e["Rotation"]; obj->_rotation = { rot[0], rot[1], rot[2], rot[3] }; }

                    Deserialize_Water(e, *obj);
                    obj->UpdateWorldMatrix();
                    Add(obj);
                }
            }
        }
    }

    fs::path etcImportDir = root / "ETC";
    if (fs::exists(etcImportDir))
    {
        for (const auto& entry : fs::directory_iterator(etcImportDir))
        {
            if (entry.path().extension() != ".json")
            {
                continue;
            }

            ifstream ifs(entry.path());
            if (!ifs.is_open())
            {
                continue;
            }

            json e = json::parse(ifs);
            string typeName = e.value("Type", "");
            if (typeName != "Light")
            {
                continue;
            }

            auto obj = make_shared<EditorObject>();
            obj->_type = EObjectType::Light;
            obj->_name = String::ToWString(e.value("Id", "Light"));
            string lt = e.value("LightType", "Point");
            obj->_lightType = (lt == "Directional") ? LightType::Directional : LightType::Point;
            obj->_lightRange = e.value("Range", 10.f);
            obj->_lightAttenuationPower = e.value("AttenuationPower", 1.f);
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

            if (e.contains("Position"))
            {
                auto& p = e["Position"];
                obj->_position = { p[0], p[1], p[2] };
            }

            if (e.contains("Rotation"))
            {
                auto& r = e["Rotation"];
                obj->_rotation = { r[0], r[1], r[2], r[3] };
            }
            obj->UpdateWorldMatrix();
            Add(obj);
        }
    }

    _dirtyRenderHandles = true;
    _octreeDirty = true;
}

