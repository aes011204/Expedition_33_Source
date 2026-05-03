#pragma once
#include "EditorBlock.h"

NS_BEGIN(Assimp)
class Importer;
NS_END

NS_BEGIN(ModelEditor)

using HRESULT = ::HRESULT;
using LRESULT = ::LRESULT;
using HWND = ::HWND;
using UINT = ::UINT;

class ModelConverter final : public EditorBlock
{
public:
	ModelConverter();
	~ModelConverter() override;

public:
	static ::LRESULT CALLBACK DragDropWndProc(::HWND hWnd, UINT message, ::WPARAM wParam, ::LPARAM lParam, ::UINT_PTR uIdSubclass, ::DWORD_PTR dwRefData);

private:
	HRESULT Initialize(HWND hWnd);

public:
	void Update(float timeDelta) override;
	void Render() override;

public:
	void ExtractAll(const wstring& path);
	HRESULT Convert_Model(const wstring& fbxFilePath, const wstring& outputDir);

private:
	void ReadNodes(const aiNode* node, int32 parentIndex = -1);
	void ReadMeshes(const aiScene* aiScene);
	void ReadMaterial(const aiScene* aiScene);
	void ReadAnimations(const aiScene* aiScene);

	HRESULT Export_ModelData(const aiScene* aiScene, const wstring& outputDir, const string& modelName);
	HRESULT Export_AnimationData(const aiScene* aiScene, const wstring& outputDir, const string& modelName);

private:
	Matrix  Convert_Matrix(const aiMatrix4x4& aiMat) const;
	string  Get_FileNameWithoutExtension(const wstring& filePath) const;
	void    Ensure_DirectoryExists(const wstring& dirPath) const;
	int32   FindNodeIndex(const char* name);
	string  Sanitize_FileName(string name);

private:
	void Clear_Converter();

private:
	::HWND                              _hWnd{ nullptr };
	char                                _pathBuffer[512]{};
	wstring                             _resourcePath{};

private:
	uint32                              importFlags{ 0 };
	const aiScene*                      _aiScene{ nullptr };
	Assimp::Importer                    _importer{};

	bool                                _isSkeletal{ false };

	vector<ModelNodeData>              _nodes;

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
