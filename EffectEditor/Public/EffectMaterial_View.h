#pragma once

#include "Editor_Window.h"
#include "EffectMaterialAuthoring_Types.h"

NS_BEGIN(EffectEditor)

class EffectMaterialPreviewRenderer;

class EffectMaterial_View final : public Editor_Window
{
public:
    EffectMaterial_View();
    ~EffectMaterial_View() override;

public:
    void Pre_Render() override;
    void Render() override;

public: //## Behavior::Selection
    void Open_Definition(const string& definitionName);
    void Open_PresetBrowser();
    void Open_Preset(const wstring& filePath);

public: //## Accessor::Preset
    // Required material slot이 현재 원본 viewer에서 선택된 preset을 장착할 때 사용한다.
    bool Has_SelectedPreset() const;
    // 원본 preset 값을 embedded instance로 복사하기 위한 read-only 접근자다.
    const EffectMaterialInstanceData& Get_SelectedPresetMaterial() const;
    // UI 안내용 원본 파일명이다. 저장 identity로 사용하지 않는다.
    string Get_SelectedMaterialFileName() const;

private: //## Types
    enum class ViewMode : uint8
    {
        Definition,
        Preset,
    };

private: //## Data
    ViewMode _viewMode{ ViewMode::Definition };
    string _definitionName{ "SpriteSymbolTranslucent" };
    vector<wstring> _presetPaths{};
    wstring _selectedPresetPath{};
    string _selectedPresetGuid{};
    string _selectedPresetType{};
    EffectMaterialInstanceData _selectedPresetMaterial{};
    string _statusMessage{};
    Unique<EffectMaterialPreviewRenderer> _previewRenderer{};

private: //## Helpers
    const EffectMaterialDefinitionData& Get_CurrentDefinition() const;
    void Render_DefinitionInfo(const EffectMaterialDefinitionData& definition);
    void Render_PresetInfo();
    void Render_PresetPreview();
    void Render_MaterialInstanceTable(const char* tableId, const EffectMaterialInstanceData& material) const;
    void Refresh_PresetList();
    bool Load_PresetFile(const wstring& filePath);

public:
    static Shared<EffectMaterial_View> Create();
};

NS_END
