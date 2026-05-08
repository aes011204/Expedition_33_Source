#pragma once
#include "EditorBlock.h"

NS_BEGIN(ModelEditor)

class ModelInspector final : public EditorBlock
{
public:
	ModelInspector();
	~ModelInspector() override = default;

private:
	HRESULT Initialize() override;

public:
	void Update(float timeDelta) override;
	void Render() override;

private:
	void Scan_ModelFiles(const wstring& resourcePath);
	void Scan_AnimFiles(const wstring& modelDirectory);
	
	void Render_ModelList();
	void Render_AnimList();
	void Render_TransformControl();

	void Load_SelectedAsset(const string& extension);

private:
	vector<std::filesystem::path> _cachedModelPaths;
	vector<std::filesystem::path> _cachedAnimPaths;

	string _selectedModelPath;
	string _selectedAnimPath;

	char _pathBuffer[512]{};
	wstring _resourcePath{};

private:
    int32   _selectedAnimIndex{ -1 };
    bool    _isLoop{ true };
    int32   _rootNodeIndex{ -1 };
    bool    _isRootMotion{ false };
    
    string _selectedModelGuid = {};

public:
	static Shared<ModelInspector> Create();
};

NS_END
