#include "ModuleDetail_Drawers.h"

#include <shobjidl.h>

#include "EffectEditorInstance.h"
#include "EffectMaterialPreviewRenderer.h"
#include "EffectMaterial_View.h"
#include "EffectMaterialInstance_View.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    constexpr const char* kEffectMaterialPayloadType = "CONTENT_BROWSER_EFFECT_MATERIAL";
    constexpr const wchar_t* kMaterialDialogDefaultFolder = L"../../../Client/Bin/Resources/Effects/Materials";

    fs::path Normalize_MaterialDialogPath(const fs::path& filePath)
    {
        try
        {
            if (fs::exists(filePath))
                return fs::weakly_canonical(filePath);
        }
        catch (...)
        {
        }

        try
        {
            return fs::absolute(filePath).lexically_normal();
        }
        catch (...)
        {
        }

        return filePath.lexically_normal();
    }

    bool Try_PickMaterialPresetDialog(wstring& outFilePath)
    {
        outFilePath.clear();

        const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool shouldUninit = SUCCEEDED(initHr);

        IFileOpenDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || dialog == nullptr)
        {
            if (shouldUninit)
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
                ImGuiDragDropFlags_AcceptBeforeDelivery))
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
            : (isHovered ? IM_COL32(120, 130, 150, 255) : IM_COL32(70, 78, 92, 255));

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

        const string displayName = data.material.sourcePresetName.empty()
            ? data.material.definitionName
            : data.material.sourcePresetName;
        const ImVec2 textPos{ slotMin.x + 57.f, slotMin.y + 18.f };
        drawList->AddText(textPos, IM_COL32(235, 238, 245, 255), displayName.c_str());

        if (isHovered)
        {
            ImGui::SetTooltip(
                "Content Browser의 .effectmaterial.json을 여기에 드롭하면 현재 embedded instance로 복사합니다.\nMain: %s\nNoise: %s\n%s / %s / SubUV %ux%u",
                data.material.mainTextureId.c_str(),
                data.material.noiseTextureId.empty() ? "(none)" : data.material.noiseTextureId.c_str(),
                data.material.blendMode.c_str(),
                data.material.opacitySource.c_str(),
                data.material.subUvRows,
                data.material.subUvCols
            );
        }

        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2{ 0.5f, 0.5f });
        if (ImGui::Button(ICON_FA_FOLDER_OPEN "##PickMaterial", ImVec2{ browseButtonWidth, slotHeight }))
        {
            wstring pickedFilePath{};
            if (Try_PickMaterialPresetDialog(pickedFilePath))
                changed = Apply_PresetFileToRequired(pickedFilePath.c_str(), data);
        }
        ImGui::PopStyleVar();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("머티리얼 원본 파일 선택");

        return changed;
    }
}

bool ModuleDetailDrawContext::Begin_PropertyTable(const char* id)
{
    return DetailProperty::Begin_PropertyTable(id);
}

void ModuleDetailDrawContext::End_PropertyTable()
{
    DetailProperty::End_PropertyTable();
}

bool ModuleDetailDrawContext::Draw_StringProperty(const char* label, string& value, const string& defaultValue)
{
    return DetailProperty::Draw_StringProperty(label, value, defaultValue);
}

bool ModuleDetailDrawContext::Draw_BoolProperty(const char* label, bool& value, bool defaultValue)
{
    return DetailProperty::Draw_BoolProperty(label, value, defaultValue);
}

bool ModuleDetailDrawContext::Draw_FloatProperty(const char* label, float& value, float defaultValue, float speed)
{
    return DetailProperty::Draw_FloatProperty(label, value, defaultValue, speed);
}

bool ModuleDetailDrawContext::Draw_ColorProperty(const char* label, Color& value, const Color& defaultValue)
{
    return DetailProperty::Draw_ColorProperty(label, value, defaultValue);
}

bool ModuleDetailDrawContext::Draw_FloatDistributionGroup(
    const char* label,
    FloatDistributionData& value,
    const FloatDistributionData& defaultValue,
    float speed)
{
    return DetailProperty::Draw_FloatDistributionGroup(label, value, defaultValue, speed);
}

bool ModuleDetailDrawContext::Draw_Vector2DistributionGroup(
    const char* label,
    Vector2DistributionData& value,
    const Vector2DistributionData& defaultValue,
    float speed)
{
    return DetailProperty::Draw_Vector2DistributionGroup(label, value, defaultValue, speed);
}

bool ModuleDetailDrawContext::Draw_Vector3DistributionGroup(
    const char* label,
    Vector3DistributionData& value,
    const Vector3DistributionData& defaultValue,
    float speed)
{
    return DetailProperty::Draw_Vector3DistributionGroup(label, value, defaultValue, speed);
}

bool ModuleDetailDrawContext::Draw_ColorDistributionGroup(
    const char* label,
    ColorDistributionData& value,
    const ColorDistributionData& defaultValue)
{
    return DetailProperty::Draw_ColorDistributionGroup(label, value, defaultValue);
}

bool ModuleDetailDrawContext::Draw_BurstListProperty(
    vector<SpawnModuleData::ParticleBurstData>& burstList,
    const vector<SpawnModuleData::ParticleBurstData>& defaultValue)
{
    return DetailProperty::Draw_BurstListProperty(burstList, defaultValue);
}

bool ModuleDetailDrawContext::Draw_ScreenAlignmentProperty(const char* label, EmitterScreenAlignment& value, EmitterScreenAlignment defaultValue)
{
    return DetailProperty::Draw_ScreenAlignmentProperty(label, value, defaultValue);
}

bool ModuleDetailDrawContext::Draw_SortModeProperty(const char* label, EmitterSortMode& value, EmitterSortMode defaultValue)
{
    return DetailProperty::Draw_SortModeProperty(label, value, defaultValue);
}

bool ModuleDetailDrawContext::Draw_UintProperty(const char* label, uint32& value, uint32 defaultValue)
{
    return DetailProperty::Draw_UintProperty(label, value, defaultValue);
}

bool ModuleDetailDrawContext::Draw_Vec3Property(const char* label, Vec3& value, const Vec3& defaultValue, float speed)
{
    return DetailProperty::Draw_Vec3Property(label, value, defaultValue, speed);
}

bool RequiredModuleDetail::Draw(AuthoringEmitter& emitter, const AuthoringModule& module, RequiredModuleData& data, ModuleDetailDrawContext& drawContext)
{
    const RequiredModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("머티리얼", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("RequiredMaterial"))
        {
            ImGui::PushID("MaterialDropSlot");
            DetailProperty::Draw_PropertyLabel("머티리얼");
            changed |= Internal::Draw_MaterialDropSlot(data);
            DetailProperty::Draw_ResetButton(false);
            ImGui::PopID();

            ImGui::PushID("MaterialEdit");
            DetailProperty::Draw_PropertyLabel("작업");
            if (ImGui::Button("인스턴스 편집"))
                Internal::Open_MaterialInstanceWindow(emitter.id, module.id);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("현재 머티리얼 인스턴스 편집");
            ImGui::SameLine();
            if (ImGui::Button("원본 보기"))
                Internal::Open_PresetBrowserWindow(emitter.id, module.id);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("원본 머티리얼을 확인");
            ImGui::PopID();

            drawContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("이미터 좌표", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("RequiredTransform"))
        {
            changed |= drawContext.Draw_Vec3Property("원점", data.emitterOrigin, defaultData.emitterOrigin);
            changed |= drawContext.Draw_Vec3Property("회전 (도)", data.emitterRotationDegrees, defaultData.emitterRotationDegrees);
            drawContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("공간 / 정렬", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("RequiredSpaceSort"))
        {
            changed |= drawContext.Draw_ScreenAlignmentProperty("화면 정렬", data.screenAlignment, defaultData.screenAlignment);
            changed |= drawContext.Draw_FloatProperty("카메라 블렌드 거리 최소", data.minCameraBlendDistance, defaultData.minCameraBlendDistance);
            changed |= drawContext.Draw_FloatProperty("카메라 블렌드 거리 최대", data.maxCameraBlendDistance, defaultData.maxCameraBlendDistance);
            changed |= drawContext.Draw_BoolProperty("로컬 스페이스 사용", data.useLocalSpace, defaultData.useLocalSpace);
            changed |= drawContext.Draw_SortModeProperty("정렬 모드", data.sortMode, defaultData.sortMode);
            drawContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("시간", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("RequiredTime"))
        {
            changed |= drawContext.Draw_UintProperty("루프 횟수", data.loopCount, defaultData.loopCount);
            changed |= drawContext.Draw_FloatProperty("지속 시간", data.duration, defaultData.duration);
            changed |= drawContext.Draw_FloatProperty("지속 시간 최소값", data.durationLow, defaultData.durationLow);
            changed |= drawContext.Draw_BoolProperty("지속 시간 범위 사용", data.useDurationRange, defaultData.useDurationRange);
            changed |= drawContext.Draw_BoolProperty("루프마다 기간 재계산", data.recalculateDurationEachLoop, defaultData.recalculateDurationEachLoop);
            changed |= drawContext.Draw_FloatProperty("딜레이", data.delay, defaultData.delay);
            changed |= drawContext.Draw_FloatProperty("딜레이 최소값", data.delayLow, defaultData.delayLow);
            changed |= drawContext.Draw_BoolProperty("딜레이 범위 사용", data.useDelayRange, defaultData.useDelayRange);
            changed |= drawContext.Draw_BoolProperty("첫 루프만 딜레이", data.delayFirstLoopOnly, defaultData.delayFirstLoopOnly);
            drawContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("종료 / 킬", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("RequiredKill"))
        {
            changed |= drawContext.Draw_BoolProperty("비활성화 시 킬", data.killOnDeactivate, defaultData.killOnDeactivate);
            changed |= drawContext.Draw_BoolProperty("완료 시 킬", data.killOnCompleted, defaultData.killOnCompleted);
            drawContext.End_PropertyTable();
        }
    }

    if (ImGui::CollapsingHeader("렌더링 제한", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("RequiredRenderLimit"))
        {
            changed |= drawContext.Draw_BoolProperty("최대 드로우 수 사용", data.useMaxDrawCount, defaultData.useMaxDrawCount);
            changed |= drawContext.Draw_UintProperty("최대 드로우 수", data.maxDrawCount, defaultData.maxDrawCount);
            drawContext.End_PropertyTable();
        }
    }

    if (changed)
    {
        emitter.rendererType = data.rendererType;
        emitter.textureId = data.material.mainTextureId;
        emitter.materialName = data.material.definitionName;
        emitter.resourceSummary = data.material.mainTextureId;
    }

    return changed;
}

bool SpawnModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, SpawnModuleData& data, ModuleDetailDrawContext& drawContext)
{
    const SpawnModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("연속 스폰", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("SpawnRate"))
        {
            changed |= drawContext.Draw_BoolProperty("연속 스폰 사용", data.processSpawnRate, defaultData.processSpawnRate);
            drawContext.End_PropertyTable();
        }

        changed |= drawContext.Draw_FloatDistributionGroup("초당 생성 수", data.spawnRate, defaultData.spawnRate);
        changed |= drawContext.Draw_FloatDistributionGroup("스폰 속도 스케일", data.spawnRateScale, defaultData.spawnRateScale);
    }

    if (ImGui::CollapsingHeader("버스트", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("SpawnBurst"))
        {
            changed |= drawContext.Draw_BoolProperty("버스트 사용", data.processBurstList, defaultData.processBurstList);
            drawContext.End_PropertyTable();
        }

        changed |= drawContext.Draw_FloatDistributionGroup("버스트 스케일", data.burstScale, defaultData.burstScale);
        changed |= drawContext.Draw_BurstListProperty(data.burstList, defaultData.burstList);
    }

    if (ImGui::CollapsingHeader("제한", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("SpawnLimit"))
        {
            changed |= drawContext.Draw_UintProperty("최대 활성 파티클 수", data.maxParticleCount, defaultData.maxParticleCount);
            drawContext.End_PropertyTable();
        }
    }

    return changed;
}

bool LifetimeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, LifetimeModuleData& data, ModuleDetailDrawContext& drawContext)
{
    const LifetimeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= drawContext.Draw_FloatDistributionGroup("수명", data.lifeTime, defaultData.lifeTime);

    return changed;
}

bool InitialSizeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialSizeModuleData& data, ModuleDetailDrawContext& drawContext)
{
    const InitialSizeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("크기", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= drawContext.Draw_Vector2DistributionGroup("크기", data.size, defaultData.size);

    return changed;
}

bool InitialVelocityModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialVelocityModuleData& data, ModuleDetailDrawContext& drawContext)
{
    const InitialVelocityModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("속도", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (drawContext.Begin_PropertyTable("InitialVelocity"))
        {
            changed |= drawContext.Draw_Vec3Property("방사 기준점", data.radialPivot, defaultData.radialPivot);
            drawContext.End_PropertyTable();
        }

        changed |= drawContext.Draw_FloatDistributionGroup("속도", data.speed, defaultData.speed);
    }

    return changed;
}

bool InitialColorModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, InitialColorModuleData& data, ModuleDetailDrawContext& drawContext)
{
    const InitialColorModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("초기 컬러", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= drawContext.Draw_ColorDistributionGroup("컬러", data.color, defaultData.color);

    return changed;
}

bool ColorOverLifeModuleDetail::Draw(AuthoringEmitter&, AuthoringModule&, ColorOverLifeModuleData& data, ModuleDetailDrawContext& drawContext)
{
    const ColorOverLifeModuleData defaultData{};
    bool changed = false;

    if (ImGui::CollapsingHeader("수명에 따른 색상", ImGuiTreeNodeFlags_DefaultOpen))
        changed |= drawContext.Draw_ColorDistributionGroup("색상", data.colorOverLife, defaultData.colorOverLife);

    return changed;
}

NS_END
