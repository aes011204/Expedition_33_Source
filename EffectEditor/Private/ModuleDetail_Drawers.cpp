#include "ModuleDetail_Drawers.h"

#include <shobjidl.h>

#include "EffectEditorInstance.h"
#include "Helper_ReadPreset.h"
#include "EffectMaterialPreviewRenderer.h"
#include "EffectMaterial_View.h"
#include "EffectMaterialInstance_View.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    constexpr const char* kEffectMaterialPayloadType = "CONTENT_BROWSER_EFFECT_MATERIAL";
    constexpr const wchar_t* kMaterialDialogDefaultFolder = L"../../../Client/Bin/Resources/Effects/Materials";
    constexpr float kMaterialFloatEpsilon = 0.0001f;

    fs::path Normalize_MaterialDialogPath(const fs::path& filePath)
    {
        try
        {
            if (fs::exists(filePath))
                return fs::weakly_canonical(filePath);
        }
        catch (...)
        {}

        try
        {
            return fs::absolute(filePath).lexically_normal();
        }
        catch (...)
        {}

        return filePath.lexically_normal();
    }

    static bool NearlyEqual(float lhs, float rhs)
    {
        return abs(lhs - rhs) <= kMaterialFloatEpsilon;
    }

    static bool Is_SameColor(const Color& lhs, const Color& rhs)
    {
        return
            NearlyEqual(lhs.x, rhs.x) &&
            NearlyEqual(lhs.y, rhs.y) &&
            NearlyEqual(lhs.z, rhs.z) &&
            NearlyEqual(lhs.w, rhs.w);
    }

    static bool Is_SameMaterial(const EffectMaterialInstanceData& lhs, const EffectMaterialInstanceData& rhs)
    {
        return
            lhs.mainTextureId == rhs.mainTextureId &&
            lhs.noiseTextureId == rhs.noiseTextureId &&
            Is_SameColor(lhs.tint, rhs.tint) &&
            NearlyEqual(lhs.intensity, rhs.intensity) &&
            NearlyEqual(lhs.opacityPower, rhs.opacityPower) &&
            NearlyEqual(lhs.noiseStrength, rhs.noiseStrength) &&
            lhs.blendMode == rhs.blendMode &&
            lhs.opacitySource == rhs.opacitySource &&
            lhs.subUVRows == rhs.subUVRows &&
            lhs.subUVCols == rhs.subUVCols;
    }

    static string Try_ResolvePresetNameFromMaterial(const EffectMaterialInstanceData& material)
    {
        const fs::path presetFolder = Normalize_MaterialDialogPath(kMaterialDialogDefaultFolder);
        if (!fs::exists(presetFolder) || !fs::is_directory(presetFolder))
            return {};

        for (const auto& entry : fs::directory_iterator(presetFolder))
        {
            if (!entry.is_regular_file())
                continue;

            const fs::path presetPath = entry.path();
            if (!String::ToLowerCopy(presetPath.generic_string()).ends_with(".effectmaterial.json"))
                continue;

            EffectMaterialInstanceData presetMaterial{};
            if (!ReadPreset::Read(presetPath, presetMaterial))
                continue;

            if (Is_SameMaterial(material, presetMaterial))
                return presetMaterial.sourcePresetName;
        }

        return {};
    }

    static const string& Resolve_MaterialDisplayName(RequiredModuleData& data)
    {
        if (data.material.sourcePresetName.empty())
            data.material.sourcePresetName = Try_ResolvePresetNameFromMaterial(data.material);

        return data.material.sourcePresetName;
    }

    bool Try_PickMaterialPresetDialog(wstring& outFilePath)
    {
        outFilePath.clear();

        const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool shouldUnInit = SUCCEEDED(initHr);

        IFileOpenDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || dialog == nullptr)
        {
            if (shouldUnInit)
                CoUninitialize();
            return false;
        }

        dialog->SetTitle(L"Select Effect Material");

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST);

        const fs::path defaultFolder = Normalize_MaterialDialogPath(kMaterialDialogDefaultFolder);
        if (!defaultFolder.empty())
        {
            IShellItem* folderItem = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(defaultFolder.wstring().c_str(), nullptr, IID_PPV_ARGS(&folderItem))) && folderItem != nullptr)
            {
                dialog->SetFolder(folderItem);
                folderItem->Release();
            }
        }

        const COMDLG_FILTERSPEC filters[] =
        {
            { L"Effect Materials", L"*.effectmaterial.json" },
            { L"All", L"*.*" },
        };

        dialog->SetFileTypes(std::size(filters), filters);

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

        if (shouldUnInit)
            CoUninitialize();

        return !outFilePath.empty();
    }

    Shared<EffectMaterial_View> Get_MaterialView()
    {
        if (nullptr == EDITOR)
            return nullptr;

        const Shared<Editor_Window> materialWindow = EDITOR->Get_Window(L"Effect Material");
        return dynamic_pointer_cast<EffectMaterial_View>(materialWindow);
    }

    void Open_MaterialInstanceWindow(uint32 emitterId, uint32 moduleId)
    {
        if (nullptr == EDITOR)
            return;

        const Shared<Editor_Window> materialInstanceWindow = EDITOR->Get_Window(L"Effect Material Instance");
        const Shared<EffectMaterialInstance_View> materialInstanceView =
            dynamic_pointer_cast<EffectMaterialInstance_View>(materialInstanceWindow);
        if (nullptr == materialInstanceView)
            return;

        materialInstanceView->Open_Instance(emitterId, moduleId);
    }

    void Open_PresetBrowserWindow(uint32 emitterId, uint32 moduleId)
    {
        const Shared<EffectMaterial_View> materialView = Get_MaterialView();
        if (nullptr == materialView)
            return;

        materialView->Open_PresetBrowser();
    }

    bool Apply_PresetFileToRequired(const wchar_t* filePath, RequiredModuleData& data)
    {
        if (filePath == nullptr || filePath[0] == L'\0')
            return false;

        const Shared<EffectMaterial_View> materialView = Get_MaterialView();
        if (materialView == nullptr)
            return false;

        materialView->Open_Preset(filePath);
        if (!materialView->Has_SelectedPreset())
            return false;

        data.material = materialView->Get_SelectedPresetMaterial();
        return true;
    }

    bool Draw_MaterialDropSlot(RequiredModuleData& data)
    {
        constexpr float browseButtonWidth{ 28.f };
        constexpr float slotHeight = 54.f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const ImGuiPayload* activePayload = ImGui::GetDragDropPayload();
        const bool isMaterialDragging =
            activePayload != nullptr &&
            activePayload->IsDataType(kEffectMaterialPayloadType);

        const ImVec2 slotMin = ImGui::GetCursorScreenPos();
        const float slotWidth = max(180.f, ImGui::GetContentRegionAvail().x - browseButtonWidth - spacing);
        ImGui::InvisibleButton("##MaterialDropSlot", ImVec2(slotWidth, slotHeight));

        bool changed = false;
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                kEffectMaterialPayloadType,
                ImGuiDragDropFlags_AcceptBeforeDelivery
            ))
            {
                if (payload->IsDelivery())
                    changed = Apply_PresetFileToRequired(static_cast<const wchar_t*>(payload->Data), data);
            }

            ImGui::EndDragDropTarget();
        }

        const bool isHovered = ImGui::IsItemHovered();
        const ImVec2 slotMax{ slotMin.x + slotWidth, slotMin.y + slotHeight };
        const ImU32 fillColor = isMaterialDragging
                                ? IM_COL32(42, 56, 78, 255)
                                : IM_COL32(30, 34, 42, 255);
        const ImU32 borderColor = isMaterialDragging
                                  ? IM_COL32(96, 165, 250, 255)
                                  : isHovered ? IM_COL32(120, 130, 150, 255) : IM_COL32(70, 78, 92, 255);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(slotMin, slotMax, fillColor, 6.f);
        drawList->AddRect(slotMin, slotMax, borderColor, 6.f, 0, isMaterialDragging ? 2.5f : 1.5f);

        static EffectMaterialPreviewRenderer materialSlotPreview{ 64 };
        materialSlotPreview.Render_Material(data.material);
        ID3D11ShaderResourceView* previewSrv = materialSlotPreview.Get_SRV();
        const ImVec2 thumbnailMin{ slotMin.x + 7.f, slotMin.y + 7.f };
        const ImVec2 thumbnailMax{ thumbnailMin.x + 40.f, thumbnailMin.y + 40.f };
        if (previewSrv != nullptr)
        {
            const ImTextureID textureId = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(previewSrv));
            drawList->AddImage(textureId, thumbnailMin, thumbnailMax);
        }
        else
        {
            drawList->AddRectFilled(thumbnailMin, thumbnailMax, IM_COL32(18, 20, 24, 255), 4.f);
            drawList->AddText(ImVec2(thumbnailMin.x + 8.f, thumbnailMin.y + 12.f), IM_COL32(150, 160, 176, 255), "MAT");
        }
        drawList->AddRect(thumbnailMin, thumbnailMax, IM_COL32(85, 96, 116, 255), 4.f);

        const string& resolvedName = Internal::Resolve_MaterialDisplayName(data);
        const string displayName = resolvedName.empty() ? "(drop material here)" : resolvedName;
        const ImVec2 textPos{ slotMin.x + 57.f, slotMin.y + 18.f };
        drawList->AddText(textPos, IM_COL32(235, 238, 245, 255), displayName.c_str());

        if (isHovered)
        {
            ImGui::SetTooltip(
                "Content Browser의 .effectmaterial.json 머티리얼 파일을 여기에 드롭하면 현재 embedded material instance로 복사합니다.\nMain: %s\nNoise: %s\n%s / %s / SubUV %ux%u",
                data.material.mainTextureId.c_str(),
                data.material.noiseTextureId.empty() ? "(none)" : data.material.noiseTextureId.c_str(),
                data.material.blendMode.c_str(),
                data.material.opacitySource.c_str(),
                data.material.subUVRows,
                data.material.subUVCols
            );
        }

        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2{ 0.5f, 0.5f });
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 0.f, 0.f });
        if (ImGui::Button(ICON_FA_FOLDER_OPEN "##PickMaterial", ImVec2{ browseButtonWidth, slotHeight }))
        {
            wstring pickedFilePath{};
            if (Try_PickMaterialPresetDialog(pickedFilePath))
                changed = Apply_PresetFileToRequired(pickedFilePath.c_str(), data);
        }
        ImGui::PopStyleVar(2);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("머티리얼 원본 파일 선택");

        return changed;
    }

    static uint32 Resolve_SubUvFrameCount(const AuthoringEmitter& emitter)
    {
        for (const AuthoringModule& module : emitter.modules)
        {
            if (module.type != AuthoringModuleType::Required)
                continue;

            const RequiredModuleData* requiredData = get_if<RequiredModuleData>(&module.data);
            if (requiredData == nullptr)
                return 1;

            const uint32 rows = max(1u, requiredData->material.subUVRows);
            const uint32 cols = max(1u, requiredData->material.subUVCols);
            return max(1u, rows * cols);
        }

        return 1;
    }
}

bool RequiredModuleDetail::Draw(AuthoringEmitter& emitter, const AuthoringModule& module, RequiredModuleData& data, DetailPropertyContext& detailContext)
{
    const RequiredModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("머티리얼", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredMaterial"))
        {
            ImGui::PushID("MaterialDropSlot");
            detailContext.Draw_PropertyLabel("머티리얼");
            changed |= Internal::Draw_MaterialDropSlot(data);
            detailContext.Draw_ResetButton(false);
            ImGui::PopID();

            ImGui::PushID("MaterialEdit");
            detailContext.Draw_PropertyLabel("작업");

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float availableWidth = ImGui::GetContentRegionAvail().x;
            const float editButtonWidth = (availableWidth - spacing) * 0.5f;
            const float sourceButtonWidth = (availableWidth - spacing) * 0.5f;

            if (ImGui::Button("인스턴스 편집", ImVec2{ editButtonWidth, 24.f }))
                Internal::Open_MaterialInstanceWindow(emitter.id, module.id);

            ImGui::SameLine();

            if (ImGui::Button("원본 보기", ImVec2{ sourceButtonWidth, 24.f }))
                Internal::Open_PresetBrowserWindow(emitter.id, module.id);

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("원본 머티리얼을 확인");
            ImGui::PopID();

            detailContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("이미터 좌표", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredTransform"))
        {
            changed |= detailContext.Draw_Vec3Property("원점", data.emitterOrigin, defaultData.emitterOrigin);
            changed |= detailContext.Draw_Vec3Property("회전 (도)", data.emitterRotationDegrees, defaultData.emitterRotationDegrees);
            detailContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("공간 / 정렬", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredSpaceSort"))
        {
            changed |= detailContext.Draw_ScreenAlignmentProperty("화면 정렬", data.screenAlignment, defaultData.screenAlignment);
            changed |= detailContext.Draw_FloatProperty("카메라 블렌드 거리 최소", data.minCameraBlendDistance, defaultData.minCameraBlendDistance);
            changed |= detailContext.Draw_FloatProperty("카메라 블렌드 거리 최대", data.maxCameraBlendDistance, defaultData.maxCameraBlendDistance);
            changed |= detailContext.Draw_BoolProperty("로컬 스페이스 사용", data.useLocalSpace, defaultData.useLocalSpace);
            changed |= detailContext.Draw_SortModeProperty("정렬 모드", data.sortMode, defaultData.sortMode);
            changed |= detailContext.Draw_FloatProperty("카메라 모션 블러 양", data.cameraMotionBlurAmount, defaultData.cameraMotionBlurAmount);
            changed |= detailContext.Draw_BoolProperty("초기화 시 기존 파티클 지우기", data.clearExistingParticlesOnInit, defaultData.clearExistingParticlesOnInit);
            detailContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("시간", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredTime"))
        {
            changed |= detailContext.Draw_UintProperty("루프 횟수", data.loopCount, defaultData.loopCount);
            changed |= detailContext.Draw_FloatProperty("지속 시간", data.duration, defaultData.duration);
            changed |= detailContext.Draw_FloatProperty("지속 시간 최소값", data.durationLow, defaultData.durationLow);
            changed |= detailContext.Draw_BoolProperty("지속 시간 범위 사용", data.useDurationRange, defaultData.useDurationRange);
            changed |= detailContext.Draw_BoolProperty("루프마다 기간 재계산", data.recalculateDurationEachLoop, defaultData.recalculateDurationEachLoop);
            changed |= detailContext.Draw_FloatProperty("딜레이", data.delay, defaultData.delay);
            changed |= detailContext.Draw_FloatProperty("딜레이 최소값", data.delayLow, defaultData.delayLow);
            changed |= detailContext.Draw_BoolProperty("딜레이 범위 사용", data.useDelayRange, defaultData.useDelayRange);
            changed |= detailContext.Draw_BoolProperty("첫 루프만 딜레이", data.delayFirstLoopOnly, defaultData.delayFirstLoopOnly);
            detailContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("서브 UV", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredSubUV"))
        {
            changed |= detailContext.Draw_UintProperty("서브 이미지 가로", data.material.subUVCols, defaultData.material.subUVCols);
            changed |= detailContext.Draw_UintProperty("서브 이미지 세로", data.material.subUVRows, defaultData.material.subUVRows);
            detailContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("종료 / 킬", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredKill"))
        {
            changed |= detailContext.Draw_BoolProperty("비활성화 시 킬", data.killOnDeactivate, defaultData.killOnDeactivate);
            changed |= detailContext.Draw_BoolProperty("완료 시 킬", data.killOnCompleted, defaultData.killOnCompleted);
            detailContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("렌더링 제한", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("RequiredRenderLimit"))
        {
            changed |= detailContext.Draw_BoolProperty("최대 드로우 수 사용", data.useMaxDrawCount, defaultData.useMaxDrawCount);
            changed |= detailContext.Draw_UintProperty("최대 드로우 수", data.maxDrawCount, defaultData.maxDrawCount);
            detailContext.End_PropertyTable();
        }
    }

    if (changed)
    {
        emitter.rendererType = data.rendererType;
        emitter.textureId = data.material.mainTextureId;
        emitter.resourceSummary = data.material.mainTextureId;
    }

    return changed;
}

bool SpawnModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, SpawnModuleData& data, DetailPropertyContext& detailContext)
{
    const SpawnModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("연속 스폰", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SpawnRate"))
        {
            changed |= detailContext.Draw_BoolProperty("연속 스폰 사용", data.processSpawnRate, defaultData.processSpawnRate);
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_FloatDistributionGroup("초당 생성 수", data.spawnRate, defaultData.spawnRate);
        changed |= detailContext.Draw_FloatDistributionGroup("스폰 속도 스케일", data.spawnRateScale, defaultData.spawnRateScale);
    }

    if (ImGui::CollapsingHeader("버스트", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SpawnBurst"))
        {
            changed |= detailContext.Draw_BoolProperty("버스트 사용", data.processBurstList, defaultData.processBurstList);
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_FloatDistributionGroup("버스트 스케일", data.burstScale, defaultData.burstScale);
        changed |= detailContext.Draw_BurstListProperty(data.burstList, defaultData.burstList);
    }

    if (ImGui::CollapsingHeader("제한", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SpawnLimit"))
        {
            changed |= detailContext.Draw_UintProperty("최대 활성 파티클 수", data.maxParticleCount, defaultData.maxParticleCount);
            detailContext.End_PropertyTable();
        }
    }

    return changed;
}

bool LifetimeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, LifetimeModuleData& data, DetailPropertyContext& detailContext)
{
    const LifetimeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_FloatDistributionGroup("수명", data.lifeTime, defaultData.lifeTime);

    return changed;
}

bool InitialSizeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialSizeModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialSizeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("크기", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_Vector2DistributionGroup("크기", data.size, defaultData.size);

    return changed;
}

bool InitialVelocityModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialVelocityModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialVelocityModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("속도", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("InitialVelocity"))
        {
            changed |= detailContext.Draw_Vec3Property("방사 기준점", data.radialPivot, defaultData.radialPivot);
            detailContext.End_PropertyTable();
        }

        changed |= detailContext.Draw_FloatDistributionGroup("속도", data.speed, defaultData.speed);
    }

    return changed;
}

bool InitialColorModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialColorModuleData& data, DetailPropertyContext& detailContext)
{
    const InitialColorModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("초기 컬러", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_ColorDistributionGroup("컬러", data.color, defaultData.color);

    return changed;
}

bool ColorOverLifeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, ColorOverLifeModuleData& data, DetailPropertyContext& detailContext)
{
    const ColorOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 색상", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= detailContext.Draw_ColorDistributionGroup("색상", data.colorOverLife, defaultData.colorOverLife);

    return changed;
}

bool SubUVFrameOverLifeModuleDetail::Draw(
    const AuthoringEmitter& emitter,
    AuthoringModule&,
    SubUVFrameOverLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const SubUVFrameOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 SubUV 프레임", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SubUVFrameOverLife"))
        {
            changed |= detailContext.Draw_UintProperty("시작 프레임", data.startFrame, defaultData.startFrame);
            changed |= detailContext.Draw_UintProperty("종료 프레임", data.endFrame, defaultData.endFrame);
            changed |= detailContext.Draw_BoolProperty("루프", data.loop, defaultData.loop);
            detailContext.End_PropertyTable();
        }

        const uint32 frameCount = Internal::Resolve_SubUvFrameCount(emitter);
        const uint32 lastFrame = frameCount > 0 ? frameCount - 1 : 0;
        data.startFrame = min(data.startFrame, lastFrame);
        data.endFrame = min(data.endFrame, lastFrame);

        ImGui::TextDisabled("현재 atlas frame 수: %u", frameCount);
    }

    return changed;
}

bool SizeByLifeModuleDetail::Draw(
    AuthoringEmitter&,
    AuthoringModule&,
    SizeByLifeModuleData& data,
    DetailPropertyContext& detailContext)
{
    const SizeByLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 크기", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (detailContext.Begin_PropertyTable("SizeByLife"))
        {
            changed |= detailContext.Draw_FloatProperty("X 시작 배율", data.multiplyXStart, defaultData.multiplyXStart);
            changed |= detailContext.Draw_FloatProperty("X 종료 배율", data.multiplyXEnd, defaultData.multiplyXEnd);
            changed |= detailContext.Draw_FloatProperty("Y 시작 배율", data.multiplyYStart, defaultData.multiplyYStart);
            changed |= detailContext.Draw_FloatProperty("Y 종료 배율", data.multiplyYEnd, defaultData.multiplyYEnd);
            detailContext.End_PropertyTable();
        }
    }

    return changed;
}

NS_END
