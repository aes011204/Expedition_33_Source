#pragma once
#include "Base.h"

NS_BEGIN(Engine)

class MaterialBuilder;

class ENGINE_DLL Asset_Manager final : public Base
{
private:
	Asset_Manager();
public:
	~Asset_Manager() override;

public:
	HRESULT Initialize(const wstring& resourceRoot);
	const AssetMeta* Find_ByGUID(const string& guid) const;
	vector<AssetMeta> Get_AssetsByType(const string& type) const;
	string Find_GUID(const wstring& filePath) const;
	wstring Resolve_Path(const string& guid) const;
	const wstring& Get_ResourceRoot() const;

	string  Register_Asset(const wstring& filePath, const string& type = "");
	void    Scan_And_Register(const wstring& directory);
	void    Refresh_Cache();
    uint32  Clear_OrphanMeta(const wstring& directory);

public: /* MaterialBuilder */
    const MaterialBuilder* Get_MaterialBuilder() const { return _materialBuilder.get(); }
    MaterialBuilder*       Get_MaterialBuilder() { return _materialBuilder.get(); }

private:
	void Load_Registry();
	void Save_Registry() const;

    bool Load_Meta(const wstring& metaPath);
    bool Save_Meta(const wstring& metaPath, const AssetMeta& assetMeta) const;

	void Load_Cache();
	void Save_Cache() const;
	void Save_AssetList(const wstring& path) const;
	void Load_AssetList(const wstring& path);

	string  Detect_AssetType(const wstring& filePath) const;
	bool    Should_RegisterAsset(const fs::path& path) const;
	bool    Should_RegisterJsonAsset(const fs::path& path) const;

	void    Rebuild_PathIndex();
    void    Deduplicate_Assets_ByPath(const string& preferGuid = "");
    void    Unregister_AssetPath(const wstring& assetPath);

private:
    static wstring  Build_MetaPath(const wstring& assetPath);
	static bool     EndsWith(const string& value, const string& suffix);
	static wstring  Normalize_PathKey(const wstring& path);
	static string   Generate_GUID();

private:
	wstring                             _resourceRoot = {};
	wstring                             _assetDataFolder = {};
	wstring                             _registryPath = {};
	wstring                             _cachePath = {};

	umap<string, AssetMeta>             _guidToMeta = {};
	umap<wstring, string>               _pathToGuid = {};

    vector<string> _ignoreFolders = { ".git", ".vs", "Fonts", "Data/Assets" };

    Unique<MaterialBuilder> _materialBuilder{};

public:
	static Unique<Asset_Manager> Create(const wstring& resourceRoot);
	virtual void Free() override;
};

NS_END
