#pragma once

#include <filesystem>
#include "Base.h"

NS_BEGIN(Engine)

class ENGINE_DLL Asset_Manager final : public Base
{
private:
	Asset_Manager();

public:
	virtual ~Asset_Manager() = default;

public:
	HRESULT Initialize(const wstring& strResourceRoot);
	const FAssetMeta* Find_ByGUID(const string& strGuid) const;
	string Find_GUID(const wstring& strFilePath) const;
	wstring Resolve_Path(const string& strGuid) const;
	string Register_Asset(const wstring& strFilePath, const string& strType = "");
	void Scan_And_Register(const wstring& strDirectory);
	void Refresh_Cache();

private:
	void Load_Registry();
	void Save_Registry() const;
	bool Load_LegacyMeta(const wstring& strMetaPath);
	void Load_Cache();
	void Save_Cache() const;
	void Save_AssetList(const wstring& strPath) const;
	void Load_AssetList(const wstring& strPath);
	string Detect_AssetType(const wstring& strFilePath) const;
	bool Should_RegisterAsset(const std::filesystem::path& Path) const;
	void Rebuild_PathIndex();
	static string ToLowerCopy(string strValue);
	static string ToString(const wstring& strValue);
	static wstring ToWString(const string& strValue);
	static bool EndsWith(const string& strValue, const string& strSuffix);
	static wstring Normalize_PathKey(const wstring& strPath);
	static string Generate_GUID();

private:
	wstring _resourceRoot = {};
	wstring _assetDataFolder = {};
	wstring _registryPath = {};
	wstring _cachePath = {};
	unordered_map<string, FAssetMeta> _guidToMeta = {};
	unordered_map<wstring, string> _pathToGuid = {};
	vector<string> _ignoreFolders = { ".git", ".vs", "Fonts", "Data/Assets" };

public:
	static Unique<Asset_Manager> Create(const wstring& strResourceRoot);
	virtual void Free() override;
};

NS_END
