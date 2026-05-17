#pragma once
#include "EditorBlock.h"
#include <assimp/Importer.hpp>

NS_BEGIN(ModelEditor)

// 모호성 해결을 위한 타입 별칭
using HRESULT = ::HRESULT;
using LRESULT = ::LRESULT;
using HWND    = ::HWND;
using UINT    = ::UINT;

class ModelConverter final : public EditorBlock
{
public:
	ModelConverter();
	~ModelConverter() override;

public:
	static LRESULT CALLBACK DragDropWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

private:
	HRESULT Initialize() override;

public:
	void Update(float timeDelta) override;
	void Render() override;

public:
	void Scan_FbxFiles(const wstring& path);
	void Extract_Selected();
	void Extract_All();
	HRESULT Convert_Model(const wstring& fbxFilePath, const wstring& outputDir);

private:
	void BuildBoneHierarchy(const aiNode* node, int32 parentBoneIndex, Matrix accumulatedTransform);
	void ReadMeshes(const aiScene* aiScene);
	void ReadMaterial(const aiScene* aiScene);
	void ReadAnimations(const aiScene* aiScene);

	HRESULT Export_ModelData(const aiScene* aiScene, const wstring& outputDir, const string& modelName);
	HRESULT Export_AnimationData(const aiScene* aiScene, const wstring& outputDir, const string& modelName);
    HRESULT Export_ReverseAnimationData();
	
	HRESULT Export_JsonLog(const wstring& outputDir);

private:
	Matrix  Convert_Matrix(const aiMatrix4x4& aiMat) const;
	string  Get_FileNameWithoutExtension(const wstring& filePath) const;
	void    Ensure_DirectoryExists(const wstring& dirPath) const;
	string  Sanitize_FileName(string name);

private:
	void Clear_Converter();

private:
	HWND                                _hWnd{ nullptr };
	char                                _pathBuffer[512]{};
	wstring                             _resourcePath{};
	bool                                _exportJsonLog{ false };
	bool                                _exportModelData{ true };
	bool                                _exportReversedAnimation{ false };
	bool                                _applyStaticGlobalScale{ true };
	vector<std::filesystem::path>       _cachedFbxPaths;
	string                              _selectedFbxPath;

private:
	uint32                              _importFlags{ 0 };
	Assimp::Importer                    _importer;
	const aiScene*                      _aiScene{ nullptr };
	bool                                _isSkeletal{ false };
    
	vector<ModelMeshData>              _meshHeaders;
	vector<vector<VTXMESH>>            _staticVertices;
	vector<vector<VTXANIMMESH>>        _animVertices;
	vector<vector<uint32>>             _indices;

	vector<vector<uint32>>             _meshBoneRefs;
	vector<vector<Matrix>>             _meshOffsets;

	vector<ModelMaterialData>          _matHeaders;
	vector<vector<ModelTextureSlot>>   _matSlots;
	vector<ModelBoneData>              _bones;
	unordered_map<string, uint32>      _boneNameMap;

	vector<ModelAnimationData>         _animHeaders;

	vector<vector<ModelChannelData>>   _channelHeaders;
	vector<vector<vector<KEYFRAME>>>   _keyframes;

public:
	static Shared<ModelConverter> Create(HWND hWnd);

};

NS_END
