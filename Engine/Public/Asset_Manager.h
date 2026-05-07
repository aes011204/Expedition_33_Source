#pragma once
#include "Base.h"

NS_BEGIN(Engine)

class ENGINE_DLL Asset_Manager final : public Base
{
private:
	Asset_Manager();

public:
	virtual ~Asset_Manager() = default;

public:
	HRESULT Initialize(const wstring& resourceRoot);
	const FAssetMeta* Find_ByGUID(const string& guid) const;
	vector<FAssetMeta> Get_AssetsByType(const string& type) const;
	string Find_GUID(const wstring& filePath) const;
	wstring Resolve_Path(const string& guid) const;
	const wstring& Get_ResourceRoot() const;
	string Register_Asset(const wstring& filePath, const string& type = "");
	void Scan_And_Register(const wstring& directory);
	void Refresh_Cache();

private:
	void Load_Registry();
	void Save_Registry() const;
	bool Load_LegacyMeta(const wstring& metaPath);
	void Load_Cache();
	void Save_Cache() const;
	void Save_AssetList(const wstring& path) const;
	void Load_AssetList(const wstring& path);
	string Detect_AssetType(const wstring& filePath) const;
	bool Should_RegisterAsset(const std::filesystem::path& path) const;
	void Rebuild_PathIndex();
	static bool EndsWith(const string& value, const string& suffix);
	static wstring Normalize_PathKey(const wstring& path);
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
	static Unique<Asset_Manager> Create(const wstring& resourceRoot);
	virtual void Free() override;
};

NS_END
