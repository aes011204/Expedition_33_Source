#include "EffectMaterial_View.h"

#include <fstream>
#include "EffectMaterialPreviewRenderer.h"
#include "GameInstance.h"
#include "Helper_String.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    string Read_String(const json& root, const char* key, const string& defaultValue)
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_string())
            return defaultValue;

        return iter->get<string>();
    }

    float Read_Float(const json& root, const char* key, float defaultValue)
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_number())
            return defaultValue;

        return iter->get<float>();
    }

    uint32 Read_Uint(const json& root, const char* key, uint32 defaultValue)
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_number_unsigned())
            return defaultValue;

        return iter->get<uint32>();
    }

    Color Read_Color(const json& root, const char* key, const Color& defaultValue)
    {
        const auto iter = root.find(key);
        if (iter == root.end())
            return defaultValue;

        if (iter->is_object())
        {
            return Color(
                iter->value("r", defaultValue.x),
                iter->value("g", defaultValue.y),
                iter->value("b", defaultValue.z),
                iter->value("a", defaultValue.w)
            );
        }

        if (iter->is_array() && iter->size() >= 4)
        {
            return Color(
                iter->at(0).is_number() ? iter->at(0).get<float>() : defaultValue.x,
                iter->at(1).is_number() ? iter->at(1).get<float>() : defaultValue.y,
                iter->at(2).is_number() ? iter->at(2).get<float>() : defaultValue.z,
                iter->at(3).is_number() ? iter->at(3).get<float>() : defaultValue.w
            );
        }

        return defaultValue;
    }

    string Resolve_PresetDisplayName(const wstring& filePath)
    {
        string fileName = String::ToString(fs::path(filePath).filename().wstring());
        constexpr const char* suffix = ".effectmaterial.json";
        if (fileName.ends_with(suffix))
            fileName.erase(fileName.size() - strlen(suffix));

        return fileName;
    }
}

EffectMaterial_View::EffectMaterial_View()
    : Editor_Window{ L"Effect Material", ICON_FA_CIRCLE_INFO }
    , _previewRenderer{ make_unique<EffectMaterialPreviewRenderer>() }
{
}

EffectMaterial_View::~EffectMaterial_View() = default;

void EffectMaterial_View::Pre_Render()
{
    if (!Is_Open())
        return;

    if (_viewMode != ViewMode::Preset || _selectedPresetPath.empty() || _previewRenderer == nullptr)
        return;

    _previewRenderer->Render_Material(_selectedPresetMaterial);
}

void EffectMaterial_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string& windowName = Get_ImGuiWindowName();

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        if (_viewMode == ViewMode::Preset)
            Render_PresetInfo();
        else
            Render_DefinitionInfo(Get_CurrentDefinition());
    }

    Set_Open(isOpen);
    ImGui::End();
}

void EffectMaterial_View::Open_Definition(const string& definitionName)
{
    _viewMode = ViewMode::Definition;
    _definitionName = definitionName;
    _statusMessage.clear();
    Set_Open(true);
}

void EffectMaterial_View::Open_PresetBrowser()
{
    _viewMode = ViewMode::Preset;
    _statusMessage.clear();
    Refresh_PresetList();

    if (_selectedPresetPath.empty() && !_presetPaths.empty())
        Load_PresetFile(_presetPaths.front());

    Set_Open(true);
}

void EffectMaterial_View::Open_Preset(const wstring& filePath)
{
    _viewMode = ViewMode::Preset;
    _statusMessage.clear();
    Refresh_PresetList();
    Load_PresetFile(filePath);
    Set_Open(true);
}

bool EffectMaterial_View::Has_SelectedPreset() const
{
    return !_selectedPresetPath.empty();
}

const EffectMaterialInstanceData& EffectMaterial_View::Get_SelectedPresetMaterial() const
{
    return _selectedPresetMaterial;
}

string EffectMaterial_View::Get_SelectedMaterialFileName() const
{
    if (_selectedPresetPath.empty())
        return {};

    return String::ToString(fs::path(_selectedPresetPath).filename().wstring());
}

const EffectMaterialDefinitionData& EffectMaterial_View::Get_CurrentDefinition() const
{
    return Resolve_EffectMaterialDefinition(_definitionName);
}

void EffectMaterial_View::Render_DefinitionInfo(const EffectMaterialDefinitionData& definition)
{
    ImGui::TextUnformatted("Effect Material Definition");
    ImGui::Separator();

    if (ImGui::BeginTable("EffectMaterialDefinitionInfo", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 0.35f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65f);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Definition");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(definition.name.c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Renderer Family");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(definition.rendererFamily.c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Default Texture");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(definition.defaultInstance.mainTextureId.c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Blend / Opacity");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%s / %s", definition.defaultInstance.blendMode.c_str(), definition.defaultInstance.opacitySource.c_str());

        ImGui::EndTable();
    }
}

void EffectMaterial_View::Render_PresetInfo()
{
    ImGui::TextUnformatted("원본 머티리얼");
    ImGui::Separator();

    if (_selectedPresetPath.empty())
    {
        ImGui::TextDisabled("선택된 원본 preset이 없습니다.");
        return;
    }

    const float contentWidth = ImGui::GetContentRegionAvail().x;
    const float previewWidth = contentWidth < 560.f
                               ? contentWidth
                               : clamp(contentWidth * 0.48f, 360.f, 560.f);

    ImGui::BeginChild("EffectMaterialPreviewPane", ImVec2(previewWidth, 0.f), false);
    {
        Render_PresetPreview();
    }
    ImGui::EndChild();

    if (contentWidth >= 560.f)
        ImGui::SameLine();

    ImGui::BeginChild("EffectMaterialPresetDetail", ImVec2(0.f, 0.f), false);
    {
        ImGui::TextDisabled("Read-only original preset. Required slot에서 장착합니다.");

        if (!_statusMessage.empty())
            ImGui::TextDisabled("%s", _statusMessage.c_str());

        if (ImGui::BeginTable("EffectMaterialPresetMeta", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 0.28f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.72f);

            const string fileName = String::ToString(fs::path(_selectedPresetPath).filename().wstring());
            const string fullPath = String::ToString(_selectedPresetPath);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("원본 파일");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(fileName.c_str());

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("경로");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s", fullPath.c_str());

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("GUID");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(_selectedPresetGuid.empty() ? "(pending)" : _selectedPresetGuid.c_str());

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("Type");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(_selectedPresetType.empty() ? "EffectMaterial" : _selectedPresetType.c_str());

            ImGui::EndTable();
        }

        ImGui::Separator();
        Render_MaterialInstanceTable("EffectMaterialPresetInstance", _selectedPresetMaterial);
    }
    ImGui::EndChild();
}

void EffectMaterial_View::Render_PresetPreview()
{
    ImGui::TextUnformatted("Plane Preview");

    const float previewHeight = max(280.f, ImGui::GetContentRegionAvail().y);
    ImGui::BeginChild("EffectMaterialOriginalPreview", ImVec2(0.f, previewHeight), true, ImGuiWindowFlags_NoScrollbar);
    {
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        canvasSize.x = max(1.f, canvasSize.x);
        canvasSize.y = max(1.f, canvasSize.y);

        const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
        const ImVec2 canvasMax = ImVec2(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(10, 12, 16, 255));
        drawList->AddRect(canvasMin, canvasMax, IM_COL32(70, 78, 92, 255));

        ImGui::InvisibleButton("EffectMaterialPreviewCanvas", canvasSize);
        const bool isHovered = ImGui::IsItemHovered();

        ID3D11ShaderResourceView* previewSrv =
            _previewRenderer == nullptr ? nullptr : _previewRenderer->Get_SRV();

        if (previewSrv != nullptr)
        {
            const float imageSize = min(canvasSize.x, canvasSize.y);
            const ImVec2 imageMin = ImVec2(
                canvasMin.x + (canvasSize.x - imageSize) * 0.5f,
                canvasMin.y + (canvasSize.y - imageSize) * 0.5f
            );
            const ImVec2 imageMax = ImVec2(imageMin.x + imageSize, imageMin.y + imageSize);
            const ImTextureID textureId = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(previewSrv));
            drawList->AddImage(textureId, imageMin, imageMax);
        }
        else
        {
            const char* message = "Preview render target is not ready.";
            if (_previewRenderer != nullptr && !_previewRenderer->Get_StatusMessage().empty())
                message = _previewRenderer->Get_StatusMessage().c_str();

            drawList->AddText(
                ImVec2(canvasMin.x + 12.f, canvasMin.y + 12.f),
                IM_COL32(214, 220, 230, 255),
                message
            );
        }

        drawList->AddText(
            ImVec2(canvasMin.x + 12.f, canvasMax.y - 24.f),
            IM_COL32(180, 190, 205, 255),
            "LMB drag: orbit / Wheel: zoom"
        );

        if (_previewRenderer != nullptr)
            _previewRenderer->Handle_ViewportInput(isHovered);
    }
    ImGui::EndChild();
}

void EffectMaterial_View::Render_MaterialInstanceTable(const char* tableId, const EffectMaterialInstanceData& material) const
{
    if (!ImGui::BeginTable(tableId, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
        return;

    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 0.35f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65f);

    const auto drawRow = [](const char* label, const string& value)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("%s", label);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(value.c_str());
    };

    drawRow("Definition", material.definitionName);
    drawRow("Main Texture", material.mainTextureId);
    drawRow("Noise Texture", material.noiseTextureId.empty() ? string{} : material.noiseTextureId);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("Tint");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("(%.2f, %.2f, %.2f, %.2f)", material.tint.x, material.tint.y, material.tint.z, material.tint.w);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("Intensity");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%.2f", material.intensity);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("Opacity Power");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%.2f", material.opacityPower);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("Noise Strength");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%.2f", material.noiseStrength);

    drawRow("Blend Mode", material.blendMode);
    drawRow("Opacity Source", material.opacitySource);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("SubUV");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%u x %u", material.subUvRows, material.subUvCols);

    ImGui::EndTable();
}

void EffectMaterial_View::Refresh_PresetList()
{
    _presetPaths.clear();

    if (GAME == nullptr)
        return;

    const vector<AssetMeta> presetAssets = GAME->Get_AssetsByType("EffectMaterial");
    for (const AssetMeta& assetMeta : presetAssets)
        _presetPaths.push_back(assetMeta.fullPath);
}

bool EffectMaterial_View::Load_PresetFile(const wstring& filePath)
{
    _statusMessage.clear();
    _selectedPresetPath.clear();
    _selectedPresetGuid.clear();
    _selectedPresetType.clear();
    _selectedPresetMaterial = EffectMaterialInstanceData{};

    if (!fs::exists(filePath))
    {
        _statusMessage = "Preset file does not exist.";
        return false;
    }

    ifstream file(filePath);
    if (!file.is_open())
    {
        _statusMessage = "Failed to open preset file.";
        return false;
    }

    json root{};
    try
    {
        file >> root;
    }
    catch (...)
    {
        _statusMessage = "Failed to parse preset json.";
        return false;
    }

    const EffectMaterialInstanceData defaultMaterial = Resolve_EffectMaterialDefinition("SpriteSymbolTranslucent").defaultInstance;
    EffectMaterialInstanceData material = defaultMaterial;
    material.definitionName = Internal::Read_String(root, "definitionName", defaultMaterial.definitionName);
    material.sourcePresetName = Internal::Resolve_PresetDisplayName(filePath);
    material.mainTextureId = Internal::Read_String(root, "mainTextureId", defaultMaterial.mainTextureId);
    material.noiseTextureId = Internal::Read_String(root, "noiseTextureId", defaultMaterial.noiseTextureId);
    material.tint = Internal::Read_Color(root, "tint", defaultMaterial.tint);
    material.intensity = Internal::Read_Float(root, "intensity", defaultMaterial.intensity);
    material.opacityPower = Internal::Read_Float(root, "opacityPower", defaultMaterial.opacityPower);
    material.noiseStrength = Internal::Read_Float(root, "noiseStrength", defaultMaterial.noiseStrength);
    material.blendMode = Internal::Read_String(root, "blendMode", defaultMaterial.blendMode);
    material.opacitySource = Internal::Read_String(root, "opacitySource", defaultMaterial.opacitySource);
    material.subUvRows = max(1u, Internal::Read_Uint(root, "subUvRows", defaultMaterial.subUvRows));
    material.subUvCols = max(1u, Internal::Read_Uint(root, "subUvCols", defaultMaterial.subUvCols));

    _selectedPresetPath = filePath;
    _selectedPresetMaterial = material;
    _selectedPresetGuid = GAME ? GAME->Find_AssetGUID(filePath) : string{};
    _selectedPresetType = "EffectMaterial";

    if (!_selectedPresetGuid.empty())
    {
        if (const AssetMeta* assetMeta = GAME->Find_AssetByGUID(_selectedPresetGuid))
            _selectedPresetType = assetMeta->type;
    }

    return true;
}

Shared<EffectMaterial_View> EffectMaterial_View::Create()
{
    return make_shared<EffectMaterial_View>();
}

NS_END
