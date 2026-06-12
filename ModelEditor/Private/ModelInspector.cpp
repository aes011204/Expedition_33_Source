#include "pch.h"
#include "ModelInspector.h"
#include <imgui.h>
#include "ModelEditorApp.h"
#include "ModelImGui_Manager.h"
#include "ModelViewerObject.h"

namespace fs = std::filesystem;

NS_BEGIN(ModelEditor)

ModelInspector::ModelInspector() : EditorBlock{}
{
}

ModelInspector::~ModelInspector()
{
    if (_hWnd != nullptr)
    {
        _hWnd = nullptr;
    }
}

HRESULT ModelInspector::Initialize()
{
	_resourcePath = ResourcePath;
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
    _modelFilter.Draw("Search Model");

    if (ImGui::BeginChild("ModelListRegion", ImVec2(0, 200), true, ImGuiWindowFlags_HorizontalScrollbar))
    {
        for (const auto& path : _cachedModelPaths)
        {
            const string fileName = path.filename().string();
            
            if (!_modelFilter.PassFilter(fileName.c_str()))
                continue;

            const string fullPath = path.string();
            const bool isSelected = (_selectedModelPath == fullPath);

            // 표시될 이름과 고유 식별자(경로)를 '##'로 구분하여 ImGui ID 충돌을 방지합니다.
            string imguiId = fileName + "##" + fullPath;

            if (ImGui::Selectable(imguiId.c_str(), isSelected, 0, ImVec2(200, 0)))
            {
                _selectedModelPath = fullPath;
                Scan_AnimFiles(path.parent_path().wstring());
                _selectedModelGuid = GAME->Ensure_AssetGUID(path.wstring(), "Model");

                Load_SelectedAsset(".model");
            }

            // 툴팁으로 전체 경로 정보 제공
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("File: %s", fileName.c_str());
                ImGui::Text("Path: %s", fullPath.c_str());
                ImGui::EndTooltip();
            }
        }
    }
    ImGui::EndChild();

    if (!_selectedModelGuid.empty())
        ImGui::Text("Model GUID: %s", _selectedModelGuid.c_str());
}

void ModelInspector::Render_AnimList()
{
	ImGui::Text("Animations (.anim)");

    auto viewer = ModelImGui_Manager::GetInstance()->Get_CurrentModelObject();

    ImGui::Checkbox("Loop", &_isLoop);
    ImGui::SameLine();
    ImGui::Checkbox("Root Motion", &_isRootMotion);
    ImGui::SameLine();
    ImGui::Checkbox("Root Motion Log", &_isRootMotionLog);
    ImGui::SameLine();

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
                bool isSelected = (_rootNodeIndex == static_cast<int32>(i));
                if (ImGui::Selectable(boneName, isSelected))
                {
                    _rootNodeIndex = static_cast<int32>(i);
                    viewer->Set_RootNode(_rootNodeIndex);
                }
            }
            ImGui::EndCombo();
        }
    }

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_HorizontalScrollbar;
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

	if (ImGui::BeginChild("AnimListRegion", ImVec2(0, 200), true, windowFlags))
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

        if (_isRootMotionLog)
        {
            auto modelCom = viewer->Get_ModelComponent();
            if (modelCom)
            {
                Vec3 translation;
                const RootMotionFrame& rootMatrix = modelCom->Get_RootTransformVelocity();
                LOG_INFO("  - Translation: x: {:.3f}, y: {:.3f}, z: {:.3f}\n ", rootMatrix.position.x, rootMatrix.position.y, rootMatrix.position.z);
            }
        }
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();
}

void ModelInspector::Render_TransformControl()
{
    ImGui::Text("Transform Control");
    ImGui::SameLine();
    ImGui::Checkbox("Force Scale 0.0001", &_isScaleMicro);

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
                if (_isScaleMicro)
                {
                    transformCom->Set_Scale(Vec3(0.0001f, 0.0001f, 0.0001f));
                }

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
                
                ImGuiSliderFlags scaleFlags = _isScaleMicro ? ImGuiSliderFlags_ReadOnly : 0;
                if (ImGui::DragFloat3("Scale", scl, 0.05f, 0.00001f, 10000.f, "%.4f", scaleFlags))
                {
                    if (!_isScaleMicro)
                    {
                        transformCom->Set_Scale(Vec3(scl[0], scl[1], scl[2]));
                    }
                }
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

Shared<ModelInspector> ModelInspector::Create(HWND hWnd)
{
	auto instance = make_shared<ModelInspector>();
    instance->Set_WindowHandle(hWnd);

	if (FAILED(instance->Initialize()))
	{
		return nullptr;
	}
	return instance;
}

NS_END
