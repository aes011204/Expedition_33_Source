#include "pch.h"
#include "ModelInspector.h"
#include <imgui.h>
#include "ModelEditorApp.h"
#include "ModelImGui_Manager.h"
#include "ModelViewerObject.h"
#include "ModelCom.h"

namespace fs = std::filesystem;

NS_BEGIN(ModelEditor)

ModelInspector::ModelInspector() : EditorBlock{}
{
}

HRESULT ModelInspector::Initialize()
{
	_resourcePath = ResourcePath; // 루트 시작 디렉토리
	strcpy_s(_pathBuffer, ToString(_resourcePath).c_str());
	Scan_ModelFiles(_resourcePath);
	return S_OK;
}

void ModelInspector::Update(float timeDelta)
{
}

void ModelInspector::Render()
{
	if (!_showBlock) return;

	ImGui::Begin("Model Inspector");

	ImGui::Text("Resource Path:");
	if (ImGui::InputText("##ResPath", _pathBuffer, sizeof(_pathBuffer)))
	{
		_resourcePath = String::ToWString(_pathBuffer);
	}
	ImGui::SameLine();
	if (ImGui::Button("Scan"))
	{
		Scan_ModelFiles(_resourcePath);
	}

    ImGui::Separator();
    Render_TransformControl();

	ImGui::Separator();
	Render_ModelList();

	ImGui::Separator();
	Render_AnimList();



	ImGui::End();
}

void ModelInspector::Scan_ModelFiles(const wstring& resourcePath)
{
	_cachedModelPaths.clear();
	_cachedAnimPaths.clear();
	_selectedModelPath.clear();
	_selectedAnimPath.clear();

	if (!fs::exists(resourcePath) || !fs::is_directory(resourcePath))
		return;

	for (const auto& entry : fs::recursive_directory_iterator(resourcePath))
	{
		if (entry.is_regular_file() && entry.path().extension() == ".model")
		{
			_cachedModelPaths.push_back(entry.path());
		}
	}
}

void ModelInspector::Scan_AnimFiles(const wstring& modelDirectory)
{
	_cachedAnimPaths.clear();
	_selectedAnimPath.clear();

	if (!fs::exists(modelDirectory) || !fs::is_directory(modelDirectory))
		return;

	for (const auto& entry : fs::directory_iterator(modelDirectory))
	{
		if (entry.is_regular_file() && entry.path().extension() == ".anim")
		{
			_cachedAnimPaths.push_back(entry.path());
		}
	}
}

void ModelInspector::Render_ModelList()
{
	ImGui::Text("Models (.model)");
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_HorizontalScrollbar;
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
	if (ImGui::BeginChild("ModelListRegion", ImVec2(0, 100), true, windowFlags))
	{
		for (const auto& path : _cachedModelPaths)
		{
			string fileName = path.filename().string();
			bool isSelected = (_selectedModelPath == path.string());

			if (ImGui::Selectable(fileName.c_str(), isSelected, 0, ImVec2(150, 0)))
			{
				_selectedModelPath = path.string();
				Scan_AnimFiles(path.parent_path().wstring());
			}

			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				Load_SelectedAsset(".model");
			}
		}
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();
}

void ModelInspector::Render_AnimList()
{
	ImGui::Text("Animations (.anim)");

    auto viewer = ModelImGui_Manager::GetInstance()->Get_CurrentModelObject();

    // Toggles and Dropdown for Animation Settings
    ImGui::Checkbox("Loop", &_isLoop);
    ImGui::SameLine();
    ImGui::Checkbox("Root Motion", &_isRootMotion);
    ImGui::SameLine();

    // Root Node Selection Dropdown
    if (viewer && viewer->Get_ModelComponent())
    {
        uint32 numBones = viewer->Get_NumBones();
        string currentBoneName = (_rootNodeIndex == -1) ? "None" : viewer->Get_BoneName(_rootNodeIndex);

        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::BeginCombo("Root Node", currentBoneName.c_str()))
        {
            if (ImGui::Selectable("None", _rootNodeIndex == -1))
                _rootNodeIndex = -1;

            for (uint32 i = 0; i < numBones; ++i)
            {
                const char* boneName = viewer->Get_BoneName(i);
                bool isSelected = (_rootNodeIndex == (int32)i);
                if (ImGui::Selectable(boneName, isSelected))
                {
                    _rootNodeIndex = (int32)i;
                    viewer->Set_RootNode(_rootNodeIndex);
                }
            }
            ImGui::EndCombo();
        }
    }

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_HorizontalScrollbar;
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

	if (ImGui::BeginChild("AnimListRegion", ImVec2(0, 150), true, windowFlags))
	{
        if (!viewer || !viewer->Get_Component<ModelCom>())
        {
            ImGui::EndChild();
            ImGui::PopStyleColor();
            return;
        }

        if (viewer && viewer->Is_Skeletal())
        {
            uint32 numAnims = viewer->Get_NumAnimations();
            for (uint32 i = 0; i < numAnims; ++i)
            {
                const char* animName = viewer->Get_AnimationName(i);
                if (!animName) continue;

                bool isSelected = (_selectedAnimIndex == static_cast<int32>(i));

                if (ImGui::Selectable(animName, isSelected))
                {
                    _selectedAnimIndex = i;

                    viewer->Play_Animation(i, 0.1f, _isLoop, _isRootMotion);
                }
            }
        }
        else
        {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f),
                "No animations available or non-skeletal model.");
        }
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();
}

void ModelInspector::Render_TransformControl()
{
    ImGui::Text("Transform Control");
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_None;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

    if (ImGui::BeginChild("TransformRegion", ImVec2(0, 100), true, windowFlags))
    {
        auto viewer = ModelImGui_Manager::GetInstance()->Get_CurrentModelObject();
        if (viewer)
        {
            auto transformCom = viewer->Get_Transform();
            if (transformCom)
            {
                Vec3 position = transformCom->Get_Position();
                Vec3 rotation = transformCom->Get_RotationEuler();
                Vec3 scale = transformCom->Get_Scale();

                float pos[3] = { position.x, position.y, position.z };
                float rot[3] = { rotation.x, rotation.y, rotation.z };
                float scl[3] = { scale.x, scale.y, scale.z };

                ImGui::PushItemWidth(200.0f);
                if (ImGui::DragFloat3("Position", pos, 0.1f))
                    transformCom->Set_Position(Vec3(pos[0], pos[1], pos[2]));
                if (ImGui::DragFloat3("Rotation", rot, 1.0f))
                    transformCom->Set_RotationEuler(Vec3(rot[0], rot[1], rot[2]));
                if (ImGui::DragFloat3("Scale", scl, 0.05f, 0.00001f, 10000))
                    transformCom->Set_Scale(Vec3(scl[0], scl[1], scl[2]));
                ImGui::PopItemWidth();
            }
        }
        else
        {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "No Model Selected");
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void ModelInspector::Load_SelectedAsset(const string& extension)
{
	if (extension == ".model")
	{
        auto viewer = ModelImGui_Manager::GetInstance()->Get_CurrentModelObject();

        if (viewer)
        {
            viewer->Change_Model(_selectedModelPath.c_str());
            _selectedAnimIndex = -1;
        }
	}
}

Shared<ModelInspector> ModelInspector::Create()
{
	auto instance = make_shared<ModelInspector>();
	if (FAILED(instance->Initialize()))
	{
		return nullptr;
	}
	return instance;
}

NS_END
