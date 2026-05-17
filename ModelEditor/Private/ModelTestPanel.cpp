#include "pch.h"
#include "ModelTestPanel.h"
#include <imgui.h>
#include "ModelImGui_Manager.h"
#include "ModelViewerObject.h"
#include "ModelCom.h"
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
	if (!_showBlock) return;

	ImGui::Begin("Model Test Panel");

    Render_MotionWarpingTest();

	ImGui::End();
}

void ModelTestPanel::Render_MotionWarpingTest()
{
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Motion Warping Test");
    ImGui::Separator();

    auto viewer = ModelImGui_Manager::GetInstance()->Get_CurrentModelObject();
    if (!viewer || !viewer->Get_ModelComponent()) {
        ImGui::TextDisabled("No model loaded.");
        return;
    }

    auto modelCom = viewer->Get_ModelComponent();
    auto transformCom = viewer->Get_Transform();

    // 1. 목표 거리 및 스피드 설정
    ImGui::DragFloat("Target Distance (m)", &_testTargetDistance, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("Test Speed", &_testSpeed, 0.05f, 0.1f, 10.0f);

    // 2. 현재 상태 정보 표시
    float currentRatio = modelCom->Get_RootMotionTranslationRatio();
    ImGui::ProgressBar(currentRatio, ImVec2(0.0f, 0.0f), "Ratio");
    ImGui::Text("Current Ratio: %.3f", currentRatio);

    // 3. 테스트 실행/중지 버튼
    if (!_testWarpingActive)
    {
        if (ImGui::Button("Execute Warp Test", ImVec2(-1, 30)))
        {
            _testWarpingActive = true;
            
            // 위치 초기화 및 기준점 동기화
            transformCom->Set_Position(Vec3::Zero);
            _testStartPos = Vec3::Zero; 

            // 애니메이션 강제 리셋 및 즉시 갱신 (순간이동 방지)
            modelCom->Set_AnimationPaused(false);
            modelCom->Set_CurrentAnimationTimeSec(0.f);
            modelCom->Refresh_CurrentAnimationPose();

            // 리셋 직후의 실제 Ratio를 첫 값으로 저장
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

    // 4. [핵심 로직] 워핑 실행 중일 때 좌표 이동
    if (_testWarpingActive)
    {
        float deltaRatio = currentRatio - _testPrevRatio;

        // 애니메이션이 바뀌었거나 루프되어 비율이 역전된 경우 또는 끝난 경우 처리
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

    // 5. 위치 및 상태 초기화 버튼
    if (ImGui::Button("Reset Position & State", ImVec2(-1, 0)))
    {
        _testWarpingActive = false;
        _testPrevRatio = 0.0f;
        _testStartPos = Vec3::Zero;

        transformCom->Set_Position(Vec3::Zero);
        modelCom->Set_AnimationPaused(true);
        modelCom->Set_CurrentAnimationTimeSec(0.f);
        modelCom->Refresh_CurrentAnimationPose();
    }

    // 현재 위치 정보 출력
    Vec3 curPos = transformCom->Get_Position();
    ImGui::Text("Current Pos: X:%.2f, Y:%.2f, Z:%.2f", curPos.x, curPos.y, curPos.z);
    ImGui::Text("Distance from Start: %.3f m", Vec3::Distance(_testStartPos, curPos));
}

Shared<ModelTestPanel> ModelTestPanel::Create()
{
	auto instance = make_shared<ModelTestPanel>();
	if (FAILED(instance->Initialize()))
	{
		return nullptr;
	}
	return instance;
}

NS_END
