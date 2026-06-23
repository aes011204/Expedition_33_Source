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
	/* missingMetaRoot가 비어 있지 않으면 force scan 중 누락 meta 자동 생성 범위를 해당 루트 하위로 제한한다. */
	HRESULT Initialize(const wstring& resourceRoot, bool forceScan = false, const wstring& missingMetaRoot = {});
	const AssetMeta* Find_ByGUID(const string& guid) const;
	vector<AssetMeta> Get_AssetsByType(const string& type) const;
	string Find_GUID(const wstring& filePath) const;
	wstring Resolve_Path(const string& guid) const;
	const wstring& Get_ResourceRoot() const;

	string  Register_Asset(const wstring& filePath, const string& type = "");
	/* missingMetaRoot가 비어 있지 않으면 기존 meta는 로드하되, 누락 meta 생성은 해당 루트 하위 파일만 허용한다. */
	void    Scan_And_Register(const wstring& directory, const wstring& missingMetaRoot = {});
    HRESULT Refresh_RegistryDirectory(const wstring& directory, const wstring& missingMetaRoot = {});
    /* background 파일 조사 결과를 main thread에서 live registry/cache에 반영한다. */
    HRESULT Commit_AssetDeltaScan(const AssetDeltaScanResult& scanResult, const wstring& missingMetaRoot = {});
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
	bool    Is_IgnoredPath(const fs::path& path) const;
	bool    Should_RegisterAsset(const fs::path& path) const;
	bool    Should_RegisterJsonAsset(const fs::path& path) const;

	void    Rebuild_PathIndex();
    void    Deduplicate_Assets_ByPath(const string& preferGuid = "");
    void    Unregister_AssetPath(const wstring& assetPath);

private:
	static wstring  Build_MetaPath(const wstring& assetPath);
	static bool     Is_PathUnderRoot(const fs::path& path, const fs::path& root);
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
    bool _assetRegistryDirty = false;

    Unique<MaterialBuilder> _materialBuilder{};

public:
	static Unique<Asset_Manager> Create(const wstring& resourceRoot, bool forceScan = false, const wstring& missingMetaRoot = {});
	virtual void Free() override;
};

NS_END
