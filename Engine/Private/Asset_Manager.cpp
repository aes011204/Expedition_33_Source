#include "Asset_Manager.h"

#include <objbase.h>

#include "MaterialBuilder.h"
#include "Vertex_Types.h"

namespace
{
    struct ModelMetaBoneEntry
    {
        string name{};
        int32 parentIndex{ -1 };
        int32 depth{ 0 };
    };

    bool Read_ModelMetaString(ifstream& stream, string& outString)
    {
        uint32 length = 0;
        stream.read(reinterpret_cast<char*>(&length), sizeof(uint32));
        if (!stream.good())
            return false;

        outString.clear();
        if (length == 0)
            return true;

        outString.resize(length);
        stream.read(outString.data(), length);
        return stream.good();
    }

    bool Skip_ModelMetaBytes(ifstream& stream, uint64 numBytes)
    {
        stream.seekg(static_cast<streamoff>(numBytes), ios::cur);
        return stream.good();
    }

    int32 Compute_ModelMetaBoneDepth(uint32 boneIndex, const vector<ModelMetaBoneEntry>& bones, vector<int32>& cachedDepths)
    {
        if (boneIndex >= bones.size())
            return 0;

        int32& cachedDepth = cachedDepths[boneIndex];
        if (cachedDepth != -1)
            return cachedDepth;

        const int32 parentIndex = bones[boneIndex].parentIndex;
        if (parentIndex < 0 || parentIndex >= static_cast<int32>(bones.size()))
        {
            cachedDepth = 0;
            return cachedDepth;
        }

        cachedDepth = Compute_ModelMetaBoneDepth(static_cast<uint32>(parentIndex), bones, cachedDepths) + 1;
        return cachedDepth;
    }

    bool Try_ReadModelMetaBones(const wstring& modelPath, vector<ModelMetaBoneEntry>& outBones)
    {
        outBones.clear();

        ifstream stream(modelPath, ios::binary);
        if (!stream.is_open())
            return false;

        bool isSkeletal = false;
        uint32 maxTextureType = 0;
        uint32 numMaterials = 0;
        uint32 numMeshes = 0;
        uint32 numBones = 0;

        stream.read(reinterpret_cast<char*>(&isSkeletal), sizeof(bool));
        stream.read(reinterpret_cast<char*>(&maxTextureType), sizeof(uint32));
        stream.read(reinterpret_cast<char*>(&numMaterials), sizeof(uint32));
        stream.read(reinterpret_cast<char*>(&numMeshes), sizeof(uint32));
        stream.read(reinterpret_cast<char*>(&numBones), sizeof(uint32));

        if (!stream.good())
            return false;

        for (uint32 materialIndex = 0; materialIndex < numMaterials; ++materialIndex)
        {
            string materialName{};
            if (!Read_ModelMetaString(stream, materialName))
                return false;

            if (!Skip_ModelMetaBytes(stream, sizeof(Vec4) * 4 + sizeof(float) * 4))
                return false;

            uint32 numSlots = 0;
            stream.read(reinterpret_cast<char*>(&numSlots), sizeof(uint32));
            if (!stream.good())
                return false;

            for (uint32 slotIndex = 0; slotIndex < numSlots; ++slotIndex)
            {
                string texturePath{};
                if (!Read_ModelMetaString(stream, texturePath))
                    return false;

                if (!Skip_ModelMetaBytes(stream,
                    sizeof(uint32) * 2 +
                    sizeof(Vec2) * 2 +
                    sizeof(float) * 2 +
                    sizeof(uint32) * 3))
                {
                    return false;
                }
            }
        }

        const uint64 vertexStride = isSkeletal ? sizeof(VTXANIMMESH) : sizeof(VTXMESH);

        for (uint32 meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
        {
            string meshName{};
            if (!Read_ModelMetaString(stream, meshName))
                return false;

            uint32 materialIndex = 0;
            uint32 numVertices = 0;
            uint32 numIndices = 0;
            uint32 numBoneRefs = 0;

            stream.read(reinterpret_cast<char*>(&materialIndex), sizeof(uint32));
            stream.read(reinterpret_cast<char*>(&numVertices), sizeof(uint32));
            stream.read(reinterpret_cast<char*>(&numIndices), sizeof(uint32));
            stream.read(reinterpret_cast<char*>(&numBoneRefs), sizeof(uint32));
            if (!stream.good())
                return false;

            if (!Skip_ModelMetaBytes(stream, vertexStride * static_cast<uint64>(numVertices)))
                return false;

            if (!Skip_ModelMetaBytes(stream, sizeof(uint32) * static_cast<uint64>(numIndices)))
                return false;

            if (isSkeletal)
            {
                if (!Skip_ModelMetaBytes(stream,
                    sizeof(uint32) * static_cast<uint64>(numBoneRefs) +
                    sizeof(Matrix) * static_cast<uint64>(numBoneRefs)))
                {
                    return false;
                }
            }
        }

        outBones.resize(numBones);
        for (uint32 boneIndex = 0; boneIndex < numBones; ++boneIndex)
        {
            if (!Read_ModelMetaString(stream, outBones[boneIndex].name))
                return false;

            stream.read(reinterpret_cast<char*>(&outBones[boneIndex].parentIndex), sizeof(int32));
            if (!stream.good())
                return false;

            if (!Skip_ModelMetaBytes(stream, sizeof(Matrix) * 2))
                return false;
        }

        vector<int32> cachedDepths(outBones.size(), -1);
        for (uint32 boneIndex = 0; boneIndex < outBones.size(); ++boneIndex)
            outBones[boneIndex].depth = Compute_ModelMetaBoneDepth(boneIndex, outBones, cachedDepths);

        return true;
    }
}

Asset_Manager::Asset_Manager()
{
}

Asset_Manager::~Asset_Manager() = default;

HRESULT Asset_Manager::Initialize(const wstring& resourceRoot, bool forceScan)
{
    const auto startupBegin = chrono::steady_clock::now();
    auto stepBegin = startupBegin;

	if (resourceRoot.empty())
        return E_FAIL;

    _resourceRoot = fs::absolute(fs::path(resourceRoot)).lexically_normal().wstring();
    _assetDataFolder = (fs::path(_resourceRoot) / L"Data" / L"Assets").wstring();
    _registryPath = (fs::path(_assetDataFolder) / L"AssetRegistry.json").wstring();
    _cachePath = (fs::path(_assetDataFolder) / L"AssetRegistry.cache.json").wstring();

    _assetRegistryDirty = false;
    const bool registryExists = fs::exists(_registryPath);

    stepBegin = chrono::steady_clock::now();
    Load_Registry();
    LOG_INFO("[StartupTiming] Asset_Manager::Load_Registry elapsed={}ms assetCount={}",
        chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - stepBegin).count(),
        _guidToMeta.size());
    const bool registryLoadedAssets = !_guidToMeta.empty();

    if (_guidToMeta.empty())
    {
        stepBegin = chrono::steady_clock::now();
        Load_Cache();
        LOG_INFO("[StartupTiming] Asset_Manager::Load_Cache elapsed={}ms assetCount={}",
            chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - stepBegin).count(),
            _guidToMeta.size());
    }
    else
    {
        LOG_INFO("[StartupTiming] Asset_Manager::Load_Cache skipped registryAssetCount={}", _guidToMeta.size());
    }

    const bool shouldScan = forceScan || _guidToMeta.empty() || !registryExists || !registryLoadedAssets;
    if (shouldScan)
    {
        stepBegin = chrono::steady_clock::now();
        Scan_And_Register(_resourceRoot);
        LOG_INFO("[StartupTiming] Asset_Manager::Scan_And_Register elapsed={}ms assetCount={} dirty={} forceScan={}",
            chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - stepBegin).count(),
            _guidToMeta.size(),
            _assetRegistryDirty,
            forceScan);

        stepBegin = chrono::steady_clock::now();
        Refresh_Cache();
        LOG_INFO("[StartupTiming] Asset_Manager::Refresh_Cache elapsed={}ms assetCount={} dirty={}",
            chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - stepBegin).count(),
            _guidToMeta.size(),
            _assetRegistryDirty);
    }
    else
    {
        LOG_INFO("[StartupTiming] Asset_Manager::Scan_And_Register skipped forceScan=false registryAssetCount={}", _guidToMeta.size());
        LOG_INFO("[StartupTiming] Asset_Manager::Refresh_Cache skipped fastPath=true");
    }

    if (shouldScan && (_assetRegistryDirty || !fs::exists(_registryPath)))
    {
        stepBegin = chrono::steady_clock::now();
        Save_Registry();
        LOG_INFO("[StartupTiming] Asset_Manager::Save_Registry elapsed={}ms assetCount={}",
            chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - stepBegin).count(),
            _guidToMeta.size());
    }
    else
    {
        LOG_INFO("[StartupTiming] Asset_Manager::Save_Registry skipped dirty={} shouldScan={}", _assetRegistryDirty, shouldScan);
    }

    stepBegin = chrono::steady_clock::now();
    _materialBuilder = MaterialBuilder::Create(fs::path(_resourceRoot) / L"Materials");
    LOG_INFO("[StartupTiming] Asset_Manager::MaterialBuilder::Create elapsed={}ms",
        chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - stepBegin).count());

    LOG_INFO("[StartupTiming] Asset_Manager::Initialize totalElapsed={}ms assetCount={}",
        chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - startupBegin).count(),
        _guidToMeta.size());
    return S_OK;
}

const AssetMeta* Asset_Manager::Find_ByGUID(const string& guid) const
{
	const auto iter = _guidToMeta.find(guid);
	if (iter == _guidToMeta.end())
		return nullptr;

	return &iter->second;
}

vector<AssetMeta> Asset_Manager::Get_AssetsByType(const string& type) const
{
	vector<AssetMeta> assets;

	for (const auto& pair : _guidToMeta)
	{
		const AssetMeta& assetMeta = pair.second;
		if (assetMeta.type == type)
			assets.push_back(assetMeta);
	}

	sort(assets.begin(), assets.end(), [](const AssetMeta& lhs, const AssetMeta& rhs)
	{
		const string lhsPath = String::ToLowerCopy(String::ToString(lhs.relativePath));
		const string rhsPath = String::ToLowerCopy(String::ToString(rhs.relativePath));
		return lhsPath < rhsPath;
	});

	return assets;
}

string Asset_Manager::Find_GUID(const wstring& filePath) const
{
	const wstring key = Normalize_PathKey(filePath);
	const auto iter = _pathToGuid.find(key);
	if (iter == _pathToGuid.end())
		return {};

	return iter->second;
}

wstring Asset_Manager::Resolve_Path(const string& guid) const
{
	const AssetMeta* assetMeta = Find_ByGUID(guid);
	if (nullptr == assetMeta)
		return {};

	return assetMeta->fullPath;
}

const wstring& Asset_Manager::Get_ResourceRoot() const
{
	return _resourceRoot;
}

string Asset_Manager::Register_Asset(const wstring& filePath, const string& type)
{
    try
    {
        const fs::path assetPath = fs::absolute(fs::path(filePath)).lexically_normal();
        if (!fs::exists(assetPath) || !fs::is_regular_file(assetPath))
            return {};

        if (!Should_RegisterAsset(assetPath))
            return {};

        const wstring pathKey = Normalize_PathKey(assetPath.wstring());
        const wstring metaPath = Build_MetaPath(assetPath.wstring());

        if (const auto iter = _pathToGuid.find(pathKey); iter != _pathToGuid.end())
        {
            const string& existingGuid = iter->second;
            const auto guidIter = _guidToMeta.find(existingGuid);

            if (guidIter != _guidToMeta.end())
            {
                AssetMeta& assetMeta = guidIter->second;
                assetMeta.fullPath = assetPath.wstring();

                if (!_resourceRoot.empty())
                    assetMeta.relativePath = fs::relative(assetPath, fs::path(_resourceRoot)).wstring();

                if (assetMeta.type.empty())
                    assetMeta.type = type.empty() ? Detect_AssetType(assetPath.wstring()) : type;

                if (!fs::exists(metaPath))
                    Save_Meta(metaPath, assetMeta);
            }

            return existingGuid;
        }

        if (fs::exists(metaPath) && Load_Meta(metaPath))
        {
            const string loadedGuid = Find_GUID(assetPath.wstring());
            if (!loadedGuid.empty())
                return loadedGuid;
        }

        AssetMeta assetMeta = {};
        assetMeta.guid = Generate_GUID();
        assetMeta.type = type.empty() ? Detect_AssetType(assetPath.wstring()) : type;
        assetMeta.fullPath = assetPath.wstring();

        if (!_resourceRoot.empty())
            assetMeta.relativePath = fs::relative(assetPath, fs::path(_resourceRoot)).wstring();

        _guidToMeta[assetMeta.guid] = assetMeta;
        _pathToGuid[pathKey] = assetMeta.guid;
        _assetRegistryDirty = true;

        Save_Meta(metaPath, assetMeta);
        return assetMeta.guid;
    }
    catch (...)
    {
        return {};
    }
}

void Asset_Manager::Scan_And_Register(const wstring& directory)
{
	 try
    {
        const fs::path directoryPath = fs::path(directory);
        if (!fs::exists(directoryPath) || !fs::is_directory(directoryPath))
            return;

        fs::recursive_directory_iterator iter(directoryPath);
        const fs::recursive_directory_iterator end = {};

        for (; iter != end; ++iter)
        {
            const fs::directory_entry& entry = *iter;
            if (entry.is_directory() && Is_IgnoredPath(entry.path()))
            {
                iter.disable_recursion_pending();
                continue;
            }

            if (!entry.is_regular_file())
                continue;

            if (!Should_RegisterAsset(entry.path()))
                continue;

            const wstring assetPath = entry.path().wstring();
            const wstring metaPath = Build_MetaPath(assetPath);

            if (fs::exists(metaPath))
                Load_Meta(metaPath);
            else
                Register_Asset(assetPath);
        }

        Deduplicate_Assets_ByPath();
    }
    catch (...)
    {
    }
}

void Asset_Manager::Refresh_Cache()
{
	 for (auto iter = _guidToMeta.begin(); iter != _guidToMeta.end();)
    {
        if (iter->second.fullPath.empty() || !fs::exists(iter->second.fullPath))
        {
            iter = _guidToMeta.erase(iter);
            _assetRegistryDirty = true;
        }
        else
            ++iter;
    }

    Deduplicate_Assets_ByPath();
    Rebuild_PathIndex();
}

uint32 Asset_Manager::Clear_OrphanMeta(const wstring& directory)
{
     uint32 removedCount = 0;

    try
    {
        if (!fs::exists(directory) || !fs::is_directory(directory))
            return 0;

        fs::recursive_directory_iterator iter(directory);
        const fs::recursive_directory_iterator end = {};

        for (; iter != end; ++iter)
        {
            const fs::directory_entry& entry = *iter;
            if (entry.is_directory() && Is_IgnoredPath(entry.path()))
            {
                iter.disable_recursion_pending();
                continue;
            }

            if (!entry.is_regular_file())
                continue;

            const fs::path path = entry.path();
            const string fileName = String::ToLowerCopy(String::ToString(path.filename().wstring()));

            if (!EndsWith(fileName, ".meta"))
                continue;

            const wstring assetPath = path.wstring().substr(0, path.wstring().length() - wcslen(L".meta"));
            const bool assetExists = fs::exists(assetPath);
            const bool shouldKeep = assetExists && Should_RegisterAsset(fs::path(assetPath));

            if (shouldKeep)
                continue;

            Unregister_AssetPath(assetPath);
            fs::remove(path);
            ++removedCount;
        }
    }
    catch (...)
    {
    }

    return removedCount;
}

void Asset_Manager::Load_Registry()
{
	Load_AssetList(_registryPath);
}

void Asset_Manager::Save_Registry() const
{
	Save_AssetList(_registryPath);
}

bool Asset_Manager::Load_Meta(const wstring& metaPath)
{
     try
    {
        ifstream file(metaPath);
        if (!file.is_open())
            return false;

        json root = {};
        file >> root;
        file.close();

        if (!root.contains("guid"))
            return false;

        const wstring assetPath = metaPath.substr(0, metaPath.length() - wcslen(L".meta"));
        if (!fs::exists(assetPath))
            return false;

        AssetMeta assetMeta = {};
        assetMeta.guid = root.value("guid", "");
        assetMeta.type = root.value("type", Detect_AssetType(assetPath));
        assetMeta.modelType = root.value("modelType", "");
        assetMeta.fullPath = fs::absolute(fs::path(assetPath)).lexically_normal().wstring();
        const bool shouldRefreshModelBoneMeta =
            assetMeta.type == "Model" &&
            String::ToLowerCopy(String::ToString(fs::path(assetMeta.fullPath).extension().wstring())) == ".model" &&
            (!root.contains("boneCount") || !root.contains("bones"));

        if (!_resourceRoot.empty())
            assetMeta.relativePath = fs::relative(fs::path(assetMeta.fullPath), fs::path(_resourceRoot)).wstring();

        if (assetMeta.guid.empty())
            return false;

        const wstring pathKey = Normalize_PathKey(assetMeta.fullPath);

        if (const auto existing = _pathToGuid.find(pathKey);
            existing != _pathToGuid.end() && existing->second != assetMeta.guid)
        {
            _guidToMeta.erase(existing->second);
            _assetRegistryDirty = true;
        }

        _guidToMeta[assetMeta.guid] = assetMeta;
        _pathToGuid[pathKey] = assetMeta.guid;

        if (shouldRefreshModelBoneMeta)
            Save_Meta(metaPath, assetMeta);

        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool Asset_Manager::Save_Meta(const wstring& metaPath, const AssetMeta& assetMeta) const
{
    try
    {
        const string extension = String::ToLowerCopy(String::ToString(fs::path(assetMeta.fullPath).extension().wstring()));
        if (extension == ".wav")
            return true;

        json root = {};

        root["guid"] = assetMeta.guid;
        root["type"] = assetMeta.type;

        if (!assetMeta.modelType.empty())
            root["modelType"] = assetMeta.modelType;

        if (assetMeta.type == "Model" && extension == ".model")
        {
            vector<ModelMetaBoneEntry> bones{};
            if (Try_ReadModelMetaBones(assetMeta.fullPath, bones))
            {
                root["boneCount"] = bones.size();
                root["bones"] = json::array();

                for (uint32 boneIndex = 0; boneIndex < bones.size(); ++boneIndex)
                {
                    json bone = {};
                    bone["depth"] = bones[boneIndex].depth;
                    bone["index"] = boneIndex;
                    bone["name"] = bones[boneIndex].name;
                    bone["parentIndex"] = bones[boneIndex].parentIndex;
                    root["bones"].push_back(bone);
                }
            }
        }

        ofstream file(metaPath);
        if (!file.is_open())
            return false;

        file << root.dump(4);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void Asset_Manager::Load_Cache()
{
	try
	{
		Load_AssetList(_cachePath);
	}
	catch (...)
	{
	}
}

void Asset_Manager::Save_Cache() const
{
	try
	{
		Save_AssetList(_cachePath);
	}
	catch (...)
	{
	}
}

void Asset_Manager::Save_AssetList(const wstring& path) const
{
	try
	{
		if (path.empty())
			return;

		json JsonData = {};
		JsonData["assets"] = json::array();

		for (const auto& Pair : _guidToMeta)
		{
			const AssetMeta& AssetMeta = Pair.second;

			json Item = {};
			Item["guid"] = AssetMeta.guid;
			Item["type"] = AssetMeta.type;
			Item["modelType"] = AssetMeta.modelType;
			Item["fullPath"] = String::ToString(AssetMeta.fullPath);
			Item["relativePath"] = String::ToString(AssetMeta.relativePath);

			JsonData["assets"].push_back(Item);
		}

		fs::create_directories(fs::path(path).parent_path());

		ofstream File(path);
		if (!File.is_open())
			return;

		File << JsonData.dump(4);
	}
	catch (...)
	{
	}
}

void Asset_Manager::Load_AssetList(const wstring& path)
{
	try
	{
		if (path.empty() || !fs::exists(path))
			return;

		ifstream File(path);
		if (!File.is_open())
			return;

		json JsonData = {};
		File >> JsonData;

		for (const json& Item : JsonData.value("assets", json::array()))
		{
			AssetMeta AssetMeta = {};
			AssetMeta.guid = Item.value("guid", "");
			AssetMeta.type = Item.value("type", "");
			AssetMeta.modelType = Item.value("modelType", "");
			AssetMeta.relativePath = String::ToWString(Item.value("relativePath", ""));

			const string fullPath = Item.value("fullPath", "");
			if (!AssetMeta.relativePath.empty())
				AssetMeta.fullPath = (fs::path(_resourceRoot) / AssetMeta.relativePath).lexically_normal().wstring();
			else if (!fullPath.empty())
				AssetMeta.fullPath = String::ToWString(fullPath);

			if (AssetMeta.guid.empty() || AssetMeta.fullPath.empty())
				continue;

			if (AssetMeta.type.empty())
				AssetMeta.type = Detect_AssetType(AssetMeta.fullPath);

			_guidToMeta[AssetMeta.guid] = AssetMeta;
		}

		Rebuild_PathIndex();
	}
	catch (...)
	{
	}
}

string Asset_Manager::Detect_AssetType(const wstring& filePath) const
{
    const string extension = String::ToLowerCopy(String::ToString(fs::path(filePath).extension().wstring()));

    if (extension == ".json")
    {
        string relativePath = String::ToLowerCopy(fs::path(filePath).generic_string());

        try
        {
            if (!_resourceRoot.empty())
                relativePath = String::ToLowerCopy(fs::relative(fs::path(filePath), fs::path(_resourceRoot)).generic_string());
        }
        catch (...)
        {
        }

        if (0 == relativePath.find("ui/ui_json/"))
            return "UIJson";

        if (0 == relativePath.find("data/json/animnotifies/"))
            return "AnimNotify";

        if (0 == relativePath.find("data/json/bt/"))
            return "BehaviorTree";

        if (0 == relativePath.find("data/json/camerapreset/"))
            return "CameraPreset";

        if (0 == relativePath.find("data/json/cinematics/"))
            return "Cinematic";

        if (0 == relativePath.find("data/json/prefabs/"))
            return "Prefab";

        if (0 == relativePath.find("materials/"))
            return "Material";

        if (0 == relativePath.find("effects/assets/"))
            return "Effect";

        if (0 == relativePath.find("effects/materials/"))
            return "EffectMaterial";

        return "Json";
    }

    if (extension == ".model" || extension == ".meshbin")
        return "Model";

    if (extension == ".anim" || extension == ".clip" || extension == ".psa")
        return "Animation";

    if (extension == ".dds" || extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga")
        return "Texture";

    if (extension == ".hlsl" || extension == ".fx")
        return "Shader";

    if (extension == ".wav" || extension == ".mp3" || extension == ".ogg")
        return "Sound";

    if (extension == ".spritefont" || extension == ".ttf" || extension == ".otf")
        return "Font";

    return "Unknown";
}

bool Asset_Manager::Is_IgnoredPath(const fs::path& path) const
{
    const fs::path normalizedPath = path.lexically_normal();
    const fs::path resourceRootPath = _resourceRoot.empty() ? fs::path() : fs::path(_resourceRoot).lexically_normal();

    for (const string& ignoreFolder : _ignoreFolders)
    {
        const string lowerIgnore = String::ToLowerCopy(ignoreFolder);
        if (lowerIgnore.find('/') == string::npos && lowerIgnore.find('\\') == string::npos)
        {
            for (const fs::path& part : normalizedPath)
            {
                if (String::ToLowerCopy(String::ToString(part.wstring())) == lowerIgnore)
                    return true;
            }

            continue;
        }

        if (resourceRootPath.empty())
            continue;

        string relativePath = {};
        try
        {
            relativePath = String::ToLowerCopy(fs::relative(normalizedPath, resourceRootPath).generic_string());
        }
        catch (...)
        {
            continue;
        }

        if (relativePath == lowerIgnore || 0 == relativePath.find(lowerIgnore + "/"))
            return true;
    }

    return false;
}

bool Asset_Manager::Should_RegisterAsset(const fs::path& path) const
{
    const string name = String::ToLowerCopy(String::ToString(path.filename().wstring()));
    const string extension = String::ToLowerCopy(String::ToString(path.extension().wstring()));

    if (Is_IgnoredPath(path))
        return false;

    if (EndsWith(name, ".meta") ||
        name == "assetregistry.json" ||
        name == "assetregistry.cache.json" ||
        extension == ".xlsx" ||
        extension == ".csv")
    {
        return false;
    }

    if (extension == ".json")
        return Should_RegisterJsonAsset(path);

    return extension == ".model"
        || extension == ".meshbin"
        || extension == ".anim"
        || extension == ".clip"
        || extension == ".psa"
        || extension == ".dds"
        || extension == ".png"
        || extension == ".jpg"
        || extension == ".jpeg"
        || extension == ".tga"
        || extension == ".hlsl"
        || extension == ".fx"
        || extension == ".wav"
        || extension == ".mp3"
        || extension == ".ogg"
        || extension == ".spritefont"
        || extension == ".ttf"
        || extension == ".otf";
}

bool Asset_Manager::Should_RegisterJsonAsset(const fs::path& path) const
{
    if (_resourceRoot.empty())
        return false;

    string relativePath = {};

    try
    {
        relativePath = String::ToLowerCopy(fs::relative(path, fs::path(_resourceRoot)).generic_string());
    }
    catch (...)
    {
        return false;
    }

    return 0 == relativePath.find("ui/ui_json/") ||
        0 == relativePath.find("data/json/animnotifies/") ||
        0 == relativePath.find("data/json/bt/") ||
        0 == relativePath.find("data/json/camerapreset/") ||
        0 == relativePath.find("data/json/cinematics/") ||
        0 == relativePath.find("data/json/prefabs/") ||
        0 == relativePath.find("materials/") ||
        0 == relativePath.find("effects/assets/") ||
        0 == relativePath.find("effects/materials/");
}

wstring Asset_Manager::Build_MetaPath(const wstring& assetPath)
{
    return assetPath + L".meta";
}

void Asset_Manager::Rebuild_PathIndex()
{
	_pathToGuid.clear();

	for (const auto& Pair : _guidToMeta)
	{
		if (!Pair.second.fullPath.empty())
			_pathToGuid[Normalize_PathKey(Pair.second.fullPath)] = Pair.first;
	}
}

void Asset_Manager::Deduplicate_Assets_ByPath(const string& preferGuid)
{
    umap<wstring, string> chosenByPath{};
    vector<string> staleGuids{};

    for (const auto& [guid, meta] : _guidToMeta)
    {
        const wstring pathKey = Normalize_PathKey(meta.fullPath);
        const auto found = chosenByPath.find(pathKey);

        if (found == chosenByPath.end())
        {
            chosenByPath[pathKey] = guid;
            continue;
        }

        if (!preferGuid.empty() && guid == preferGuid)
        {
            staleGuids.push_back(found->second);
            chosenByPath[pathKey] = guid;
        }
        else
        {
            staleGuids.push_back(guid);
        }
    }

    if (!staleGuids.empty())
        _assetRegistryDirty = true;

    for (const string& guid : staleGuids)
        _guidToMeta.erase(guid);

    Rebuild_PathIndex();
}

void Asset_Manager::Unregister_AssetPath(const wstring& assetPath)
{
    try
    {
        if (assetPath.empty())
            return;

        const fs::path normalizedPath = fs::absolute(fs::path(assetPath)).lexically_normal();
        const wstring pathKey = Normalize_PathKey(normalizedPath.wstring());

        const auto iter = _pathToGuid.find(pathKey);
        if (iter == _pathToGuid.end())
            return;

        const string guid = iter->second;
        _pathToGuid.erase(iter);

        const auto guidIter = _guidToMeta.find(guid);
        if (guidIter != _guidToMeta.end())
            _guidToMeta.erase(guidIter);
    }
    catch (...)
    {
    }
}

bool Asset_Manager::EndsWith(const string& value, const string& suffix)
{
	if (value.length() < suffix.length())
		return false;

	return equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

wstring Asset_Manager::Normalize_PathKey(const wstring& path)
{
	try
	{
		wstring key = fs::absolute(fs::path(path)).lexically_normal().wstring();
		transform(key.begin(), key.end(), key.begin(), [](wchar_t ch)
		{
			return static_cast<wchar_t>(towlower(ch));
		});

		return key;
	}
	catch (...)
	{
		return path;
	}
}

string Asset_Manager::Generate_GUID()
{
	GUID Guid = {};
	if (FAILED(CoCreateGuid(&Guid)))
		return {};

	wchar_t guidBuffer[64] = {};
	StringFromGUID2(Guid, guidBuffer, static_cast<int>(size(guidBuffer)));

	string guid = String::ToLowerCopy(String::ToString(guidBuffer));
	guid.erase(remove(guid.begin(), guid.end(), '{'), guid.end());
	guid.erase(remove(guid.begin(), guid.end(), '}'), guid.end());

	return guid;
}

Unique<Asset_Manager> Asset_Manager::Create(const wstring& resourceRoot, bool forceScan)
{
    Unique<Asset_Manager> instance(new Asset_Manager());

    if (FAILED(instance->Initialize(resourceRoot, forceScan)))
    {
        MSG_BOX("Create Failed : Asset Manager");

        return nullptr;
    }

    return instance;
}

void Asset_Manager::Free()
{
	_pathToGuid.clear();
	_guidToMeta.clear();

	__super::Free();
}

