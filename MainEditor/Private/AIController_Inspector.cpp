#include "AIController_Inspector.h"

#include "AIController.h"
#include "BT_View.h"
#include "EditorInstance.h"
#include "Inspector.h"
#include <filesystem>

namespace
{
    namespace fs = std::filesystem;

    const fs::path kBTJsonRoot = "../../../Client/Bin/Resources/Data/json/BT/";

    vector<fs::path> Collect_BTJsonFiles()
    {
        vector<fs::path> files;

        if (!fs::exists(kBTJsonRoot))
            return files;

        for (const auto& entry : fs::directory_iterator(kBTJsonRoot))
        {
            if (!entry.is_regular_file())
                continue;

            if (entry.path().extension() != ".json")
                continue;

            files.push_back(entry.path());
        }

        sort(files.begin(), files.end());
        return files;
    }

    string To_RelativeBTPath(const fs::path& fullPath)
    {
        return (kBTJsonRoot / fullPath.filename()).generic_string();
    }

    string To_PreviewLabel(const string& currentPath)
    {
        if (currentPath.empty())
            return "(None)";

        fs::path path(currentPath);
        return path.has_filename() ? path.filename().string() : currentPath;
    }
}

void AIController_Inspector::Draw_Inspector(const Shared<Component>& component)
{
    Shared<AIController> aiCtrl = dynamic_pointer_cast<AIController>(component);
    if (aiCtrl == nullptr)
        return;

    if (!Draw_Header("AIController"))
        return;

    ImGui::Spacing();

    string currentPath = aiCtrl->Get_BehaviorTreeAssetPath();
    const string preview = To_PreviewLabel(currentPath);
    const vector<fs::path> btFiles = Collect_BTJsonFiles();

    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Behavior Tree Asset");
    ImGui::SameLine(120.f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

    if (ImGui::BeginCombo("##BehaviorTreeAsset", preview.c_str()))
    {
        const bool noneSelected = currentPath.empty();
        if (ImGui::Selectable("(None)", noneSelected))
            aiCtrl->Set_BehaviorTreeAssetPath("");

        if (noneSelected)
            ImGui::SetItemDefaultFocus();

        for (const auto& filePath : btFiles)
        {
            const string relativePath = To_RelativeBTPath(filePath);
            const string fileName = filePath.filename().string();
            const bool isSelected = (currentPath == relativePath);

            if (ImGui::Selectable(fileName.c_str(), isSelected))
                aiCtrl->Set_BehaviorTreeAssetPath(relativePath);

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    const bool hasBTAsset = !currentPath.empty();

    ImGui::BeginDisabled(!hasBTAsset);

    if (ImGui::Button("Open BT"))
    {
        Shared<Editor_Window> window = EDITOR->Get_Window(L"BT View");
        Shared<BT_View> btView = dynamic_pointer_cast<BT_View>(window);

        if (btView)
            btView->Open_BTAsset(currentPath);
    }

    ImGui::SameLine();

    if (ImGui::Button("Open + Debug Me"))
    {
        Shared<Editor_Window> window = EDITOR->Get_Window(L"BT View");
        Shared<BT_View> btView = dynamic_pointer_cast<BT_View>(window);

        if (btView)
        {
            btView->Open_BTAsset(currentPath);
            btView->Set_DebugTargetAIController(aiCtrl);
        }
    }

    ImGui::EndDisabled();

    ImGui::Spacing();

    const Engine::ClassReflectionInfo& info = aiCtrl->Get_ReflectionInfo();
    for (const auto& property : info.properties)
    {
        if (property.name == "_behaviorTreeAssetPath")
            continue;

        Inspector::Draw_ReflectionProperty(info.className, aiCtrl.get(), property);
        ImGui::Spacing();
    }
}

Shared<AIController_Inspector> AIController_Inspector::Create()
{
    return make_shared<AIController_Inspector>();
}
