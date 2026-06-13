#include "pch.h"
#include "ModelTestPanel.h"

#include <imgui.h>
#include <cmath>

#include "ModelCom.h"
#include "ModelImGui_Manager.h"
#include "ModelViewerObject.h"
#include "TransformCom.h"

NS_BEGIN(ModelEditor)

ModelTestPanel::ModelTestPanel() : EditorBlock{}
{
}

HRESULT ModelTestPanel::Initialize()
{
    return S_OK;
}

void ModelTestPanel::Update(float timeDelta)
{
}

void ModelTestPanel::Render()
{
    if (!_showBlock)
        return;

    ImGui::Begin("Model Test Panel");

    Render_MorphTargetTest();
    ImGui::Separator();
    Render_MotionWarpingTest();

    ImGui::End();
}

Engine::ModelCom* ModelTestPanel::Get_CurrentModelComponent() const
{
    auto viewer = ModelImGui_Manager::GetInstance()->Get_CurrentModelObject();
    if (viewer == nullptr)
        return nullptr;

    return viewer->Get_ModelComponent().get();
}

void ModelTestPanel::Render_MorphTargetTest()
{
    ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "Morph Target Test");
    ImGui::Separator();

    auto modelCom = Get_CurrentModelComponent();
    if (modelCom == nullptr)
    {
        ImGui::TextDisabled("No model loaded.");
        return;
    }

    const MorphData* morphData = modelCom->Get_MorphData();
    if (morphData == nullptr)
    {
        ImGui::TextDisabled("No .morph loaded.");
        return;
    }

    ImGui::Text("Targets: %u", static_cast<uint32>(morphData->targets.size()));
    ImGui::Text("Meshes: %u", static_cast<uint32>(morphData->meshes.size()));

    auto viewer = ModelImGui_Manager::GetInstance()->Get_CurrentModelObject();
    ImGui::Text("Compute Preview: %s / Last Render: %s",
        viewer != nullptr && viewer->Is_ComputePreviewReady() ? "Ready" : "Disabled",
        viewer != nullptr && viewer->Was_LastRenderComputeSkinned() ? "Compute" : "Default");
    ImGui::InputText("Filter##MorphTargetFilter", _morphFilter, sizeof(_morphFilter));

    const string filter = _morphFilter;
    ImGui::BeginChild("MorphTargetList", ImVec2(0.f, 180.f), true);
    for (uint32 i = 0; i < morphData->targets.size(); ++i)
    {
        const string& targetName = morphData->targets[i];
        if (!filter.empty() && targetName.find(filter) == string::npos)
            continue;

        const bool selected = (_selectedMorphIndex == static_cast<int32>(i));
        if (ImGui::Selectable((targetName + "##" + to_string(i)).c_str(), selected))
        {
            _selectedMorphIndex = static_cast<int32>(i);
            _selectedMorphWeight = 1.f;
            modelCom->Reset_MorphWeights();
            modelCom->Set_MorphWeight(i, _selectedMorphWeight);
        }
    }
    ImGui::EndChild();

    if (_selectedMorphIndex >= 0 && _selectedMorphIndex < static_cast<int32>(morphData->targets.size()))
    {
        const string& selectedName = morphData->targets[_selectedMorphIndex];
        ImGui::TextWrapped("Selected: %s", selectedName.c_str());

        Render_SelectedMorphDebug(*morphData, modelCom->Get_MorphWeights());

        if (ImGui::SliderFloat("Weight##MorphTargetWeight", &_selectedMorphWeight, 0.f, 1.f))
        {
            modelCom->Reset_MorphWeights();
            modelCom->Set_MorphWeight(static_cast<uint32>(_selectedMorphIndex), _selectedMorphWeight);
        }
    }

    if (ImGui::Button("Reset Morph", ImVec2(-1.f, 0.f)))
    {
        _selectedMorphWeight = 0.f;
        modelCom->Reset_MorphWeights();
    }
}

void ModelTestPanel::Render_SelectedMorphDebug(const Engine::MorphData& morphData, const vector<float>* weights) const
{
    const uint32 targetIndex = static_cast<uint32>(_selectedMorphIndex);
    float currentWeight = 0.f;
    if (weights != nullptr && targetIndex < weights->size())
        currentWeight = (*weights)[targetIndex];

    uint32 activeMeshCount = 0;
    uint32 totalDeltaCount = 0;
    float maxDeltaLength = 0.f;
    float deltaLengthSum = 0.f;

    for (const MorphMeshData& meshData : morphData.meshes)
    {
        if (targetIndex >= meshData.vertexRanges.size())
            continue;

        const MorphVertexRange& range = meshData.vertexRanges[targetIndex];
        if (range.count == 0 || range.start >= meshData.vertexDeltas.size())
            continue;

        ++activeMeshCount;
        const uint32 rangeEnd = range.start + std::min<uint32>(
            range.count,
            static_cast<uint32>(meshData.vertexDeltas.size()) - range.start);

        for (uint32 deltaIndex = range.start; deltaIndex < rangeEnd; ++deltaIndex)
        {
            const MorphVertexDelta& delta = meshData.vertexDeltas[deltaIndex];
            const Vec3& offset = delta.deltaPosition;
            const float length =
                std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z) *
                std::abs(delta.weightScale);

            maxDeltaLength = std::max(maxDeltaLength, length);
            deltaLengthSum += length;
            ++totalDeltaCount;
        }
    }

    const float avgDeltaLength = totalDeltaCount > 0
        ? deltaLengthSum / static_cast<float>(totalDeltaCount)
        : 0.f;

    ImGui::Text("Current Weight: %.3f", currentWeight);
    ImGui::Text("Target Deltas: %u / Meshes: %u", totalDeltaCount, activeMeshCount);
    ImGui::Text("Delta Length: max %.4f / avg %.4f", maxDeltaLength, avgDeltaLength);
}

void ModelTestPanel::Render_MotionWarpingTest()
{
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Motion Warping Test");
    ImGui::Separator();

    auto viewer = ModelImGui_Manager::GetInstance()->Get_CurrentModelObject();
    if (!viewer || !viewer->Get_ModelComponent())
    {
        ImGui::TextDisabled("No model loaded.");
        return;
    }

    auto modelCom = viewer->Get_ModelComponent();
    auto transformCom = viewer->Get_Transform();

    ImGui::DragFloat("Target Distance (m)", &_testTargetDistance, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("Test Speed", &_testSpeed, 0.05f, 0.1f, 10.0f);

    float currentRatio = modelCom->Get_RootMotionTranslationRatio();
    ImGui::ProgressBar(currentRatio, ImVec2(0.0f, 0.0f), "Ratio");
    ImGui::Text("Current Ratio: %.3f", currentRatio);

    if (!_testWarpingActive)
    {
        if (ImGui::Button("Execute Warp Test", ImVec2(-1, 30)))
        {
            _testWarpingActive = true;
            transformCom->Set_Position(Vec3::Zero);
            _testStartPos = Vec3::Zero;
            modelCom->Set_AnimationPaused(false);
            modelCom->Set_CurrentAnimationTimeSec(0.f);
            modelCom->Refresh_CurrentAnimationPose();
            _testPrevRatio = modelCom->Get_RootMotionTranslationRatio();
        }
    }
    else
    {
        if (ImGui::Button("Stop Test", ImVec2(-1, 30)))
        {
            _testWarpingActive = false;
            modelCom->Set_AnimationPaused(true);
        }
    }

    if (_testWarpingActive)
    {
        float deltaRatio = currentRatio - _testPrevRatio;
        if (deltaRatio < -0.05f || currentRatio >= 0.999f)
        {
            _testWarpingActive = false;
        }
        else if (deltaRatio > 0.0f)
        {
            Vec3 forward = transformCom->Get_LocalForward();
            forward.Normalize();

            Vec3 moveStep = forward * (_testTargetDistance * deltaRatio * _testSpeed);
            moveStep.z *= -1.0f;

            Vec3 currentPos = transformCom->Get_Position();
            transformCom->Set_Position(currentPos + moveStep);
        }

        _testPrevRatio = currentRatio;
    }

    ImGui::Separator();

    if (ImGui::Button("Reset Position & State", ImVec2(-1, 0)))
    {
        _testWarpingActive = false;
        transformCom->Set_Position(Vec3::Zero);
        _testStartPos = Vec3::Zero;
        _testPrevRatio = 0.0f;
        modelCom->Set_AnimationPaused(true);
        modelCom->Set_CurrentAnimationTimeSec(0.f);
        modelCom->Refresh_CurrentAnimationPose();
    }

    Vec3 curPos = transformCom->Get_Position();
    ImGui::Text("Current Pos: X:%.2f, Y:%.2f, Z:%.2f", curPos.x, curPos.y, curPos.z);
    ImGui::Text("Distance from Start: %.3f m", Vec3::Distance(_testStartPos, curPos));
}

Shared<ModelTestPanel> ModelTestPanel::Create()
{
    auto instance = make_shared<ModelTestPanel>();
    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}

NS_END
