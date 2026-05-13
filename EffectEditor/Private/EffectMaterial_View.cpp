#include "EffectMaterial_View.h"

#include <fstream>
#include "EffectEditorInstance.h"
#include "GameInstance.h"
#include "Helper_String.h"
#include "RenderTarget.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "VIBufferCom_PointParticle.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    constexpr uint32 kPreviewTextureSize = 512;
    constexpr const char* kFallbackTextureId = "Texture_Effect_DefaultTexture";
    constexpr const wchar_t* kEffectSpriteShaderId = L"Shader_EffectSprite";
    constexpr const wchar_t* kPointParticleBufferId = L"Com_VIBuffer_PointParticle";
    constexpr int kOpacitySourceAlpha = 0;
    constexpr int kOpacitySourceRed = 1;
    constexpr int kOpacitySourceLuminance = 2;

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

    int Resolve_OpacitySourceIndex(const string& opacitySource)
    {
        if (opacitySource == "Alpha" || opacitySource == "alpha")
            return kOpacitySourceAlpha;

        if (opacitySource == "Red" || opacitySource == "red")
            return kOpacitySourceRed;

        if (opacitySource == "Luminance" || opacitySource == "luminance")
            return kOpacitySourceLuminance;

        return kOpacitySourceAlpha;
    }

    template <typename T>
    Shared<T> Clone_Component(uint32 levelIndex, const wstring& prototypeTag, void* arg = nullptr)
    {
        if (GAME == nullptr)
            return nullptr;

        return dynamic_pointer_cast<T>(
            GAME->Clone_Prototype(Prototype::Comopnent, levelIndex, prototypeTag, arg)
        );
    }
}

class EffectMaterialPreviewRenderer final
{
public:
    void Render_Material(const EffectMaterialInstanceData& material)
    {
        _statusMessage.clear();

        if (EDITOR == nullptr || GAME == nullptr)
        {
            _statusMessage = "Preview renderer is not ready.";
            return;
        }

        if (!Ready_RenderTarget() ||
            !Ready_Shader() ||
            !Ready_Texture(material.mainTextureId) ||
            !Ready_NoiseTexture(material.noiseTextureId) ||
            !Ready_Buffer())
            return;

        ID3D11ShaderResourceView* nullSrvs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
        EDITOR->Get_Context()->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSrvs);

        if (FAILED(_renderTarget->Begin(true, true)))
        {
            _statusMessage = "Preview render target begin failed.";
            return;
        }

        const HRESULT hr = Render_Sprite(material);
        GAME->Bind_BackBuffer();

        if (FAILED(hr))
            _statusMessage = "Preview draw failed.";
    }

    ID3D11ShaderResourceView* Get_SRV() const
    {
        return _renderTarget == nullptr ? nullptr : _renderTarget->Get_SRV();
    }

    const string& Get_StatusMessage() const
    {
        return _statusMessage;
    }

    void Handle_ViewportInput(bool isHovered)
    {
        if (!isHovered)
            return;

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            const ImVec2 delta = ImGui::GetIO().MouseDelta;
            _yawRadians += delta.x * 0.01f;
            _pitchRadians = clamp(_pitchRadians + delta.y * 0.01f, -1.2f, 1.2f);
        }

        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.f)
            _distance = clamp(_distance - wheel * 0.25f, 1.5f, 8.f);
    }

private:
    Shared<RenderTarget> _renderTarget{};
    Shared<ShaderCom> _shader{};
    Shared<Texture> _texture{};
    Shared<Texture> _noiseTexture{};
    Shared<VIBufferCom_PointParticle> _buffer{};
    string _loadedTextureId{};
    string _loadedNoiseTextureId{};
    string _statusMessage{};
    float _yawRadians{ 0.f };
    float _pitchRadians{ 0.15f };
    float _distance{ 3.5f };

private:
    bool Ready_RenderTarget()
    {
        if (_renderTarget != nullptr)
            return true;

        _renderTarget = RenderTarget::Create(
            EDITOR->Get_Device(),
            EDITOR->Get_Context(),
            Internal::kPreviewTextureSize,
            Internal::kPreviewTextureSize,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            Vec4{ 0.03f, 0.035f, 0.045f, 1.f },
            true
        );

        if (_renderTarget == nullptr)
        {
            _statusMessage = "Preview render target creation failed.";
            return false;
        }

        return true;
    }

    bool Ready_Shader()
    {
        if (_shader != nullptr)
            return true;

        _shader = Internal::Clone_Component<ShaderCom>(ETOI(LevelType::Static), Internal::kEffectSpriteShaderId);
        if (_shader == nullptr)
        {
            _statusMessage = "Shader_EffectSprite prototype is missing.";
            return false;
        }

        return true;
    }

    bool Ready_Texture(const string& textureId)
    {
        const string requestedTextureId = textureId.empty() ? Internal::kFallbackTextureId : textureId;
        if (_texture != nullptr && _loadedTextureId == requestedTextureId)
            return true;

        _texture = Internal::Clone_Component<Texture>(ETOI(LevelType::GamePlay), String::ToWString(requestedTextureId));
        _loadedTextureId = requestedTextureId;

        if (_texture != nullptr)
            return true;

        if (requestedTextureId == Internal::kFallbackTextureId)
        {
            _statusMessage = "Preview texture prototype is missing.";
            return false;
        }

        _texture = Internal::Clone_Component<Texture>(ETOI(LevelType::GamePlay), String::ToWString(Internal::kFallbackTextureId));
        _loadedTextureId = Internal::kFallbackTextureId;

        if (_texture == nullptr)
        {
            _statusMessage = "Preview texture and fallback texture are missing.";
            return false;
        }

        _statusMessage = "Preview texture missing. Fallback texture is shown.";
        return true;
    }

    bool Ready_NoiseTexture(const string& noiseTextureId)
    {
        if (noiseTextureId.empty())
        {
            _noiseTexture = nullptr;
            _loadedNoiseTextureId.clear();
            return true;
        }

        if (_noiseTexture != nullptr && _loadedNoiseTextureId == noiseTextureId)
            return true;

        _noiseTexture = Internal::Clone_Component<Texture>(ETOI(LevelType::GamePlay), String::ToWString(noiseTextureId));
        _loadedNoiseTextureId = noiseTextureId;

        if (_noiseTexture == nullptr)
        {
            _loadedNoiseTextureId.clear();
            _statusMessage = "Noise texture missing. Preview continues without noise.";
        }

        return true;
    }

    bool Ready_Buffer()
    {
        if (_buffer != nullptr)
            return true;

        PointParticleDesc desc{};
        desc.instanceCount = 1;
        desc.center = Vec3{ 0.f, 0.f, 0.f };
        desc.range = Vec3{ 0.f, 0.f, 0.f };
        desc.scale = Vec2{ 2.f, 2.f };
        desc.pivot = Vec3{ 0.f, 0.f, 0.f };
        desc.speed = Vec2{ 0.f, 0.f };
        desc.lifeTime = Vec2{ 100.f, 100.f };
        desc.loop = true;
        desc.drawMode = PointParticleDrawMode::Direct;

        _buffer = Internal::Clone_Component<VIBufferCom_PointParticle>(
            ETOI(LevelType::Static),
            Internal::kPointParticleBufferId,
            &desc
        );

        if (_buffer == nullptr)
        {
            _statusMessage = "Point particle preview buffer prototype is missing.";
            return false;
        }

        return true;
    }

    HRESULT Render_Sprite(const EffectMaterialInstanceData& material) const
    {
        const float cosPitch = cosf(_pitchRadians);
        const Vec3 eye{
            sinf(_yawRadians) * cosPitch * _distance,
            sinf(_pitchRadians) * _distance,
            -cosf(_yawRadians) * cosPitch * _distance
        };

        const Matrix viewMatrix = XMMatrixLookAtLH(
            XMVectorSet(eye.x, eye.y, eye.z, 1.f),
            XMVectorSet(0.f, 0.f, 0.f, 1.f),
            XMVectorSet(0.f, 1.f, 0.f, 0.f)
        );
        const Matrix projMatrix = XMMatrixPerspectiveFovLH(
            XMConvertToRadians(35.f),
            1.f,
            0.1f,
            100.f
        );

        const Vec4 billboardTarget{ eye.x, eye.y, eye.z, 1.f };
        const Vec4 spriteParams{
            material.intensity,
            max(material.opacityPower, 0.0001f),
            material.noiseStrength,
            _noiseTexture != nullptr ? 1.f : 0.f
        };
        const int opacitySource = Internal::Resolve_OpacitySourceIndex(material.opacitySource);

        CHECK_FAILED(_shader->Bind_Matrix("g_ViewMatrix", &viewMatrix), E_FAIL);
        CHECK_FAILED(_shader->Bind_Matrix("g_ProjMatrix", &projMatrix), E_FAIL);
        CHECK_FAILED(_shader->Bind_RawValue("g_vBillboardTarget", &billboardTarget, sizeof(billboardTarget)), E_FAIL);
        CHECK_FAILED(_texture->Bind_ShaderResourceView(_shader.get(), "g_Texture", 0), E_FAIL);

        if (_noiseTexture != nullptr)
            CHECK_FAILED(_noiseTexture->Bind_ShaderResourceView(_shader.get(), "g_NoiseTexture", 0), E_FAIL);

        CHECK_FAILED(_shader->Bind_RawValue("g_vTint", &material.tint, sizeof(material.tint)), E_FAIL);
        CHECK_FAILED(_shader->Bind_RawValue("g_vEffectSpriteParams", &spriteParams, sizeof(spriteParams)), E_FAIL);
        CHECK_FAILED(_shader->Bind_RawValue("g_iOpacitySource", &opacitySource, sizeof(opacitySource)), E_FAIL);
        CHECK_FAILED(_shader->Begin(0), E_FAIL);
        CHECK_FAILED(_buffer->Bind_Resources(), E_FAIL);
        CHECK_FAILED(_buffer->Render(), E_FAIL);

        return S_OK;
    }
};

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
