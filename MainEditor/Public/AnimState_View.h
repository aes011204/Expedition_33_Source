#pragma once

#include "Editor_Window.h"
#include "Editor_Camera_Free.h"

NS_BEGIN(Engine)
class GameObject;
class ModelCom;
class RenderTarget;
NS_END

NS_BEGIN(MainEditor)

class AnimState_View final : public Editor_Window
{
public:
    AnimState_View();
    ~AnimState_View() override = default;

public:
    void Update(float timeDelta) override;
    void Pre_Render() override;
    void Render() override;

	bool CanSave() const override;
	void Save() override;

public:
    void Open_FromModelComponent(
        const Shared<ModelCom>& modelCom,
        const string& preferredAnimationName = "",
        uint32 preferredDockId = 0,
        bool focusOnOpen = true);

    bool Is_CompanionPreviewFocused();

public:


private:
    struct AnimationEntry
    {
        string name{};
        string displayName{};
        string playbackName{};
    };

private:
    void Refresh_AnimationList();

    void Draw_Header();
    void Draw_AnimationSelector();
    void Draw_PreviewPanel();
    void Draw_PlaybackToolbar();

    Shared<ModelCom> Get_PlaybackModel() const;
    Shared<GameObject> Get_PreviewRenderObject() const;

    void Ensure_PreviewRenderTarget(uint32 width, uint32 height);
    void Ensure_PreviewCamera();
    void Ensure_PreviewGridResources();
    void Fit_PreviewCameraToModel();
    void Draw_PreviewGrid() const;

    void Sync_PreviewAnimationSelection(bool keepCurrentTime = false);

private:
	void Apply_EnumFromProperty(const string &enumDisplayName, const vector<string> &names, const vector<int> &values);

	// Reflection 기반 Enum 검색 결과
	struct EnumSearchResult
	{
		string ownerClassName{};
		string propertyDisplayName{};
		vector<string> enumNames{};
		vector<int> enumValues{};
	};
	void Search_EnumProperties(const string &searchStr, vector<EnumSearchResult> &outResults) const;

	void Draw_EnumSettingPanel();
	void Draw_MappingTable();
	void Draw_MappingSaveLoadPanel();
	void Save_Mapping(const string &fileName);
	void Load_Mapping(const string &fileName);
    void Refresh_AnimmapFiles();

private:
    static string Make_AnimationComboLabel(const AnimationEntry& entry);
    static const AnimationEntry* Find_AnimationEntryByAnyName(const vector<AnimationEntry>& entries, const string& name);
    static int32 Find_AnimationIndex(const Shared<ModelCom>& modelCom, const string& animationName);
    static Shared<GameObject> Resolve_RootPreviewObject(const Shared<GameObject>& gameObject);
    static Editor_Camera_Free::EDITOR_CAMERA_DESC Build_PreviewCameraDesc(const Shared<GameObject>& gameObject);
    static void Apply_PreviewCameraTransform(const Shared<Editor_Camera_Free>& camera);

private:
    wstring _sourceModelPath{};
    Shared<ModelCom> _liveModelCom{};
    Shared<RenderTarget> _previewRenderTarget{};
    Shared<Editor_Camera_Free> _previewCamera{};
    Shared<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> _previewGridBatch{};
    Shared<DirectX::BasicEffect> _previewGridEffect{};
    ComPtr<ID3D11InputLayout> _previewGridInputLayout{};
    ComPtr<ID3D11DepthStencilState> _previewGridDepthState{};

    string _modelGuid{};
    string _currentAnimationName{};
    vector<AnimationEntry> _availableAnimations{};

    uint32 _pendingDockId = 0;
    bool _requestFocusOnOpen = false;

    bool _isPreviewHovered = false;
    bool _isPreviewActive = false;
    bool _showPreviewGrid = true;

    uint32 _previewWidth = 640;
    uint32 _previewHeight = 360;

private:
    string _enumTypeName{};
    vector<string> _stateNames{};
    vector<int32> _stateIndices{};
    map<string, string> _animationMapping{};
	char _mappingSaveLoadBuffer[MAX_PATH] = "";
	char _enumSearchBuffer[128] = "";
	float _topSectionHeight = 400.f;

    vector<string> _availableAnimmapFiles{};
    int32 _selectedAnimmapFileIndex = -1;

public:
    static Shared<AnimState_View> Create();
};

NS_END
