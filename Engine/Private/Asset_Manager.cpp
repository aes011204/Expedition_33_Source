#include "Asset_Manager.h"

#include <objbase.h>
#include <filesystem>

Asset_Manager::Asset_Manager()
{
}

HRESULT Asset_Manager::Initialize(const wstring& strResourceRoot)
{
	if (strResourceRoot.empty())
		return E_FAIL;

	try
	{
		_resourceRoot = fs::absolute(fs::path(strResourceRoot)).lexically_normal().wstring();
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

const FAssetMeta* Asset_Manager::Find_ByGUID(const string& strGuid) const
{
	const auto iter = _guidToMeta.find(strGuid);
	if (iter == _guidToMeta.end())
		return nullptr;

	return &iter->second;
}

string Asset_Manager::Find_GUID(const wstring& strFilePath) const
{
	const wstring strKey = Normalize_PathKey(strFilePath);
	const auto iter = _pathToGuid.find(strKey);
	if (iter == _pathToGuid.end())
		return {};

	return iter->second;
}

wstring Asset_Manager::Resolve_Path(const string& strGuid) const
{
	const FAssetMeta* pAssetMeta = Find_ByGUID(strGuid);
	if (nullptr == pAssetMeta)
		return {};

	return pAssetMeta->strFullPath;
}

string Asset_Manager::Register_Asset(const wstring& strFilePath, const string& strType)
{
	try
	{
		const fs::path AssetPath = fs::absolute(fs::path(strFilePath)).lexically_normal();
		if (!fs::exists(AssetPath) || !fs::is_regular_file(AssetPath))
			return {};

		const wstring strKey = Normalize_PathKey(AssetPath.wstring());
		const auto iterPath = _pathToGuid.find(strKey);
		if (iterPath != _pathToGuid.end())
			return iterPath->second;

		const wstring strLegacyMetaPath = AssetPath.wstring() + L".meta";
		if (fs::exists(strLegacyMetaPath) && Load_LegacyMeta(strLegacyMetaPath))
		{
			const string strLoadedGuid = Find_GUID(AssetPath.wstring());
			if (!strLoadedGuid.empty())
				return strLoadedGuid;
		}

		FAssetMeta AssetMeta = {};
		AssetMeta.strGuid = Generate_GUID();
		AssetMeta.strType = strType.empty() ? Detect_AssetType(AssetPath.wstring()) : strType;
		AssetMeta.strFullPath = AssetPath.wstring();

		if (!_resourceRoot.empty())
		{
			const fs::path RelativePath = fs::relative(AssetPath, fs::path(_resourceRoot));
			AssetMeta.strRelativePath = RelativePath.wstring();
		}

		_guidToMeta.emplace(AssetMeta.strGuid, AssetMeta);
		_pathToGuid.emplace(strKey, AssetMeta.strGuid);

		return AssetMeta.strGuid;
	}
	catch (...)
	{
		return {};
	}
}

void Asset_Manager::Scan_And_Register(const wstring& strDirectory)
{
	try
	{
		const fs::path DirectoryPath = fs::path(strDirectory);
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
		if (iter->second.strFullPath.empty() || !fs::exists(iter->second.strFullPath))
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

bool Asset_Manager::Load_LegacyMeta(const wstring& strMetaPath)
{
	try
	{
		ifstream File(strMetaPath);
		if (!File.is_open())
			return false;

		json JsonData = {};
		File >> JsonData;

		const fs::path MetaPath = fs::path(strMetaPath);
		const wstring strMetaFileName = MetaPath.filename().wstring();
		const wstring strAssetFileName = strMetaFileName.substr(0, strMetaFileName.length() - wcslen(L".meta"));
		const fs::path AssetPath = (MetaPath.parent_path() / strAssetFileName).lexically_normal();

		if (!fs::exists(AssetPath))
			return false;

		FAssetMeta AssetMeta = {};
		AssetMeta.strGuid = JsonData.value("guid", "");
		AssetMeta.strType = JsonData.value("type", Detect_AssetType(AssetPath.wstring()));
		AssetMeta.strModelType = JsonData.value("modelType", "");
		AssetMeta.strFullPath = fs::absolute(AssetPath).lexically_normal().wstring();

		if (!_resourceRoot.empty())
			AssetMeta.strRelativePath = fs::relative(fs::path(AssetMeta.strFullPath), fs::path(_resourceRoot)).wstring();

		if (AssetMeta.strGuid.empty())
			return false;

		_guidToMeta[AssetMeta.strGuid] = AssetMeta;
		_pathToGuid[Normalize_PathKey(AssetMeta.strFullPath)] = AssetMeta.strGuid;

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

void Asset_Manager::Save_AssetList(const wstring& strPath) const
{
	try
	{
		if (strPath.empty())
			return;

		json JsonData = {};
		JsonData["assets"] = json::array();

		for (const auto& Pair : _guidToMeta)
		{
			const FAssetMeta& AssetMeta = Pair.second;

			json Item = {};
			Item["guid"] = AssetMeta.strGuid;
			Item["type"] = AssetMeta.strType;
			Item["modelType"] = AssetMeta.strModelType;
			Item["fullPath"] = ToString(AssetMeta.strFullPath);
			Item["relativePath"] = ToString(AssetMeta.strRelativePath);

			JsonData["assets"].push_back(Item);
		}

		fs::create_directories(fs::path(strPath).parent_path());

		ofstream File(strPath);
		if (!File.is_open())
			return;

		File << JsonData.dump(4);
	}
	catch (...)
	{
	}
}

void Asset_Manager::Load_AssetList(const wstring& strPath)
{
	try
	{
		if (strPath.empty() || !fs::exists(strPath))
			return;

		ifstream File(strPath);
		if (!File.is_open())
			return;

		json JsonData = {};
		File >> JsonData;

		for (const json& Item : JsonData.value("assets", json::array()))
		{
			FAssetMeta AssetMeta = {};
			AssetMeta.strGuid = Item.value("guid", "");
			AssetMeta.strType = Item.value("type", "");
			AssetMeta.strModelType = Item.value("modelType", "");
			AssetMeta.strRelativePath = ToWString(Item.value("relativePath", ""));

			const string strFullPath = Item.value("fullPath", "");
			if (!strFullPath.empty())
				AssetMeta.strFullPath = ToWString(strFullPath);
			else if (!AssetMeta.strRelativePath.empty())
				AssetMeta.strFullPath = (fs::path(_resourceRoot) / AssetMeta.strRelativePath).lexically_normal().wstring();

			if (AssetMeta.strGuid.empty() || AssetMeta.strFullPath.empty())
				continue;

			if (AssetMeta.strType.empty())
				AssetMeta.strType = Detect_AssetType(AssetMeta.strFullPath);

			_guidToMeta[AssetMeta.strGuid] = AssetMeta;
		}

		Rebuild_PathIndex();
	}
	catch (...)
	{
	}
}

string Asset_Manager::Detect_AssetType(const wstring& strFilePath) const
{
	const string strName = ToLowerCopy(ToString(fs::path(strFilePath).filename().wstring()));
	const string strExtension = ToLowerCopy(ToString(fs::path(strFilePath).extension().wstring()));

	if (EndsWith(strName, ".prefab.json"))
		return "Prefab";
	if (EndsWith(strName, ".level.json"))
		return "Level";
	if (EndsWith(strName, ".matinst.json"))
		return "MaterialInstance";
	if (strExtension == ".meshbin")
		return "Model";
	if (strExtension == ".dds" || strExtension == ".png" || strExtension == ".jpg" || strExtension == ".jpeg" || strExtension == ".tga")
		return "Texture";
	if (strExtension == ".hlsl" || strExtension == ".fx")
		return "Shader";
	if (strExtension == ".clip" || strExtension == ".psa")
		return "Animation";
	if (strExtension == ".wav" || strExtension == ".mp3" || strExtension == ".ogg")
		return "Sound";
	if (strExtension == ".ttf" || strExtension == ".otf")
		return "Font";
	if (strExtension == ".xlsx" || strExtension == ".csv")
		return "DataTable";

	return "Unknown";
}

bool Asset_Manager::Should_RegisterAsset(const fs::path& Path) const
{
	const string strName = ToLowerCopy(ToString(Path.filename().wstring()));
	const string strExtension = ToLowerCopy(ToString(Path.extension().wstring()));

	if (!_resourceRoot.empty())
	{
		const fs::path RegistryDirectory = fs::path(_assetDataFolder).lexically_normal();
		const fs::path AssetPath = Path.lexically_normal();
		const string strRegistryDirectory = ToLowerCopy(ToString(RegistryDirectory.wstring()));
		const string strAssetPath = ToLowerCopy(ToString(AssetPath.wstring()));

		if (0 == strAssetPath.find(strRegistryDirectory))
			return false;
	}

	for (const fs::path& Part : Path)
	{
		const string strPart = ToLowerCopy(ToString(Part.wstring()));
		for (const string& strIgnoreFolder : _ignoreFolders)
		{
			if (strPart == ToLowerCopy(strIgnoreFolder))
				return false;
		}
	}

	if (EndsWith(strName, ".meta") || strName == "assetregistry.json" || strName == "assetregistry.cache.json")
		return false;

	if (EndsWith(strName, ".prefab.json") || EndsWith(strName, ".level.json") || EndsWith(strName, ".matinst.json"))
		return true;

	if (EndsWith(strName, ".material.json"))
		return false;

	return strExtension == ".meshbin"
		|| strExtension == ".dds"
		|| strExtension == ".png"
		|| strExtension == ".jpg"
		|| strExtension == ".jpeg"
		|| strExtension == ".tga"
		|| strExtension == ".hlsl"
		|| strExtension == ".fx"
		|| strExtension == ".clip"
		|| strExtension == ".psa"
		|| strExtension == ".wav"
		|| strExtension == ".mp3"
		|| strExtension == ".ogg"
		|| strExtension == ".ttf"
		|| strExtension == ".otf"
		|| strExtension == ".xlsx"
		|| strExtension == ".csv";
}

void Asset_Manager::Rebuild_PathIndex()
{
	_pathToGuid.clear();

	for (const auto& Pair : _guidToMeta)
	{
		if (!Pair.second.strFullPath.empty())
			_pathToGuid[Normalize_PathKey(Pair.second.strFullPath)] = Pair.first;
	}
}

string Asset_Manager::ToLowerCopy(string strValue)
{
	transform(strValue.begin(), strValue.end(), strValue.begin(), [](unsigned char ch)
	{
		return static_cast<char>(tolower(ch));
	});

	return strValue;
}

string Asset_Manager::ToString(const wstring& strValue)
{
	if (strValue.empty())
		return {};

	const int iSize = WideCharToMultiByte(CP_UTF8, 0, strValue.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (iSize <= 0)
		return {};

	string strResult(static_cast<size_t>(iSize - 1), '\0');
	WideCharToMultiByte(CP_UTF8, 0, strValue.c_str(), -1, strResult.data(), iSize, nullptr, nullptr);
	return strResult;
}

wstring Asset_Manager::ToWString(const string& strValue)
{
	if (strValue.empty())
		return {};

	const int iSize = MultiByteToWideChar(CP_UTF8, 0, strValue.c_str(), -1, nullptr, 0);
	if (iSize <= 0)
		return {};

	wstring strResult(static_cast<size_t>(iSize - 1), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, strValue.c_str(), -1, strResult.data(), iSize);
	return strResult;
}

bool Asset_Manager::EndsWith(const string& strValue, const string& strSuffix)
{
	if (strValue.length() < strSuffix.length())
		return false;

	return equal(strSuffix.rbegin(), strSuffix.rend(), strValue.rbegin());
}

wstring Asset_Manager::Normalize_PathKey(const wstring& strPath)
{
	try
	{
		wstring strKey = fs::absolute(fs::path(strPath)).lexically_normal().wstring();
		transform(strKey.begin(), strKey.end(), strKey.begin(), [](wchar_t ch)
		{
			return static_cast<wchar_t>(towlower(ch));
		});

		return strKey;
	}
	catch (...)
	{
		return strPath;
	}
}

string Asset_Manager::Generate_GUID()
{
	GUID Guid = {};
	if (FAILED(CoCreateGuid(&Guid)))
		return {};

	wchar_t szGuid[64] = {};
	StringFromGUID2(Guid, szGuid, static_cast<int>(size(szGuid)));

	string strGuid = ToLowerCopy(ToString(szGuid));
	strGuid.erase(remove(strGuid.begin(), strGuid.end(), '{'), strGuid.end());
	strGuid.erase(remove(strGuid.begin(), strGuid.end(), '}'), strGuid.end());

	return strGuid;
}

Unique<Asset_Manager> Asset_Manager::Create(const wstring& strResourceRoot)
{
	Unique<Asset_Manager> pInstance{ new Asset_Manager() };
	if (FAILED(pInstance->Initialize(strResourceRoot)))
		return nullptr;

	return pInstance;
}

void Asset_Manager::Free()
{
	_pathToGuid.clear();
	_guidToMeta.clear();

	__super::Free();
}

