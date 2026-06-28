#pragma once

#include "TrailAnchor_Asset.h"
#include "Editor_Camera_Free.h"
#include "Editor_Window.h"

NS_BEGIN(Engine)
class GameObject;
class ModelCom;
class RenderTarget;
NS_END

NS_BEGIN(Client)
class SkyBoxFlow;
class SkyCloudCard;
class Weapon;
struct WeaponSourceHistoryAnchorDesc;
NS_END

NS_BEGIN(MainEditor)

class Prefab_View : public Editor_Window
{
public:
    enum class SelectionType { None, GameObject, Component, };

public:
    Prefab_View();
    virtual ~Prefab_View() = default;

public:
    void Update(float timeDelta) override;
    void Pre_Render() override;
    void Render() override;

    bool CanSave() const override;
    void Save() override;

public:
    void Open_Prefab(const string& prefabName, const fs::path& prefabPath);
    void Close_Prefab();

    const string& Get_PrefabName() const { return _prefabName; }
    bool Is_PreviewPrefab(const string& prefabName) const;
    uint32 Get_DockId() const { return _dockId; }


private:
    struct FMonsterBattlePrefabEntry
    {
        string prefabName{};
        string className{};
    };

private:
    void Handle_GizmoShortcut();
    void Draw_Header();
    void Draw_StructurePanel();
    void Draw_PreviewPanel();
    void Draw_InspectorPanel();
    void Render_SelectedObjectGizmo();
    void Render_GizmoOverlay();
    bool Draw_PlayerBattleWeaponSelector(const Shared<GameObject>& rootObject);
    void Draw_SelectedWeaponTrailAnchorPanel(const Shared<GameObject>& selectedObject);
    bool Draw_MonsterBattlePrefabList(const Shared<GameObject>& selectedObject);
    bool Draw_MonsterBattleSettings(const Shared<GameObject>& selectedObject);
    void Draw_GameObjectTree(const Shared<GameObject>& gameObject, const wstring& objectKey);
    void Draw_ComponentEntries(const Shared<GameObject>& gameObject, const wstring& objectKey);

    Shared<GameObject> Find_GameObjectByKey(const Shared<GameObject>& rootObject, const wstring& objectKey) const;

private:
    void Ensure_PreviewRenderTarget(uint32 width, uint32 height);
    void Ensure_PreviewGridResources();
    void Ensure_PreviewSkyBackdrop();
    void Update_PreviewSkyBackdrop(float timeDelta);
    void Late_Update_PreviewSkyBackdrop(float timeDelta);

    void Reset_PreviewCameraPose();
    void Lock_PreviewObjectToOrigin();

    void Draw_PreviewColliders(const Shared<GameObject>& gameObject) const;
    void Draw_ParkourProbePreview() const;
    void Draw_PreviewGrid() const;
    void Draw_MonsterBattleSpawnPreview() const;
    void Draw_SelectedWeaponTrailAnchorPreview() const;
    void Draw_SelectedWeaponSourceAnchorPreview() const;

    Shared<ModelCom> Find_FirstModelComponent() const;
    void Prepare_AnimationViewSiblingDock();

    void Open_PendingAnimationView();
    void Open_PendingAnimStateView();
    uint32 Save_PlayerBattleWeaponTransforms() const;
    void Load_SelectedWeaponTrailAnchor(const string& modelGuid, const Shared<ModelCom>& modelCom);
    void Save_SelectedWeaponTrailAnchor() const;
    void Draw_SelectedWeaponSourceAnchorPanel(const Shared<GameObject>& weaponObject, const Shared<ModelCom>& modelCom);
    bool Try_GetSelectedWeaponModel(const Shared<GameObject>& selectedObject, Shared<ModelCom>& outModel, string& outModelGuid) const;
    void Rebuild_MonsterBattlePrefabEntries();
    bool Try_ResolveTrailAnchorWorldPosition(
        const Shared<GameObject>& selectedObject,
        const Client::TrailAnchorPointDesc& anchor,
        Vec3& outWorldPosition) const;
    bool Try_ResolveSourceAnchorWorldPosition(
        const Shared<GameObject>& weaponObject,
        const Client::WeaponSourceHistoryAnchorDesc& anchor,
        Vec3& outWorldPosition) const;

private:
    static Editor_Camera_Free::EDITOR_CAMERA_DESC Build_PreviewCameraDesc(const Shared<GameObject>& gameObject);
    static void Apply_PreviewCameraTransform(const Shared<Editor_Camera_Free>& camera);
    static void Disable_PreviewAnimNotifyExecution(const Shared<GameObject>& gameObject);
    static bool Should_DeferToCompanionPreview();

private:
    string              _prefabName {};
    fs::path            _prefabPath{};
    wstring             _defaultSpawnLayerTag = TEXT("Layer_GameObject");
    char                _defaultSpawnLayerTagBuffer[128] = "Layer_GameObject";

    // 프리펩이 어떤 클래스로 만들어졌는지 구별용
    string              _sourceStaticClass{};

    Shared<GameObject>  _previewObject{};
    Shared<RenderTarget> _previewRenderTarget{};
    Shared<Editor_Camera_Free> _previewCamera{};
    Shared<Client::SkyBoxFlow> _previewSkyBox{};
    vector<Shared<Client::SkyCloudCard>> _previewSkyCloudCards{};
    Shared<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> _previewGridBatch{};
    Shared<DirectX::BasicEffect> _previewGridEffect{};
    ComPtr<ID3D11InputLayout> _previewGridInputLayout{};
    ComPtr<ID3D11DepthStencilState> _previewGridDepthState{};
    ComPtr<ID3D11DepthStencilState> _previewOverlayDepthState{};

    bool                _isPreviewHovered = false;
    bool                _isPreviewActive = false;
    bool                _isGizmoHovered = false;
    bool                _isGizmoUsing = false;
    bool                _applyDefaultWindowLayout = false;
    bool                _openAnimationViewWithPrefab = false;

    bool                _pendingOpenAnimationView = false;
    uint32              _dockId = 0;

    bool                _pendingOpenAnimStateView = false;
    uint32              _pendingAnimStateDockId = 0;

    uint32              _pendingDockId = 0;
    uint32              _pendingAnimationDockId = 0;
    int32               _focusPrefabFrames = 0;

    uint32              _previewWidth = 512;
    uint32              _previewHeight = 512;
    bool                _previewHasBegunPlay = false;
    bool                _previewSkyBackdropBegunPlay = false;
    bool                _requestResetPreviewCamera = false;
    ImVec2              _previewCanvasMin = {};
    ImVec2              _previewCanvasMax = {};
    ImGuizmo::OPERATION _gizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE      _gizmoMode = ImGuizmo::LOCAL;

    SelectionType       _selectionType = SelectionType::None;
    bool                _showPrefabCollider = false;
    bool                _showPreviewGrid = true;
    bool                _showPreviewSkyBackdrop = true;

    wstring _selectedObjectKey{};
    wstring _selectedComponentKey{};
    string _selectedWeaponTrailModelGuid{};
    Client::TrailAnchorAsset _selectedWeaponTrailAnchor{};
    vector<FMonsterBattlePrefabEntry> _monsterBattlePrefabEntries{};

public:
    static Shared<Prefab_View> Create();

};

NS_END
