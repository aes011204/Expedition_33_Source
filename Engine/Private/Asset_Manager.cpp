#include "Asset_Manager.h"

#include <objbase.h>

Asset_Manager::Asset_Manager()
{
}

HRESULT Asset_Manager::Initialize(const wstring& resourceRoot)
{
	if (resourceRoot.empty())
		return E_FAIL;

	try
	{
		_resourceRoot = fs::absolute(fs::path(resourceRoot)).lexically_normal().wstring();
		_assetDataFolder = (fs::path(_resourceRoot) / L"Data" / L"Assets").wstring();
		_registryPath = (fs::path(_assetDataFolder) / L"AssetRegistry.json").wstring();
		_cachePath = (fs::path(_assetDataFolder) / L"AssetRegistry.cache.json").wstring();

		Load_Cache();
		Load_Registry();
		Scan_And_Register(_resourceRoot);
		Refresh_Cache();
		Save_Registry();
		Save_Cache();
	}
	catch (...)
	{
		return E_FAIL;
	}

	return S_OK;
}

const FAssetMeta* Asset_Manager::Find_ByGUID(const string& guid) const
{
	const auto iter = _guidToMeta.find(guid);
	if (iter == _guidToMeta.end())
		return nullptr;

	return &iter->second;
}

vector<FAssetMeta> Asset_Manager::Get_AssetsByType(const string& type) const
{
	vector<FAssetMeta> assets;

	for (const auto& pair : _guidToMeta)
	{
		const FAssetMeta& assetMeta = pair.second;
		if (assetMeta.type == type)
			assets.push_back(assetMeta);
	}

	sort(assets.begin(), assets.end(), [](const FAssetMeta& lhs, const FAssetMeta& rhs)
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
	const FAssetMeta* assetMeta = Find_ByGUID(guid);
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
		const fs::path AssetPath = fs::absolute(fs::path(filePath)).lexically_normal();
		if (!fs::exists(AssetPath) || !fs::is_regular_file(AssetPath))
			return {};

		const wstring key = Normalize_PathKey(AssetPath.wstring());
		const auto iterPath = _pathToGuid.find(key);
		if (iterPath != _pathToGuid.end())
			return iterPath->second;

		const wstring legacyMetaPath = AssetPath.wstring() + L".meta";
		if (fs::exists(legacyMetaPath) && Load_LegacyMeta(legacyMetaPath))
		{
			const string loadedGuid = Find_GUID(AssetPath.wstring());
			if (!loadedGuid.empty())
				return loadedGuid;
		}

		FAssetMeta AssetMeta = {};
		AssetMeta.guid = Generate_GUID();
		AssetMeta.type = type.empty() ? Detect_AssetType(AssetPath.wstring()) : type;
		AssetMeta.fullPath = AssetPath.wstring();

		if (!_resourceRoot.empty())
		{
			const fs::path RelativePath = fs::relative(AssetPath, fs::path(_resourceRoot));
			AssetMeta.relativePath = RelativePath.wstring();
		}

		_guidToMeta.emplace(AssetMeta.guid, AssetMeta);
		_pathToGuid.emplace(key, AssetMeta.guid);

		return AssetMeta.guid;
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
		const fs::path DirectoryPath = fs::path(directory);
		if (!fs::exists(DirectoryPath) || !fs::is_directory(DirectoryPath))
			return;

		for (const fs::directory_entry& Entry : fs::recursive_directory_iterator(DirectoryPath))
		{
			if (!Entry.is_regular_file())
				continue;

			if (!Should_RegisterAsset(Entry.path()))
				continue;

			Register_Asset(Entry.path().wstring());
		}
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
			iter = _guidToMeta.erase(iter);
		else
			++iter;
	}

	Rebuild_PathIndex();
}

void Asset_Manager::Load_Registry()
{
	Load_AssetList(_registryPath);
}

void Asset_Manager::Save_Registry() const
{
	Save_AssetList(_registryPath);
}

bool Asset_Manager::Load_LegacyMeta(const wstring& metaPath)
{
	try
	{
		ifstream File(metaPath);
		if (!File.is_open())
			return false;

		json JsonData = {};
		File >> JsonData;

		const fs::path MetaPath = fs::path(metaPath);
		const wstring metaFileName = MetaPath.filename().wstring();
		const wstring assetFileName = metaFileName.substr(0, metaFileName.length() - wcslen(L".meta"));
		const fs::path AssetPath = (MetaPath.parent_path() / assetFileName).lexically_normal();

		if (!fs::exists(AssetPath))
			return false;

		FAssetMeta AssetMeta = {};
		AssetMeta.guid = JsonData.value("guid", "");
		AssetMeta.type = JsonData.value("type", Detect_AssetType(AssetPath.wstring()));
		AssetMeta.modelType = JsonData.value("modelType", "");
		AssetMeta.fullPath = fs::absolute(AssetPath).lexically_normal().wstring();

		if (!_resourceRoot.empty())
			AssetMeta.relativePath = fs::relative(fs::path(AssetMeta.fullPath), fs::path(_resourceRoot)).wstring();

		if (AssetMeta.guid.empty())
			return false;

		_guidToMeta[AssetMeta.guid] = AssetMeta;
		_pathToGuid[Normalize_PathKey(AssetMeta.fullPath)] = AssetMeta.guid;

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
			const FAssetMeta& AssetMeta = Pair.second;

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
			FAssetMeta AssetMeta = {};
			AssetMeta.guid = Item.value("guid", "");
			AssetMeta.type = Item.value("type", "");
			AssetMeta.modelType = Item.value("modelType", "");
			AssetMeta.relativePath = String::ToWString(Item.value("relativePath", ""));

			const string fullPath = Item.value("fullPath", "");
			if (!fullPath.empty())
				AssetMeta.fullPath = String::ToWString(fullPath);
			else if (!AssetMeta.relativePath.empty())
				AssetMeta.fullPath = (fs::path(_resourceRoot) / AssetMeta.relativePath).lexically_normal().wstring();

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
	const string name = String::ToLowerCopy(String::ToString(fs::path(filePath).filename().wstring()));
	const string extension = String::ToLowerCopy(String::ToString(fs::path(filePath).extension().wstring()));

	if (EndsWith(name, ".prefab.json"))
		return "Prefab";
	if (EndsWith(name, ".level.json"))
		return "Level";
	if (EndsWith(name, ".matinst.json"))
		return "MaterialInstance";
	if (extension == ".meshbin")
		return "Model";
	if (extension == ".dds" || extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga")
		return "Texture";
	if (extension == ".hlsl" || extension == ".fx")
		return "Shader";
	if (extension == ".clip" || extension == ".psa")
		return "Animation";
	if (extension == ".wav" || extension == ".mp3" || extension == ".ogg")
		return "Sound";
	if (extension == ".spritefont" || extension == ".ttf" || extension == ".otf")
		return "Font";
	if (extension == ".xlsx" || extension == ".csv")
		return "DataTable";

	return "Unknown";
}

bool Asset_Manager::Should_RegisterAsset(const fs::path& path) const
{
	const string name = String::ToLowerCopy(String::ToString(path.filename().wstring()));
	const string extension = String::ToLowerCopy(String::ToString(path.extension().wstring()));

	if (extension == ".spritefont" && !_resourceRoot.empty())
	{
		try
		{
			string relativePath = String::ToLowerCopy(String::ToString(fs::relative(path, fs::path(_resourceRoot)).wstring()));
			replace(relativePath.begin(), relativePath.end(), '\\', '/');

			if (0 == relativePath.find("fonts/"))
				return true;
		}
		catch (...)
		{
		}
	}

	if (!_resourceRoot.empty())
	{
		const fs::path RegistryDirectory = fs::path(_assetDataFolder).lexically_normal();
		const fs::path AssetPath = path.lexically_normal();
		const string registryDirectory = String::ToLowerCopy(String::ToString(RegistryDirectory.wstring()));
		const string assetPath = String::ToLowerCopy(String::ToString(AssetPath.wstring()));

		if (0 == assetPath.find(registryDirectory))
			return false;
	}

	for (const fs::path& Part : path)
	{
		const string part = String::ToLowerCopy(String::ToString(Part.wstring()));
		for (const string& ignoreFolder : _ignoreFolders)
		{
			if (part == String::ToLowerCopy(ignoreFolder))
				return false;
		}
	}

	if (EndsWith(name, ".meta") || name == "assetregistry.json" || name == "assetregistry.cache.json")
		return false;

	if (EndsWith(name, ".prefab.json") || EndsWith(name, ".level.json") || EndsWith(name, ".matinst.json"))
		return true;

	if (EndsWith(name, ".material.json"))
		return false;

	return extension == ".meshbin"
           || extension == ".dds"
           || extension == ".png"
           || extension == ".jpg"
           || extension == ".jpeg"
           || extension == ".tga"
           || extension == ".hlsl"
           || extension == ".fx"
           || extension == ".clip"
           || extension == ".psa"
           || extension == ".wav"
           || extension == ".mp3"
           || extension == ".ogg"
           || extension == ".spritefont"
           || extension == ".ttf"
           || extension == ".otf"
           || extension == ".xlsx"
           || extension == ".csv";
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

Unique<Asset_Manager> Asset_Manager::Create(const wstring& resourceRoot)
{
	Unique<Asset_Manager> instance{ new Asset_Manager() };
	if (FAILED(instance->Initialize(resourceRoot)))
		return nullptr;

	return instance;
}

void Asset_Manager::Free()
{
	_pathToGuid.clear();
	_guidToMeta.clear();

	__super::Free();
}

