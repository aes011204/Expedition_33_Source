#include "MeshDataPreview_View.h"

#include "EffectEditorInstance.h"
#include "EffectMaterialInstance_View.h"
#include "EffectMaterialPresetReader.h"
#include "EffectMaterialScalarModulationPreview.h"
#include "EffectPreviewOrbitCamera.h"
#include "GameInstance.h"
#include "Helper_EffectAuthoring.h"
#include "Helper_Math.h"
#include "ModelCom.h"
#include "RenderTarget.h"
#include "ShaderCom.h"
#include "Texture.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    constexpr auto kWindowName = L"MeshData Preview";
    constexpr auto kMeshShaderId = L"Shader_EffectMeshPreview";
    constexpr auto kMeshDistortionShaderId = L"Shader_EffectMeshPreviewDistortion";
    constexpr auto kEffectMaterialPayloadType = "CONTENT_BROWSER_EFFECT_MATERIAL";
    constexpr auto kEffectModelPayloadType = "CONTENT_BROWSER_EFFECT_MODEL";
    constexpr auto kModelDialogDefaultFolder = L"Effects\\Models";
    constexpr auto kMaterialDialogDefaultFolder = L"Effects\\Materials";

    constexpr float kMeshPreviewHomeDistance{ 6.f };
    constexpr float kMeshPreviewHomeYawDegrees{ 0.f };
    constexpr float kMeshPreviewHomePitchDegrees{ 30.f };
    constexpr float kMeshPreviewHomeTransitionSpeed{ 10.f };
    constexpr float kMeshPreviewOrbitSensor{ 0.105f };
    constexpr int kOpacitySourceAlpha{ 0 };
    constexpr int kOpacitySourceRed{ 1 };
    constexpr int kOpacitySourceLuminance{ 2 };

    template <typename T>
    Shared<T> Clone_Component(uint32 levelIndex, const wstring& prototypeTag, void* arg = nullptr)
    {
        if (nullptr == GAME)
            return nullptr;

        return dynamic_pointer_cast<T>(
            GAME->Clone_Prototype(Prototype::Comopnent, levelIndex, prototypeTag, arg)
        );
    }

    string To_DisplayPath(const fs::path& path)
    {
        return path.empty() ? string{} : String::ToString(path.lexically_normal().wstring());
    }

    string Make_AssetRelativePath(const fs::path& filePath)
    {
        if (GAME == nullptr)
            return String::ToString(filePath.lexically_normal().generic_wstring());

        const fs::path assetRoot = fs::path(GAME->Get_AssetRoot()).lexically_normal();
        const fs::path normalizedPath = filePath.lexically_normal();
        const fs::path relativePath = normalizedPath.lexically_relative(assetRoot);
        if (!relativePath.empty() && relativePath.native().find(L"..") != 0)
            return String::ToString(relativePath.generic_wstring());

        return String::ToString(normalizedPath.wstring());
    }

    fs::path Resolve_AssetPath(const string& guid, const string& path)
    {
        if (nullptr == GAME)
            return fs::path(String::ToWString(path));

        if (!guid.empty())
        {
            const wstring resolvedPath = GAME->Resolve_AssetPath(guid);
            if (!resolvedPath.empty())
                return fs::path(resolvedPath).lexically_normal();
        }

        fs::path resolvedPath = fs::path(String::ToWString(path));
        if (resolvedPath.is_relative())
            resolvedPath = fs::path(GAME->Get_AssetRoot()) / resolvedPath;

        return resolvedPath.lexically_normal();
    }

    bool Try_PickAssetDialog(
        const wchar_t* title,
        const wchar_t* filterName,
        const wchar_t* filterSpec,
        const wchar_t* defaultFolder,
        wstring& outFilePath)
    {
        outFilePath.clear();

        const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool shouldUninit = SUCCEEDED(initHr);

        IFileOpenDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || nullptr == dialog)
        {
            if (shouldUninit)
                CoUninitialize();
            return false;
        }

        dialog->SetTitle(title);

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST);

        if (nullptr != GAME && nullptr != defaultFolder)
        {
            const fs::path folderPath = fs::path(GAME->Get_AssetRoot()) / defaultFolder;
            IShellItem* folderItem = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(folderPath.lexically_normal().wstring().c_str(), nullptr, IID_PPV_ARGS(&folderItem))) &&
                folderItem != nullptr)
            {
                dialog->SetFolder(folderItem);
                folderItem->Release();
            }
        }

        const COMDLG_FILTERSPEC filters[] = {
            { filterName, filterSpec },
            { L"All", L"*.*" },
        };
        dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);

        hr = dialog->Show(nullptr);
        if (SUCCEEDED(hr))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr)
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr)
                {
                    outFilePath = path;
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }

        dialog->Release();
        if (shouldUninit)
            CoUninitialize();

        return !outFilePath.empty();
    }

    bool Apply_ModelFileToMeshData(const wchar_t* payloadPath, MeshTypeData& data)
    {
        if (nullptr == payloadPath || nullptr == GAME)
            return false;

        const fs::path filePath = fs::path(payloadPath).lexically_normal();
        if (String::ToLowerCopy(filePath.extension().string()) != ".model")
            return false;

        data.modelGuid = GAME->Ensure_AssetGUID(filePath.wstring(), "Model");
        data.modelPath = Make_AssetRelativePath(filePath);
        return true;
    }

    bool Apply_EffectMaterialFileToMeshData(const wchar_t* payloadPath, MeshTypeData& data)
    {
        if (nullptr == payloadPath || nullptr == GAME)
            return false;

        const fs::path filePath = fs::path(payloadPath).lexically_normal();
        if (String::ToLowerCopy(filePath.extension().string()) != ".json")
            return false;

        data.assignedEffectMaterialGuid = GAME->Ensure_AssetGUID(filePath.wstring(), "EffectMaterial");
        data.assignedEffectMaterialPath = Make_AssetRelativePath(filePath);
        data.hasAssignedMaterialInstance = EffectMaterialPresetReader::Read(filePath, data.assignedMaterial);
        if (!data.hasAssignedMaterialInstance)
            data.assignedMaterial = {};
        return true;
    }

    bool Resolve_EffectiveMaterial(const MeshTypeData& meshData, EffectMaterialInstanceData& outMaterial)
    {
        if (meshData.hasAssignedMaterialInstance)
        {
            outMaterial = meshData.assignedMaterial;
            return true;
        }

        const fs::path materialPath = Resolve_AssetPath(
            meshData.assignedEffectMaterialGuid,
            meshData.assignedEffectMaterialPath
        );
        return !materialPath.empty() && EffectMaterialPresetReader::Read(materialPath, outMaterial);
    }

    void Apply_RequiredRenderPolicy(const RequiredModuleData* required, EffectMaterialInstanceData& material)
    {
        if (required == nullptr)
            return;

        material.blendMode = required->material.blendMode;
        material.alphaCutoff = required->material.alphaCutoff;
    }

    static int Resolve_MaterialSourceIndex(const string& source)
    {
        if (source == "Red" || source == "red")
            return kOpacitySourceRed;

        if (source == "Luminance" || source == "luminance")
            return kOpacitySourceLuminance;

        return kOpacitySourceAlpha;
    }

    bool Is_MeshDistortionPreviewFamily(const EffectMaterialInstanceData& material)
    {
        return material.materialFamily == EffectMaterialFamily::SpriteDistortion;
    }

    void Draw_CheckerRect(ImDrawList& drawList, const ImVec2& minPos, const ImVec2& maxPos)
    {
        constexpr float cellSize = 54.f;
        const ImU32 darkColor = IM_COL32(55, 64, 78, 255);
        const ImU32 lightColor = IM_COL32(105, 120, 140, 255);

        for (float y = minPos.y; y < maxPos.y; y += cellSize)
        {
            for (float x = minPos.x; x < maxPos.x; x += cellSize)
            {
                const int32 xi = static_cast<int32>((x - minPos.x) / cellSize);
                const int32 yi = static_cast<int32>((y - minPos.y) / cellSize);
                const ImU32 color = (xi + yi) % 2 == 0 ? lightColor : darkColor;
                drawList.AddRectFilled(
                    ImVec2(x, y),
                    ImVec2(min(x + cellSize, maxPos.x), min(y + cellSize, maxPos.y)),
                    color
                );
            }
        }

        drawList.AddRect(minPos, maxPos, IM_COL32(84, 96, 116, 255));
    }

    bool Draw_AssetDropSlot(
        const char* id,
        const char* title,
        const char* payloadType,
        const string& displayPath,
        const char* placeholder,
        const char* summary,
        const function<bool(const wchar_t*)>& onDrop,
        const function<bool()>& onPick)
    {
        constexpr float slotHeight = 46.f;
        constexpr float browseButtonWidth = 34.f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float slotWidth = max(160.f, ImGui::GetContentRegionAvail().x - browseButtonWidth - spacing);
        bool changed = false;

        ImGui::PushID(id);
        ImGui::InvisibleButton("DropSlot", ImVec2(slotWidth, slotHeight));

        const ImGuiPayload* activePayload = ImGui::GetDragDropPayload();
        const bool isMatchingPayload = activePayload != nullptr && activePayload->IsDataType(payloadType);
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payloadType, ImGuiDragDropFlags_AcceptBeforeDelivery))
            {
                if (payload->IsDelivery())
                    changed = onDrop(static_cast<const wchar_t*>(payload->Data));
            }
            ImGui::EndDragDropTarget();
        }

        const ImVec2 slotMin = ImGui::GetItemRectMin();
        const ImVec2 slotMax = ImGui::GetItemRectMax();
        const bool hovered = ImGui::IsItemHovered();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(
            slotMin,
            slotMax,
            hovered || isMatchingPayload ? IM_COL32(34, 42, 58, 255) : IM_COL32(30, 34, 42, 255),
            6.f
        );
        drawList->AddRect(
            slotMin,
            slotMax,
            hovered || isMatchingPayload ? IM_COL32(116, 156, 235, 255) : IM_COL32(78, 88, 108, 255),
            6.f,
            0,
            isMatchingPayload ? 2.f : 1.f
        );

        const float textLeft = slotMin.x + 10.f;
        drawList->PushClipRect(ImVec2(textLeft, slotMin.y), ImVec2(slotMax.x - 8.f, slotMax.y), true);
        drawList->AddText(ImVec2(textLeft, slotMin.y + 6.f), IM_COL32(235, 238, 245, 255), title);
        drawList->AddText(
            ImVec2(textLeft, slotMin.y + 25.f),
            IM_COL32(150, 160, 176, 255),
            displayPath.empty() ? placeholder : displayPath.c_str()
        );
        drawList->PopClipRect();

        if (hovered)
            ImGui::SetTooltip("%s\n%s", summary, displayPath.empty() ? placeholder : displayPath.c_str());

        ImGui::SameLine(0.f, spacing);
        if (ImGui::Button(ICON_FA_FOLDER_OPEN, ImVec2(browseButtonWidth, slotHeight)))
            changed = onPick();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("파일 선택");

        ImGui::PopID();
        return changed;
    }
}

struct MeshDataPreview_View::PreviewRenderer
{
    uint32 width{ 512 };
    uint32 height{ 512 };
    Shared<RenderTarget> renderTarget{};
    Shared<ModelCom> model{};
    Shared<ShaderCom> shader{};
    Shared<Texture> mainTexture{};
    Shared<Texture> noiseTexture{};
    Shared<Texture> maskTexture{};
    Shared<Texture> flowTexture{};
    string loadedModelKey{};
    wstring loadedShaderKey{};
    string loadedMainTextureKey{};
    string loadedNoiseTextureKey{};
    string loadedMaskTextureKey{};
    string loadedFlowTextureKey{};
    string statusMessage{};
    bool hasRendered{ false };
    EffectPreviewOrbitCameraState cameraState{};
    float materialElapsedTime{};
    bool isViewportDragging{ false };
    bool hasPendingHome{ false };

    PreviewRenderer()
    {
        Reset_View();
    }

    ~PreviewRenderer()
    {
        End_ViewportDrag();
    }

    void Render_Model(const AuthoringEmitter& emitter, const MeshTypeData& meshData, const fs::path& modelPath)
    {
        statusMessage.clear();
        hasRendered = false;

        if (EDITOR == nullptr || GAME == nullptr)
        {
            statusMessage = "Preview renderer is not ready.";
            return;
        }

        if (modelPath.empty() || !fs::exists(modelPath))
        {
            statusMessage = "Model asset is missing.";
            return;
        }

        EffectMaterialInstanceData effectMaterial{};
        if (!Internal::Resolve_EffectiveMaterial(meshData, effectMaterial))
        {
            statusMessage = "Assigned effect material is missing.";
            return;
        }

        if (const MaterialScalarModulationModuleData* modulation = Find_MaterialScalarModulationModuleData(emitter))
        {
            const RequiredModuleData* required = Find_RequiredModuleData(emitter);
            const float previewDuration = required != nullptr ? required->duration : 1.f;
            const MaterialScalarModulationPreviewResult previewMaterial =
                Evaluate_MaterialScalarModulationPreview(
                    effectMaterial,
                    Build_CoreColorRgbModulationRuntimeDesc(*modulation),
                    Build_MaterialScalarModulationRuntimeDesc(*modulation),
                    Compute_MaterialScalarPreviewPhase(materialElapsedTime, previewDuration),
                    false
                );
            effectMaterial = previewMaterial.material;
        }

        Internal::Apply_RequiredRenderPolicy(Find_RequiredModuleData(emitter), effectMaterial);

        if (!Ready_RenderTarget() || !Ready_Shader(effectMaterial) || !Ready_Model(modelPath))
            return;

        if (!Ready_EffectMaterialTextures(effectMaterial))
            return;

        ShaderResourceView* nullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
        EDITOR->Get_Context()->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);

        if (FAILED(renderTarget->Begin(true, true)))
        {
            statusMessage = "Preview render target begin has failed.";
            return;
        }

        const HRESULT hr = Draw_Model(meshData.previewScale, effectMaterial);
        GAME->Bind_BackBuffer();

        if (FAILED(hr))
        {
            statusMessage = "Model preview draw failed.";
            return;
        }

        hasRendered = true;
    }

    void Draw_Viewport(const char* id, const AuthoringEmitter& emitter, const MeshTypeData& meshData, const fs::path& modelPath)
    {
        const ImVec2 availableSize = ImGui::GetContentRegionAvail();
        const ImVec2 canvasSize{ max(1.f, availableSize.x), max(260.f, availableSize.y) };

        ImGui::BeginChild(id, canvasSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            ImVec2 innerSize = ImGui::GetContentRegionAvail();
            innerSize.x = max(1.f, innerSize.x);
            innerSize.y = max(1.f, innerSize.y);
            Resize_RenderTarget(static_cast<uint32>(innerSize.x), static_cast<uint32>(innerSize.y));
            materialElapsedTime += max(ImGui::GetIO().DeltaTime, 0.f);
            Render_Model(emitter, meshData, modelPath);

            const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
            const ImVec2 canvasMax{ canvasMin.x + innerSize.x, canvasMin.y + innerSize.y };
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            Internal::Draw_CheckerRect(*drawList, canvasMin, canvasMax);

            ImGui::InvisibleButton("MeshDataPreviewCanvas", innerSize);
            const bool isHovered = ImGui::IsItemHovered();

            if (ShaderResourceView* srv = Get_SRV())
            {
                const ImTextureID textureId = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(srv));
                drawList->AddImage(textureId, canvasMin, canvasMax);
            }
            else
            {
                const char* message = statusMessage.empty() ? "Model preview is not ready." : statusMessage.c_str();
                drawList->AddText(ImVec2(canvasMin.x + 12.f, canvasMin.y + 34.f), IM_COL32(245, 198, 120, 255), message);
            }

            Draw_ViewportTextOverlay(canvasMin, canvasMax, "MeshData material preview");
            Handle_ViewportInput(isHovered, canvasMin, canvasMax);
        }
        ImGui::EndChild();
    }

    ShaderResourceView* Get_SRV() const
    {
        return hasRendered && renderTarget != nullptr ? renderTarget->Get_SRV() : nullptr;
    }

    bool Ready_RenderTarget()
    {
        if (renderTarget != nullptr)
            return true;

        renderTarget = RenderTarget::Create(
            L"MeshDataPreviewRenderTarget",
            EDITOR->Get_Device(),
            EDITOR->Get_Context(),
            width,
            height,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            Vec4{ 0.f, 0.f, 0.f, 0.f },
            true
        );

        if (renderTarget == nullptr)
        {
            statusMessage = "Preview render target creation failed.";
            return false;
        }

        return true;
    }

    bool Ready_Shader(const EffectMaterialInstanceData& material)
    {
        const wstring requestedShaderId = Internal::Is_MeshDistortionPreviewFamily(material)
                                          ? Internal::kMeshDistortionShaderId
                                          : Internal::kMeshShaderId;
        if (shader != nullptr && loadedShaderKey == requestedShaderId)
            return true;

        shader.reset();
        loadedShaderKey.clear();

        shader = Internal::Clone_Component<ShaderCom>(ETOI(LevelType::Static), requestedShaderId);
        if (shader == nullptr)
        {
            statusMessage = requestedShaderId == Internal::kMeshDistortionShaderId
                            ? "Shader_EffectMeshPreviewDistortion prototype is missing."
                            : "Shader_EffectMeshPreview prototype is missing.";
            return false;
        }

        loadedShaderKey = requestedShaderId;
        return true;
    }

    bool Ready_Model(const fs::path& modelPath)
    {
        const string requestedKey = String::ToString(modelPath.lexically_normal().wstring());
        if (model != nullptr && loadedModelKey == requestedKey)
            return true;

        model.reset();
        loadedModelKey.clear();

        model = ModelCom::Create(
            EDITOR->Get_Device(),
            EDITOR->Get_Context(),
            ModelType::NonAnim,
            requestedKey.c_str(),
            Matrix::CreateScale(0.01f)
        );
        if (model == nullptr)
        {
            statusMessage = "Model load failed.";
            return false;
        }

        loadedModelKey = requestedKey;
        Reset_ViewToModel();
        return true;
    }

    bool Ready_Texture(
        const string& textureGuid,
        const string& texturePath,
        bool required,
        Shared<Texture>& outTexture,
        string& loadedTextureKey,
        const char* missingMessage)
    {
        if (textureGuid.empty() && texturePath.empty())
        {
            outTexture.reset();
            loadedTextureKey.clear();
            if (required)
                statusMessage = missingMessage;
            return !required;
        }

        const fs::path texturePathResolved = Internal::Resolve_AssetPath(textureGuid, texturePath);
        if (texturePathResolved.empty() || !fs::exists(texturePathResolved))
        {
            outTexture.reset();
            loadedTextureKey.clear();
            statusMessage = missingMessage;
            return !required;
        }

        const string requestedKey = String::ToString(texturePathResolved.lexically_normal().wstring());
        if (outTexture != nullptr && loadedTextureKey == requestedKey)
            return true;

        outTexture = Texture::Create(EDITOR->Get_Device(), EDITOR->Get_Context(), texturePathResolved.wstring().c_str(), 1);
        if (outTexture == nullptr)
        {
            loadedTextureKey.clear();
            statusMessage = missingMessage;
            return !required;
        }

        loadedTextureKey = requestedKey;
        return true;
    }

    bool Ready_EffectMaterialTextures(const EffectMaterialInstanceData& material)
    {
        if (Internal::Is_MeshDistortionPreviewFamily(material))
        {
            mainTexture.reset();
            noiseTexture.reset();
            maskTexture.reset();
            loadedMainTextureKey.clear();
            loadedNoiseTextureKey.clear();
            loadedMaskTextureKey.clear();

            Ready_Texture(
                material.flowTextureGuid,
                material.flowTexturePath,
                false,
                flowTexture,
                loadedFlowTextureKey,
                "Flow texture missing. Distortion preview continues without offset."
            );
            return true;
        }

        flowTexture.reset();
        loadedFlowTextureKey.clear();

        if (!Ready_Texture(
            material.mainTextureGuid,
            material.mainTexturePath,
            true,
            mainTexture,
            loadedMainTextureKey,
            "Assigned effect material main texture is missing."
        ))
            return false;

        Ready_Texture(
            material.noiseTextureGuid,
            material.noiseTexturePath,
            false,
            noiseTexture,
            loadedNoiseTextureKey,
            "Noise texture missing. Preview continues without noise."
        );

        Ready_Texture(
            material.maskTextureGuid,
            material.maskTexturePath,
            false,
            maskTexture,
            loadedMaskTextureKey,
            "Mask texture missing. Preview continues without mask."
        );

        return true;
    }

    HRESULT Bind_DistortionPreviewResources(const EffectMaterialInstanceData& material)
    {
        CHECK_NULL(shader, E_FAIL);

        if (flowTexture != nullptr)
            CHECK_FAILED(flowTexture->Bind_ShaderResourceView(shader.get(), "g_FlowTexture", 0), E_FAIL);
        else
            CHECK_FAILED(shader->Bind_SRV("g_FlowTexture", nullptr), E_FAIL);

        const Vec4 distortionParams(
            material.refractionIntensity,
            max(0.f, material.refractionPresence),
            flowTexture != nullptr ? 1.f : 0.f,
            materialElapsedTime
        );
        const Vec4 screenSize(static_cast<float>(max(1u, width)), static_cast<float>(max(1u, height)), 0.f, 0.f);
        const float flowUVTilingMode = static_cast<float>(static_cast<uint32>(material.flowUVTilingMode));
        const Vec2 flowUVPolicyParams(
            static_cast<float>(static_cast<uint32>(material.flowUVPolicy.uPolicy)),
            static_cast<float>(static_cast<uint32>(material.flowUVPolicy.vPolicy))
        );

        CHECK_FAILED(shader->Bind_RawValue("g_Tint", &material.tint, sizeof(material.tint)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectDistortionParams", &distortionParams, sizeof(distortionParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_DistortionScreenSize", &screenSize, sizeof(screenSize)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_FlowUVPolicyParams", &flowUVPolicyParams, sizeof(flowUVPolicyParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_FlowUVScale", &material.flowUVScale, sizeof(material.flowUVScale)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_FlowUVOffset", &material.flowUVOffset, sizeof(material.flowUVOffset)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_FlowUVScrollSpeed", &material.flowUVScrollSpeed, sizeof(material.flowUVScrollSpeed)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_FlowUVTilingMode", &flowUVTilingMode, sizeof(flowUVTilingMode)), E_FAIL);
        return S_OK;
    }

    HRESULT Bind_EffectMaterialResources(const EffectMaterialInstanceData& material)
    {
        CHECK_NULL(shader, E_FAIL);
        CHECK_NULL(mainTexture, E_FAIL);

        CHECK_FAILED(mainTexture->Bind_ShaderResourceView(shader.get(), "g_Texture", 0), E_FAIL);

        if (noiseTexture != nullptr)
            CHECK_FAILED(noiseTexture->Bind_ShaderResourceView(shader.get(), "g_NoiseTexture", 0), E_FAIL);
        else
            CHECK_FAILED(shader->Bind_SRV("g_NoiseTexture", nullptr), E_FAIL);

        if (maskTexture != nullptr)
            CHECK_FAILED(maskTexture->Bind_ShaderResourceView(shader.get(), "g_MaskTexture", 0), E_FAIL);
        else
            CHECK_FAILED(shader->Bind_SRV("g_MaskTexture", nullptr), E_FAIL);

        CHECK_FAILED(shader->Bind_RawValue("g_Tint", &material.tint, sizeof(material.tint)), E_FAIL);

        const Vec4 effectMeshParams = Vec4(
            material.intensity,
            material.opacityPower,
            material.noiseStrength,
            noiseTexture != nullptr ? 1.f : 0.f
        );
        const Vec4 effectMeshAlphaParams = Vec4(
            clamp(material.alphaCutoff, 0.f, 1.f),
            clamp(material.alphaErosion, 0.f, 1.f),
            maskTexture != nullptr ? 1.f : 0.f,
            0.f
        );
        const Vec4 effectMeshMainUVParams = Vec4(material.mainUVScale.x, material.mainUVScale.y, material.mainUVScrollSpeed.x, material.mainUVScrollSpeed.y);
        const Vec4 effectMeshNoiseUVParams = Vec4(
            material.noiseUVScale.x,
            material.noiseUVScale.y,
            material.noiseUVScrollSpeed.x,
            material.noiseUVScrollSpeed.y
        );
        const Vec4 effectMeshMaskUVParams = Vec4(material.maskUVScale.x, material.maskUVScale.y, material.maskUVScrollSpeed.x, material.maskUVScrollSpeed.y);
        const Vec4 effectMeshUVOffsetParams = Vec4(material.mainUVOffset.x, material.mainUVOffset.y, material.noiseUVOffset.x, material.noiseUVOffset.y);
        const Vec4 effectMeshMaskUVOffsetParams = Vec4(material.maskUVOffset.x, material.maskUVOffset.y, 0.f, 0.f);
        const Vec4 effectMeshUVModeParams = Vec4(
            static_cast<float>(static_cast<uint32>(material.mainUVTilingMode)),
            static_cast<float>(static_cast<uint32>(material.noiseUVTilingMode)),
            static_cast<float>(static_cast<uint32>(material.maskUVTilingMode)),
            0.f
        );
        const EffectMaterialAdditiveContributionData& additive = material.additive;
        const EffectMaterialCoreEmissiveData& coreEmissive = material.coreEmissive;
        const Vec4 effectMeshAdditiveParams = Vec4(
            static_cast<float>(static_cast<uint32>(additive.colorSource)),
            static_cast<float>(static_cast<uint32>(additive.amountSource)),
            static_cast<float>(static_cast<uint32>(additive.coveragePolicy)),
            additive.intensityScale
        );
        const Vec4 effectMeshAdditiveFlags = Vec4(additive.blackNeutral ? 1.f : 0.f, 0.f, 0.f, 0.f);
        const Vec4 effectMeshCoreEmissiveParams = Vec4(
            coreEmissive.enabled ? 1.f : 0.f,
            coreEmissive.corePower,
            coreEmissive.coreIntensity,
            coreEmissive.outerPower
        );
        const Vec4 effectMeshCoreEmissiveColor = Vec4(
            coreEmissive.coreColor.x,
            coreEmissive.coreColor.y,
            coreEmissive.coreColor.z,
            coreEmissive.outerIntensity
        );
        const int opacitySource = Internal::Resolve_MaterialSourceIndex(material.opacitySource);
        const Vec4 effectMeshSourceParams = Vec4(
            static_cast<float>(Internal::Resolve_MaterialSourceIndex(material.noiseSource)),
            static_cast<float>(Internal::Resolve_MaterialSourceIndex(material.maskSource)),
            material.noiseInvert ? 1.f : 0.f,
            material.maskInvert ? 1.f : 0.f
        );

        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshParams", &effectMeshParams, sizeof(effectMeshParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshAlphaParams", &effectMeshAlphaParams, sizeof(effectMeshAlphaParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshMainUVParams", &effectMeshMainUVParams, sizeof(effectMeshMainUVParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshNoiseUVParams", &effectMeshNoiseUVParams, sizeof(effectMeshNoiseUVParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshMaskUVParams", &effectMeshMaskUVParams, sizeof(effectMeshMaskUVParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshUVOffsetParams", &effectMeshUVOffsetParams, sizeof(effectMeshUVOffsetParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshMaskUVOffsetParams", &effectMeshMaskUVOffsetParams, sizeof(effectMeshMaskUVOffsetParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshUVModeParams", &effectMeshUVModeParams, sizeof(effectMeshUVModeParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshSourceParams", &effectMeshSourceParams, sizeof(effectMeshSourceParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshMaterialTime", &materialElapsedTime, sizeof(materialElapsedTime)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshAdditiveParams", &effectMeshAdditiveParams, sizeof(effectMeshAdditiveParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshAdditiveEmissiveColor", &additive.emissiveColor, sizeof(additive.emissiveColor)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshAdditiveConstantColor", &additive.constantColor, sizeof(additive.constantColor)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshAdditiveFlags", &effectMeshAdditiveFlags, sizeof(effectMeshAdditiveFlags)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshCoreEmissiveParams", &effectMeshCoreEmissiveParams, sizeof(effectMeshCoreEmissiveParams)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_EffectMeshCoreEmissiveColor", &effectMeshCoreEmissiveColor, sizeof(effectMeshCoreEmissiveColor)), E_FAIL);
        CHECK_FAILED(shader->Bind_RawValue("g_OpacitySource", &opacitySource, sizeof(opacitySource)), E_FAIL);
        return S_OK;
    }

    uint32 Resolve_ShaderPassIndex(const EffectMaterialInstanceData& material) const
    {
        if (Internal::Is_MeshDistortionPreviewFamily(material))
            return 0u;

        const bool additive = material.blendMode == EffectMaterialBlendMode::Additive;
        const bool twoSided = material.twoSided;
        if (additive)
            return twoSided ? 3u : 1u;
        return twoSided ? 2u : 0u;
    }

    HRESULT Draw_Model(const Vec3& previewScale, const EffectMaterialInstanceData& material)
    {
        CHECK_NULL(shader, E_FAIL);
        CHECK_NULL(model, E_FAIL);

        const float aspect = static_cast<float>(width) / static_cast<float>(max(1u, height));
        const EffectPreviewOrbitCameraFrame cameraFrame = EffectPreviewOrbitCamera::Build_Frame(
            cameraState,
            aspect,
            XMConvertToRadians(35.f),
            0.1f,
            100.f
        );
        const Matrix world = Matrix::CreateScale(
            max(0.0001f, previewScale.x),
            max(0.0001f, previewScale.y),
            max(0.0001f, previewScale.z)
        );

        CameraCB cameraCB{};
        cameraCB.viewMatrix = cameraFrame.view;
        cameraCB.projMatrix = cameraFrame.proj;
        cameraCB.viewInverseMatrix = XMMatrixInverse(nullptr, cameraFrame.view);
        cameraCB.projInverseMatrix = XMMatrixInverse(nullptr, cameraFrame.proj);
        cameraCB.cameraPosition = Vec4(cameraFrame.eye.x, cameraFrame.eye.y, cameraFrame.eye.z, 1.f);
        cameraCB.farPlane = 100.f;

        CHECK_FAILED(shader->Bind_ObjectCB(world), E_FAIL);
        CHECK_FAILED(shader->Bind_CBufferData(cameraCB), E_FAIL);
        if (Internal::Is_MeshDistortionPreviewFamily(material))
            CHECK_FAILED(Bind_DistortionPreviewResources(material), E_FAIL);
        else
            CHECK_FAILED(Bind_EffectMaterialResources(material), E_FAIL);

        const uint32 passIndex = Resolve_ShaderPassIndex(material);
        const uint32 numMeshes = static_cast<uint32>(model->Get_NumMeshes());
        for (uint32 meshIndex = 0; meshIndex < numMeshes; ++meshIndex)
        {
            CHECK_FAILED(shader->Begin(passIndex), E_FAIL);
            CHECK_FAILED(model->Render(meshIndex), E_FAIL);
        }

        return S_OK;
    }

    void Resize_RenderTarget(uint32 requestedWidth, uint32 requestedHeight)
    {
        requestedWidth = clamp(requestedWidth, 128u, 2048u);
        requestedHeight = clamp(requestedHeight, 128u, 2048u);
        if (width == requestedWidth && height == requestedHeight)
            return;

        width = requestedWidth;
        height = requestedHeight;
        renderTarget.reset();
        hasRendered = false;
    }

    void Handle_ViewportInput(bool isHovered, const ImVec2& canvasMin, const ImVec2& canvasMax)
    {
        const ImGuiIO& io = ImGui::GetIO();
        Update_HomeTransition(max(io.DeltaTime, 0.f));

        if (isHovered && ImGui::IsKeyPressed(ImGuiKey_F, false))
            Start_HomeTransition();

        const bool dragInputHeld =
            ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
            ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
            ImGui::IsMouseDown(ImGuiMouseButton_Right);

        if (isViewportDragging && !dragInputHeld)
            End_ViewportDrag();

        const bool shouldBeginDrag =
            isHovered &&
            (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
             ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
             ImGui::IsMouseClicked(ImGuiMouseButton_Right));

        if (shouldBeginDrag)
        {
            Begin_ViewportDrag(canvasMin, canvasMax);
            hasPendingHome = false;
            return;
        }

        if (isViewportDragging)
        {
            const Vec2 mouseDelta = GAME->Get_MouseDelta();
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                EffectPreviewOrbitCamera::Orbit(cameraState, mouseDelta, Internal::kMeshPreviewOrbitSensor);
            else if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) || ImGui::IsMouseDown(ImGuiMouseButton_Right))
            {
                const EffectPreviewOrbitCameraFrame cameraFrame = EffectPreviewOrbitCamera::Build_Frame(
                    cameraState,
                    static_cast<float>(width) / static_cast<float>(max(1u, height)),
                    XMConvertToRadians(35.f),
                    0.1f,
                    100.f
                );
                const float panScale = max(0.01f, cameraState.distance * 0.003f);
                EffectPreviewOrbitCamera::Pan(cameraState, mouseDelta, cameraFrame.right, Vec3::Up, panScale);
            }
        }

        if (isHovered && io.MouseWheel != 0.f)
        {
            hasPendingHome = false;
            EffectPreviewOrbitCamera::Dolly(cameraState, io.MouseWheel, 0.46f, 0.25f, 80.f);
        }
    }

    void Begin_ViewportDrag(const ImVec2& canvasMin, const ImVec2& canvasMax)
    {
        if (GAME == nullptr)
            return;

        RECT lockRect{};
        lockRect.left = static_cast<LONG>(canvasMin.x);
        lockRect.top = static_cast<LONG>(canvasMin.y);
        lockRect.right = static_cast<LONG>(canvasMax.x);
        lockRect.bottom = static_cast<LONG>(canvasMax.y);
        GAME->Set_MouseLockOverrideRect(lockRect);
        GAME->Begin_EditorMouseLockSession();
        GAME->Lock_Mouse();
        isViewportDragging = true;
    }

    void End_ViewportDrag()
    {
        if (!isViewportDragging || GAME == nullptr)
            return;

        isViewportDragging = false;
        GAME->Clear_MouseLockOverrideRect();
        GAME->End_EditorMouseLockSession();
        GAME->Unlock_Mouse();
    }

    void Start_HomeTransition()
    {
        hasPendingHome = true;
    }

    void Update_HomeTransition(float timeDelta)
    {
        if (!hasPendingHome)
            return;

        EffectPreviewOrbitCameraState target{};
        Build_HomeState(target);
        cameraState.pivot = Math::Lerp_Damp(cameraState.pivot, target.pivot, Internal::kMeshPreviewHomeTransitionSpeed, timeDelta);
        cameraState.distance = Math::Lerp_Damp(cameraState.distance, target.distance, Internal::kMeshPreviewHomeTransitionSpeed, timeDelta);
        cameraState.yawDegrees = Math::Lerp_Damp(cameraState.yawDegrees, target.yawDegrees, Internal::kMeshPreviewHomeTransitionSpeed, timeDelta);
        cameraState.pitchDegrees = Math::Lerp_Damp(cameraState.pitchDegrees, target.pitchDegrees, Internal::kMeshPreviewHomeTransitionSpeed, timeDelta);

        if ((cameraState.pivot - target.pivot).LengthSquared() <= 0.001f &&
            fabsf(cameraState.distance - target.distance) <= 0.01f &&
            fabsf(cameraState.yawDegrees - target.yawDegrees) <= 0.01f &&
            fabsf(cameraState.pitchDegrees - target.pitchDegrees) <= 0.01f)
        {
            cameraState = target;
            hasPendingHome = false;
        }
    }

    void Build_HomeState(EffectPreviewOrbitCameraState& outState) const
    {
        Vec3 pivot = Vec3::Zero;
        float distance = Internal::kMeshPreviewHomeDistance;
        if (model != nullptr)
        {
            Vec3 localMin{}, localMax{};
            if (model->Try_Get_LocalBounds(localMin, localMax))
            {
                const Vec3 center = (localMin + localMax) * 0.5f * 0.01f;
                const Vec3 extents = (localMax - localMin) * 0.5f * 0.01f;
                pivot = center;
                distance = max(6.f, extents.Length() * 4.6f);
            }
        }

        EffectPreviewOrbitCamera::Reset(
            outState,
            pivot,
            distance,
            Internal::kMeshPreviewHomeYawDegrees,
            Internal::kMeshPreviewHomePitchDegrees
        );
    }

    void Reset_View()
    {
        EffectPreviewOrbitCamera::Reset(
            cameraState,
            Vec3::Zero,
            Internal::kMeshPreviewHomeDistance,
            Internal::kMeshPreviewHomeYawDegrees,
            Internal::kMeshPreviewHomePitchDegrees
        );
    }

    void Reset_ViewToModel()
    {
        Build_HomeState(cameraState);
    }

    void Draw_ViewportTextOverlay(const ImVec2& canvasMin, const ImVec2& canvasMax, const char* label)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddText(ImVec2(canvasMin.x + 12.f, canvasMin.y + 12.f), IM_COL32(214, 220, 230, 255), label);
        if (!statusMessage.empty())
            drawList->AddText(ImVec2(canvasMin.x + 12.f, canvasMin.y + 34.f), IM_COL32(245, 198, 120, 255), statusMessage.c_str());
        drawList->AddText(
            ImVec2(canvasMin.x + 12.f, canvasMax.y - 24.f),
            IM_COL32(180, 190, 205, 255),
            "LMB orbit / MMB or RMB pan / Wheel zoom / F reset"
        );
    }
};

MeshDataPreview_View::MeshDataPreview_View()
    : Editor_Window{ Internal::kWindowName, ICON_FA_CUBES }
    , _previewRenderer{ make_unique<PreviewRenderer>() }
{
}

MeshDataPreview_View::~MeshDataPreview_View()
{
    Free();
}

void MeshDataPreview_View::Update(float timeDelta)
{
    __super::Update(timeDelta);
    _timeDelta = max(0.f, timeDelta);
}

void MeshDataPreview_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    ImGui::SetNextWindowSize(ImVec2{ 1180.f, 760.f }, ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Get_ImGuiWindowName().c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        const Shared<Emitter_View> emitterView = Resolve_EmitterView();
        AuthoringEmitter* emitter = Find_TargetEmitter(emitterView);
        MeshTypeData* meshData = Find_TargetMeshData(emitter);
        if (emitterView == nullptr || emitter == nullptr || meshData == nullptr)
            ImGui::TextDisabled("Mesh TypeData selection is not available.");
        else
        {
            const float contentWidth = ImGui::GetContentRegionAvail().x;
            const bool useSideBySideLayout = contentWidth >= 860.f;
            const fs::path modelPath = Resolve_AuthoredAssetPath(meshData->modelGuid, meshData->modelPath);

            if (useSideBySideLayout)
            {
                const float spacing = ImGui::GetStyle().ItemSpacing.x;
                const float previewPaneWidth = clamp(contentWidth * 0.56f, 520.f, 760.f);
                ImGui::BeginChild("MeshDataPreviewViewportPane", ImVec2(previewPaneWidth, 0.f), false);
                {
                    if (_previewRenderer != nullptr)
                        _previewRenderer->Draw_Viewport("MeshDataPreviewViewport", *emitter, *meshData, modelPath);
                }
                ImGui::EndChild();

                ImGui::SameLine(0.f, spacing);
                ImGui::BeginChild("MeshDataPreviewInspectorPane", ImVec2(0.f, 0.f), false);
                {
                    Render_Inspector(*emitterView, *emitter, *meshData);
                }
                ImGui::EndChild();
            }
            else
            {
                if (_previewRenderer != nullptr)
                    _previewRenderer->Draw_Viewport("MeshDataPreviewViewport", *emitter, *meshData, modelPath);
                ImGui::Separator();
                Render_Inspector(*emitterView, *emitter, *meshData);
            }
        }
    }

    Set_Open(isOpen);
    ImGui::End();
}

void MeshDataPreview_View::Open_Target(uint32 emitterId)
{
    _targetEmitterId = emitterId;
    Set_Open(true);
}

Shared<Emitter_View> MeshDataPreview_View::Resolve_EmitterView() const
{
    if (EDITOR == nullptr)
        return nullptr;

    const Shared<Editor_Window> emitterWindow = EDITOR->Get_Window(L"Emitter");
    return dynamic_pointer_cast<Emitter_View>(emitterWindow);
}

AuthoringEmitter* MeshDataPreview_View::Find_TargetEmitter(const Shared<Emitter_View>& emitterView) const
{
    return emitterView != nullptr ? emitterView->Find_Emitter(_targetEmitterId) : nullptr;
}

MeshTypeData* MeshDataPreview_View::Find_TargetMeshData(AuthoringEmitter* emitter) const
{
    if (emitter == nullptr || emitter->typeData.kind != AuthoringTypeDataKind::Mesh)
        return nullptr;

    return get_if<MeshTypeData>(&emitter->typeData.payload);
}

fs::path MeshDataPreview_View::Resolve_AuthoredAssetPath(const string& guid, const string& path) const
{
    return Internal::Resolve_AssetPath(guid, path);
}

void MeshDataPreview_View::Render_Inspector(Emitter_View& emitterView, AuthoringEmitter& emitter, MeshTypeData& meshData)
{
    bool changed = false;
    const Emitter_View::AuthoringSnapshot beforeSnapshot = emitterView.Capture_AuthoringSnapshot();

    ImGui::TextUnformatted("MeshData");
    ImGui::TextDisabled("Model asset and assigned material instance are stored in this MeshData.");
    ImGui::Separator();

    changed |= Render_ModelAssetSection(meshData);
    changed |= Render_MaterialAssetSection(meshData);
    changed |= Render_TransformSection(meshData);
    changed |= Render_MaterialInstanceSection(emitter, meshData, nullptr);

    if (ImGui::CollapsingHeader("Validation", ImGuiTreeNodeFlags_DefaultOpen))
        Render_ValidationSummary(meshData);

    if (changed)
    {
        Apply_MeshDataChanged(emitterView, emitter);
        Begin_PendingAuthoringEdit(emitterView, beforeSnapshot, "Edit MeshData");
    }

    Commit_PendingAuthoringEditIfIdle(emitterView);
}

bool MeshDataPreview_View::Render_ModelAssetSection(MeshTypeData& meshData)
{
    bool changed = false;
    if (ImGui::CollapsingHeader("Mesh Asset", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (_detailPropertyContext.Begin_PropertyTable("MeshDataPreviewModelAsset"))
        {
            _detailPropertyContext.Draw_PropertyLabel("Model Asset");
            changed |= Internal::Draw_AssetDropSlot(
                "MeshModelSlot",
                "Model Asset",
                Internal::kEffectModelPayloadType,
                meshData.modelPath,
                "(drop .model here)",
                "MeshData source model",
                [&meshData](const wchar_t* payloadPath)
                {
                    return Internal::Apply_ModelFileToMeshData(payloadPath, meshData);
                },
                [&meshData]
                {
                    wstring pickedFilePath{};
                    if (!Internal::Try_PickAssetDialog(
                        L"Select Mesh Model",
                        L"Model Assets",
                        L"*.model",
                        Internal::kModelDialogDefaultFolder,
                        pickedFilePath
                    ))
                        return false;

                    return Internal::Apply_ModelFileToMeshData(pickedFilePath.c_str(), meshData);
                }
            );
            _detailPropertyContext.Draw_ResetButton(false);
            _detailPropertyContext.End_PropertyTable();
        }
    }
    return changed;
}

bool MeshDataPreview_View::Render_MaterialAssetSection(MeshTypeData& meshData)
{
    bool changed = false;
    if (ImGui::CollapsingHeader("Assigned Effect Material", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (_detailPropertyContext.Begin_PropertyTable("MeshDataPreviewMaterialAsset"))
        {
            _detailPropertyContext.Draw_PropertyLabel("Source Material");
            changed |= Internal::Draw_AssetDropSlot(
                "MeshAssignedMaterialSlot",
                "Source Material",
                Internal::kEffectMaterialPayloadType,
                meshData.assignedEffectMaterialPath,
                "(drop .effectmaterial.json here)",
                "Material source preset",
                [&meshData](const wchar_t* payloadPath)
                {
                    return Internal::Apply_EffectMaterialFileToMeshData(payloadPath, meshData);
                },
                [&meshData]
                {
                    wstring pickedFilePath{};
                    if (!Internal::Try_PickAssetDialog(
                        L"Select Assigned Effect Material",
                        L"Effect Materials",
                        L"*.effectmaterial.json",
                        Internal::kMaterialDialogDefaultFolder,
                        pickedFilePath
                    ))
                        return false;

                    return Internal::Apply_EffectMaterialFileToMeshData(pickedFilePath.c_str(), meshData);
                }
            );
            _detailPropertyContext.Draw_ResetButton(false);
            _detailPropertyContext.End_PropertyTable();
        }

        ImGui::TextDisabled("Embedded material copy");
        if (ImGui::Button("Reset Copy From Source"))
        {
            const fs::path materialPath = Resolve_AuthoredAssetPath(
                meshData.assignedEffectMaterialGuid,
                meshData.assignedEffectMaterialPath
            );
            changed = !materialPath.empty() && EffectMaterialPresetReader::Read(materialPath, meshData.assignedMaterial);
            if (changed)
                meshData.hasAssignedMaterialInstance = true;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("원본 .effectmaterial 파일을 다시 읽어 MeshData 내부 material copy를 덮어씁니다.");

        if (!meshData.hasAssignedMaterialInstance)
            ImGui::TextDisabled("No embedded material copy yet. Assign or reset from source to edit values.");
    }
    return changed;
}

bool MeshDataPreview_View::Render_TransformSection(MeshTypeData& meshData)
{
    bool changed = false;
    if (ImGui::CollapsingHeader("Mesh Preview Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const MeshTypeData defaultData{};
        if (_detailPropertyContext.Begin_PropertyTable("MeshDataPreviewTransform"))
        {
            changed |= _detailPropertyContext.Draw_Vec3Property("Preview Scale", meshData.previewScale, defaultData.previewScale);
            changed |= _detailPropertyContext.Draw_BoolProperty(
                "Use Model Materials",
                meshData.useModelMaterials,
                defaultData.useModelMaterials,
                "Debug fallback입니다. 기본 authoring은 MeshData material copy를 model material slot에 override합니다."
            );
            _detailPropertyContext.End_PropertyTable();
        }
    }
    return changed;
}

bool MeshDataPreview_View::Render_MaterialInstanceSection(const AuthoringEmitter& emitter, MeshTypeData& meshData, bool* outRestartPreview)
{
    if (!meshData.hasAssignedMaterialInstance)
    {
        ImGui::TextDisabled("Assign an effect material source before editing the MeshData material instance.");
        return false;
    }

    ImGui::TextDisabled("Embedded material copy is emitter-local MeshData authoring data.");

    const RequiredModuleData* required = Find_RequiredModuleData(emitter);
    const EffectMaterialRenderPolicyBinding renderPolicyBinding{
        required != nullptr ? required->material.blendMode : meshData.assignedMaterial.blendMode,
        required != nullptr ? required->material.alphaCutoff : meshData.assignedMaterial.alphaCutoff,
        required != nullptr ? "Required" : "Assigned Material"
    };

    return EffectMaterialInstance_View::Draw_InstanceProperties(
        _detailPropertyContext,
        _textureThumbnailCache,
        meshData.assignedMaterial,
        outRestartPreview,
        EffectMaterialTwoSidedDisplayMode::EditableImplemented,
        true,
        &renderPolicyBinding
    );
}

void MeshDataPreview_View::Render_ValidationSummary(const MeshTypeData& meshData)
{
    const fs::path modelPath = Resolve_AuthoredAssetPath(meshData.modelGuid, meshData.modelPath);

    if (meshData.modelGuid.empty() && meshData.modelPath.empty())
        ImGui::TextColored(ImVec4{ 1.f, 0.55f, 0.25f, 1.f }, "Model missing: no model asset is assigned.");
    else if (!fs::exists(modelPath))
        ImGui::TextColored(ImVec4{ 1.f, 0.55f, 0.25f, 1.f }, "Model missing: %s", Internal::To_DisplayPath(modelPath).c_str());
    else if (String::ToLowerCopy(modelPath.extension().string()) != ".model")
        ImGui::TextColored(ImVec4{ 1.f, 0.35f, 0.35f, 1.f }, "Unsupported model extension.");
    else
        ImGui::TextColored(ImVec4{ 0.45f, 0.9f, 0.55f, 1.f }, "Model asset is valid for static preview.");

    const fs::path materialPath = Resolve_AuthoredAssetPath(
        meshData.assignedEffectMaterialGuid,
        meshData.assignedEffectMaterialPath
    );
    if (meshData.assignedEffectMaterialGuid.empty() && meshData.assignedEffectMaterialPath.empty())
        ImGui::TextDisabled("Material missing: assigned effect material source is not set.");
    else if (!fs::exists(materialPath))
        ImGui::TextColored(ImVec4{ 1.f, 0.55f, 0.25f, 1.f }, "Material missing: %s", Internal::To_DisplayPath(materialPath).c_str());
    else
        ImGui::TextColored(ImVec4{ 0.45f, 0.9f, 0.55f, 1.f }, "Assigned effect material source is readable.");

    if (meshData.hasAssignedMaterialInstance)
        ImGui::TextColored(ImVec4{ 0.45f, 0.9f, 0.55f, 1.f }, "MeshData material instance copy is editable.");
    else
        ImGui::TextDisabled("MeshData material instance copy is not initialized.");
}

void MeshDataPreview_View::Apply_MeshDataChanged(Emitter_View& emitterView, AuthoringEmitter& emitter)
{
    emitter.previewDirty = true;
    emitter.rendererType = Authoring::Resolve_RendererType(emitter.typeData);
    if (const MeshTypeData* meshData = Find_TargetMeshData(&emitter))
    {
        emitter.textureId = meshData->hasAssignedMaterialInstance ? meshData->assignedMaterial.mainTexturePath : meshData->assignedEffectMaterialPath;
        emitter.resourceSummary = meshData->modelPath;
    }

    emitterView.MarkDirty();
    MarkDirty();
}

void MeshDataPreview_View::Begin_PendingAuthoringEdit(
    Emitter_View&,
    const Emitter_View::AuthoringSnapshot& beforeSnapshot,
    const string& description)
{
    if (_hasPendingAuthoringEdit)
        return;

    _hasPendingAuthoringEdit = true;
    _pendingAuthoringSnapshot = beforeSnapshot;
    _pendingAuthoringDescription = description;
}

void MeshDataPreview_View::Commit_PendingAuthoringEditIfIdle(Emitter_View& emitterView)
{
    if (!_hasPendingAuthoringEdit || ImGui::IsAnyItemActive())
        return;

    Commit_PendingAuthoringEdit(emitterView);
}

void MeshDataPreview_View::Commit_PendingAuthoringEdit(Emitter_View& emitterView)
{
    if (!_hasPendingAuthoringEdit)
        return;

    Emitter_View::AuthoringSnapshot afterSnapshot = emitterView.Capture_AuthoringSnapshot();
    afterSnapshot.selectedEmitterIndex = _pendingAuthoringSnapshot.selectedEmitterIndex;
    afterSnapshot.selectedTypeData = _pendingAuthoringSnapshot.selectedTypeData;
    afterSnapshot.selectedModuleIndex = _pendingAuthoringSnapshot.selectedModuleIndex;
    emitterView.Execute_AuthoringSnapshotCommand(
        _pendingAuthoringSnapshot,
        afterSnapshot,
        _pendingAuthoringDescription
    );

    _hasPendingAuthoringEdit = false;
    _pendingAuthoringSnapshot = {};
    _pendingAuthoringDescription.clear();
}

Shared<MeshDataPreview_View> MeshDataPreview_View::Create()
{
    return make_shared<MeshDataPreview_View>();
}

void MeshDataPreview_View::Free()
{
    _previewRenderer.reset();
    _textureThumbnailCache.clear();
    __super::Free();
}

NS_END
