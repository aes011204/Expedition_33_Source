#include "EffectMaterial_View.h"

#include <fstream>
#include "Helper_ReadPreset.h"
#include "EffectMaterialPreviewRenderer.h"
#include "GameInstance.h"
#include "Helper_String.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    constexpr const wchar_t* kPreviewFallbackTextureId = L"Texture_Effect_DefaultTexture";

    bool Is_SupportedBlendMode(const string& blendMode)
    {
        return
            blendMode == "AlphaBlend" ||
            blendMode == "alphaBlend" ||
            blendMode == "alphablend" ||
            blendMode == "Additive" ||
            blendMode == "additive";
    }

    bool Is_SupportedOpacitySource(const string& opacitySource)
    {
        return
            opacitySource == "Alpha" ||
            opacitySource == "alpha" ||
            opacitySource == "Red" ||
            opacitySource == "red" ||
            opacitySource == "Luminance" ||
            opacitySource == "luminance";
    }

    bool Should_WarnInvalidSubUv(const json& root, const char* key)
    {
        const auto iter = root.find(key);
        if (iter == root.end())
            return false;

        if (!iter->is_number_unsigned())
            return true;

        return iter->get<uint32>() == 0;
    }

    bool Can_ResolvePreviewFallbackTexture()
    {
        if (GAME == nullptr)
            return false;

        return GAME->Clone_Prototype(
            Prototype::Comopnent,
            ETOI(LevelType::GamePlay),
            kPreviewFallbackTextureId
        ) != nullptr;
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

    if (_selectedPresetPath.empty() || _previewRenderer == nullptr)
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
        Render_PresetInfo();
    }

    Set_Open(isOpen);
    ImGui::End();
}

void EffectMaterial_View::Open_PresetBrowser()
{
    _statusMessage.clear();
    Refresh_PresetList();

    if (_selectedPresetPath.empty() && !_presetPaths.empty())
        Load_PresetFile(_presetPaths.front());

    Set_Open(true);
}

void EffectMaterial_View::Open_Preset(const wstring& filePath)
{
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

void EffectMaterial_View::Render_PresetInfo()
{
    ImGui::TextUnformatted("원본 머티리얼");
    ImGui::Separator();

    if (_selectedPresetPath.empty())
    {
        if (!_statusMessage.empty())
            ImGui::TextDisabled("%s", _statusMessage.c_str());
        else
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
    if (_previewRenderer == nullptr)
        return;

    _previewRenderer->Draw_Viewport(
        "EffectMaterialOriginalPreview",
        _selectedPresetMaterial,
        "원본 프리뷰"
    );
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
        LOG_ERROR("EffectMaterial preset load failed: file does not exist. path='{}'", String::ToString(filePath));
        return false;
    }

    ifstream file(filePath);
    if (!file.is_open())
    {
        _statusMessage = "Failed to open preset file.";
        LOG_ERROR("EffectMaterial preset load failed: open failed. path='{}'", String::ToString(filePath));
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
        LOG_ERROR("EffectMaterial preset load failed: json parse failed. path='{}'", String::ToString(filePath));
        return false;
    }

    EffectMaterialInstanceData material{};
    if (!ReadPreset::Read(filePath, material))
    {
        _statusMessage = "Failed to read preset material.";
        LOG_ERROR("EffectMaterial preset load failed: material read failed. path='{}'", String::ToString(filePath));
        return false;
    }
    const bool warnSubUvRows = Internal::Should_WarnInvalidSubUv(root, "subUvRows");
    const bool warnSubUvCols = Internal::Should_WarnInvalidSubUv(root, "subUvCols");

    const string presetPath = String::ToString(filePath);
    if (warnSubUvRows)
        LOG_WARN("EffectMaterial preset corrected invalid subUvRows to 1. path='{}'", presetPath);
    if (warnSubUvCols)
        LOG_WARN("EffectMaterial preset corrected invalid subUvCols to 1. path='{}'", presetPath);

    if (!Internal::Is_SupportedBlendMode(material.blendMode))
    {
        _statusMessage = "Unsupported blendMode in preset.";
        LOG_ERROR(
            "EffectMaterial preset load failed: unsupported blendMode='{}'. path='{}'",
            material.blendMode,
            presetPath
        );
        return false;
    }

    if (!Internal::Is_SupportedOpacitySource(material.opacitySource))
    {
        _statusMessage = "Unsupported opacitySource in preset.";
        LOG_ERROR(
            "EffectMaterial preset load failed: unsupported opacitySource='{}'. path='{}'",
            material.opacitySource,
            presetPath
        );
        return false;
    }

    if (material.mainTextureId.empty() && !Internal::Can_ResolvePreviewFallbackTexture())
    {
        _statusMessage = "Main texture is missing and preview fallback texture is unavailable.";
        LOG_ERROR(
            "EffectMaterial preset load failed: mainTextureId is empty and preview fallback texture is unavailable. path='{}'",
            presetPath
        );
        return false;
    }

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
