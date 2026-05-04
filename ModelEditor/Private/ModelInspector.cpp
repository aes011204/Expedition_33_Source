#include "pch.h"
#include "ModelInspector.h"
#include <imgui.h>
#include "ModelEditorApp.h"

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
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_HorizontalScrollbar;
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
	if (ImGui::BeginChild("AnimListRegion", ImVec2(0, 100), true, windowFlags))
	{
		for (const auto& path : _cachedAnimPaths)
		{
			string fileName = path.filename().string();
			bool isSelected = (_selectedAnimPath == path.string());

			if (ImGui::Selectable(fileName.c_str(), isSelected, 0, ImVec2(150, 0)))
			{
				_selectedAnimPath = path.string();
			}

			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				Load_SelectedAsset(".anim");
			}
		}
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();
}

void ModelInspector::Load_SelectedAsset(const string& extension)
{
	if (extension == ".model")
	{
		LOG_INFO("Lazy Load Model: {}", fs::path(_selectedModelPath).filename().string());
	}
	else if (extension == ".anim")
	{
		LOG_INFO("Lazy Load Animation: {}", fs::path(_selectedAnimPath).filename().string());
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
