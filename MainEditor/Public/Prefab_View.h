#pragma once

#include "Editor_Window.h"

NS_BEGIN(Engine)
class GameObject;
class RenderTarget;
NS_END

NS_BEGIN(MainEditor)

class Editor_Camera_Free;

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


private:
    void Draw_Header();
    void Draw_StructurePanel();
    void Draw_PreviewPanel();
    void Draw_InspectorPanel();

    void Draw_GameObjectEntry();
    void Draw_ComponentEntries();

private:
    void Ensure_PreviewRenderTarget(uint32 width, uint32 height);
    void Ensure_PreviewGridResources();
    void Reset_PreviewCameraPose();
    void Lock_PreviewObjectToOrigin();
    void Draw_PreviewGrid() const;

private:
    string              _prefabName {};
    fs::path            _prefabPath{};

    // 프리펩이 어떤 클래스로 만들어졌는지 구별용
    string              _sourceStaticClass{};

    Shared<GameObject>  _previewObject{};
    Shared<RenderTarget> _previewRenderTarget{};
    Shared<Editor_Camera_Free> _previewCamera{};
    Shared<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> _previewGridBatch{};
    Shared<DirectX::BasicEffect> _previewGridEffect{};
    ComPtr<ID3D11InputLayout> _previewGridInputLayout{};
    ComPtr<ID3D11DepthStencilState> _previewGridDepthState{};

    bool                _isPreviewHovered = false;
    bool                _isPreviewActive = false;
    bool                _applyDefaultWindowLayout = false;

    uint32              _previewWidth = 512;
    uint32              _previewHeight = 512;
    bool                _previewHasBegunPlay = false;

    SelectionType       _selectionType = SelectionType::None;
    wstring             _selectedComponentTag{};

    bool                _showPrefabCollider = false;
    bool                _showPreviewGrid = true;


public:
    static Shared<Prefab_View> Create();

};

NS_END
