
#include "BT_View.h"
#include "EditorInstance.h"
#include "AIController.h"
#include "BehaviorTree.h"
#include "Composite.h"
#include "Decorator.h"
#include "GameInstance.h"
#include "BNode.h"
#include "GameObject.h"
#include "Inspector.h"
#include "TypeRegistry.h"

namespace 
{
    ImColor Get_DebugNodeColor(Engine::BTNodeState state)
    {
        switch (state)
        {
        case Engine::BTNodeState::RUN:     return ImVec4(180.f / 255.f, 160.f / 255.f, 40.f / 255.f, 1.f);
        case Engine::BTNodeState::SUCCESS: return ImVec4(60.f / 255.f, 150.f / 255.f, 60.f / 255.f, 1.f);
        case Engine::BTNodeState::FAILED:  return ImVec4(160.f / 255.f, 60.f / 255.f, 60.f / 255.f, 1.f);
        default:                           return ImVec4(60.f / 255.f, 60.f / 255.f, 60.f / 255.f, 1.f);
        }
    }

    string Build_DebugTargetLabel(const wstring& layerTag, const Shared<Engine::GameObject>& gameObject)
    {
        const string objectName = gameObject ? String::ToString(gameObject->Get_Name()) : "Unknown";

        std::ostringstream oss;
        if (!layerTag.empty())
            oss << String::ToString(layerTag) << " / ";

        oss << objectName;

        if (gameObject)
            oss << " [0x" << std::hex << reinterpret_cast<uintptr_t>(gameObject.get()) << "]";

        return oss.str();
    }

    void Save_BTNodeProperties(json& nodeJson, const Shared<Engine::BNode>& node)
    {
        if (node == nullptr)
            return;

        auto& info = node->Get_ReflectionInfo();
        const char* base = reinterpret_cast<const char*>(node.get());

        for (const auto& prop : info.properties)
        {
            const string key = prop.jsonKey.empty() ? prop.name : prop.jsonKey;
            const void* address = base + prop.offset;

            switch (prop.type)
            {
            case PropertyType::Bool:
                nodeJson[key] = *static_cast<const bool*>(address);
                break;

            case PropertyType::Int:
                nodeJson[key] = *static_cast<const int*>(address);
                break;

            case PropertyType::Float:
                nodeJson[key] = *static_cast<const float*>(address);
                break;

            case PropertyType::String:
                nodeJson[key] = *static_cast<const string*>(address);
                break;

            case PropertyType::Enum:
                nodeJson[key] = *reinterpret_cast<const int*>(address);
                break;

            default:
                break;
            }
        }
    }

    void Restore_BTNodeProperties(const json& nodeJson, const Shared<Engine::BNode>& node)
    {
        if (node == nullptr)
            return;

        auto& info = node->Get_ReflectionInfo();
        char* base = reinterpret_cast<char*>(node.get());

        for (const auto& prop : info.properties)
        {
            const string key = prop.jsonKey.empty() ? prop.name : prop.jsonKey;
            if (!nodeJson.contains(key))
                continue;

            void* address = base + prop.offset;

            switch (prop.type)
            {
            case PropertyType::Bool:
                *static_cast<bool*>(address) = nodeJson[key].get<bool>();
                break;

            case PropertyType::Int:
                *static_cast<int*>(address) = nodeJson[key].get<int>();
                break;

            case PropertyType::Float:
                *static_cast<float*>(address) = nodeJson[key].get<float>();
                break;

            case PropertyType::String:
                *static_cast<string*>(address) = nodeJson[key].get<string>();
                break;

            case PropertyType::Enum:
                *reinterpret_cast<int*>(address) = nodeJson[key].get<int>();
                break;

            default:
                break;
            }
        }
    }
}

BT_View::BT_View() : Editor_Window(L"BT View")
{
}

BT_View::~BT_View()
{
    Free();
}

HRESULT BT_View::Initialize()
{
     Recreate_EditorContext();
    //Create_Node();

     Set_Open(false);

    return S_OK;
}

void BT_View::Update(float timeDelta)
{
}

void BT_View::Inspactor()
{
    EditorNode* node = FindNodeById(_selectedNodeId);

    if (node == nullptr)
    {
        ImGui::Text("No node selected");
        return;
    }

    ImGui::Text("Selected Node");
    ImGui::Separator();

    if (ImGui::InputText("Name", node->titleBuffer, sizeof(node->titleBuffer)))
    {
        node->title = node->titleBuffer;
    }

    ImGui::Text("Type: %d", static_cast<int>(node->type));

    if (!node->className.empty())
        ImGui::Text("Class : %s", node->className.c_str());

    if (!node->category.empty())
        ImGui::Text("Category : %s", node->category.c_str());

    ImGui::Separator();

    const bool hasRuntimeNode = (node->runtimeNode != nullptr);
    if (hasRuntimeNode)
    {
        vector<Engine::BTBlackboardKeyView> keyViews;
        keyViews.reserve(_blackboardKeys.size());

        for (const auto& key : _blackboardKeys)
            keyViews.push_back({ key.name, key.type });

        Inspector::Set_BTBlackboardKeys(&keyViews);

        Engine::ClassReflectionInfo& info = node->runtimeNode->Get_ReflectionInfo();
        Inspector::Draw_ReflectionSection(info.className, node->runtimeNode.get(), info);

        Inspector::Set_BTBlackboardKeys(nullptr);
    }

    if (!hasRuntimeNode)
    {
        switch (node->type)
        {
        case NodeType::Selector:
            ImGui::Text("Selector Node");
            break;

        case NodeType::Sequence:
            ImGui::Text("Sequence Node");
            break;

        case NodeType::Task:
        {
            ImGui::Text("Task Node");
            static int damage = 10;
            ImGui::InputInt("Damage", &damage);
            break;
        }

        case NodeType::Decorator:
        {
            ImGui::Text("Decorator Node");
            ImGui::TextDisabled("Child order is fixed by slot index.");
            static bool flag = true;
            ImGui::Checkbox("Condition", &flag);
            break;
        }

        case NodeType::Parallel:
        {
            ImGui::Text("Parallel Node");
            ImGui::TextDisabled("Child 0 is the main task.");
            break;
        }

        default:
            break;
        }
    }

    if (CanNodeHaveChildren(node->type))
    {
        ImGui::Separator();
        ImGui::Text("Child Slots");

        for (size_t i = 0; i < node->outputPins.size(); ++i)
            ImGui::BulletText("%s", GetOutputPinLabel(*node, i));

        if (node->type != NodeType::Decorator)
        {
            if (ImGui::Button("Add Slot"))
                node->outputPins.push_back(ed::PinId(GenerateID()));

            ImGui::SameLine();

            if (ImGui::Button("Remove Slot") && node->outputPins.size() > GetDefaultOutputPinCount(node->type))
            {
                const ed::PinId removedPin = node->outputPins.back();
                node->outputPins.pop_back();

                _links.erase(
                    std::remove_if(_links.begin(), _links.end(),
                        [removedPin](const EditorLink& link)
                        {
                            return link.startPin == removedPin;
                        }),
                    _links.end());
            }
        }
    }
}


void BT_View::Update_SelectedNode()
{
    int count = ed::GetSelectedObjectCount();

    if (count == 0)
    {
        _selectedNodeId = ed::NodeId();
        return;
    }

    std::vector<ed::NodeId> nodes(count);
    int actual = ed::GetSelectedNodes(nodes.data(), count);

    if (actual > 0)
    {
        _selectedNodeId = nodes[0]; // 하나만 선택
    }
}
void BT_View::Recreate_EditorContext()
{
    if (_editorContext)
    {
        ed::SetCurrentEditor(nullptr);
        ed::DestroyEditor(_editorContext);
        _editorContext = nullptr;
    }

    ed::Config config;
    config.SettingsFile = nullptr;

    _editorContext = ed::CreateEditor(&config);

    /*_selectedNodeId = ed::NodeId();
    _pendingPositions.clear();

    for (const auto& node : _nodes)
    {
        _pendingPositions.insert(node.id.Get());
    }*/
}
void BT_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    std::string windowName = String::ToString(Get_WindowName());
    windowName += " - ";
    windowName += _currentFileName;
    windowName += "###BT View";

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));

    if (!ImGui::Begin(windowName.c_str(), &isOpen))
    {
        ImGui::End();
        ImGui::PopStyleVar();
        Set_Open(isOpen);
        return;
    }

    ImGui::Text("Behavior Tree Editor");
    Draw_ToolBar();
    Draw_DebugTargetSelector();
    Draw_LoadPopup();
    Draw_SavePopup();
    ImGui::Separator();



    float leftWidth = ImGui::GetContentRegionAvail().x * 0.7f;

    ImGui::BeginChild("Canvas", ImVec2(leftWidth, 0), true);
    {
        if (_pendingContextRebuild)
        {
            Recreate_EditorContext();
            _pendingContextRebuild = false;
        }

        ed::SetCurrentEditor(_editorContext);
        ImGui::Text("Node Editor Area");

        ImGui::Separator();

        ed::Begin("BT Canvas");

        if (_needsInitialNodePlacement)
        {
            for (auto& node : _nodes)
                ed::SetNodePosition(node.id, node.position);

            _needsInitialNodePlacement = false;
        }

        Update_SelectedNode();

        Update_NodePositions();
        Refresh_DebugState();

        Draw_Nodes();
        Draw_Links();
        Handle_CreateLink();

        Handle_DeleteLink();
        Handle_DeleteNode();

        Handle_CreateNodePopup();
        ed::End();
        ed::SetCurrentEditor(nullptr);
       
    }
    ImGui::EndChild();

    ImGui::SameLine();



    ImGui::BeginChild("RightArea", ImVec2(0, 0), false);
    {
        //  Inspector 
        float inspectorHeight = ImGui::GetContentRegionAvail().y * 0.6f;

        ImGui::BeginChild("Inspector", ImVec2(0, inspectorHeight), true);
        {
            ImGui::Text("Node Inspector");
            ImGui::Separator();
            //
            Inspactor();
            
        }
        ImGui::EndChild();

        //  Blackboard
        ImGui::BeginChild("Blackboard", ImVec2(0, 0), true);
        {
            ImGui::Text("Blackboard");
            //
            ImGui::Separator();
            Draw_Blackboard();
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleVar();

    Set_Open(isOpen);

}

void BT_View::Create_Node(NodeType type)
{
    //// Selector
    //EditorNode selector;
    //selector.type = NodeType::Selector;
    //selector.id = ed::NodeId(GenerateID());
    //selector.inputPin = ed::PinId(GenerateID());
    //selector.outputPin = ed::PinId(GenerateID());
    //selector.title = "Selector";
    //selector.position = ImVec2(100, 100);

    //// Attack
    //EditorNode attack;
    //attack.id = ed::NodeId(GenerateID());
    //attack.type = NodeType::Task;
    //attack.inputPin = ed::PinId(GenerateID());
    //attack.outputPin = ed::PinId(GenerateID());
    //attack.title = "Attack";
    //attack.position = ImVec2(400, 100);

    //_nodes.push_back(selector);
    //_nodes.push_back(attack);

    EditorNode node;

    node.id = ed::NodeId(GenerateID());
    node.inputPin = ed::PinId(GenerateID());
    node.type = type;
    EnsureOutputPins(node, GetDefaultOutputPinCount(type));

    switch (type)
    {
    case NodeType::Selector:
        node.title = "Selector";
        break;
    case NodeType::Sequence:
        node.title = "Sequence";
        break;
    case NodeType::Parallel:
        node.title = "Parallel";
        break;
    case NodeType::Task:
        node.title = "Task";
        break;
    case NodeType::Decorator:
        node.title = "Decorator";
        break;
    }
    strcpy_s(node.titleBuffer, node.title.c_str());

    node.position = _popupPos;

    if (FindRootNode() == nullptr)
        node.isRoot = true;


    _nodes.push_back(node);


    ed::SetNodePosition(node.id, _popupPos);
}

void BT_View::Create_Node(const string& className, const string& category)
{
    EditorNode node;

    node.id = ed::NodeId(GenerateID());
    node.inputPin = ed::PinId(GenerateID());

    node.className = className;
    node.category = category;
    node.isRuntimeBTNode = true;

    node.title = className;
    strcpy_s(node.titleBuffer, node.title.c_str());

    if (category == "Task")
        node.type = NodeType::Task;
    else if (category == "Decorator" || category == "Condition")
        node.type = NodeType::Decorator;
    else
        node.type = NodeType::Task;

    EnsureOutputPins(node, GetDefaultOutputPinCount(node.type));

    node.position = _popupPos;

    node.runtimeNode = dynamic_pointer_cast<Engine::BNode>(
        GAME->Create_BTNodePrototype(className));


    if (FindRootNode() == nullptr)
        node.isRoot = true;

    _nodes.push_back(node);
    ed::SetNodePosition(node.id, node.position);
}

void BT_View::Create_NewBT()
{
    _nodes.clear();
    _links.clear();
    _selectedNodeId = ed::NodeId();

    _currentFileName = "Untitled";
    strcpy_s(_saveFileName, "BT_Test");

    _pendingContextRebuild = true;
    _needsInitialNodePlacement = false;
}

void BT_View::Save_BT(const std::string& path)
{
    json j;

    for (auto& node : _nodes)
    {
        json n;

        n["id"] = (uint64_t)node.id.Get();
        n["type"] = (int)node.type;
        n["title"] = node.title;
        n["x"] = node.position.x;
        n["y"] = node.position.y;
        n["inputPin"] = (uint64_t)node.inputPin.Get();

        if (!node.outputPins.empty())
        {
            n["outputPin"] = (uint64_t)node.outputPins.front().Get();
            for (const auto& outputPin : node.outputPins)
                n["outputPins"].push_back((uint64_t)outputPin.Get());
        }

        n["className"] = node.className;
        n["category"] = node.category;
        n["isRuntimeBTNode"] = node.isRuntimeBTNode;

        Save_BTNodeProperties(n, node.runtimeNode);
       
        j["nodes"].push_back(n);
    }

    EditorNode* rootNode = FindRootNode();
    if (rootNode != nullptr)
        j["rootNodeId"] = static_cast<uint64_t>(rootNode->id.Get());

    for (auto& link : _links)
    {
        json l;

        l["id"] = (uint64_t)link.id.Get();
        l["start"] = (uint64_t)link.startPin.Get();
        l["end"] = (uint64_t)link.endPin.Get();

        j["links"].push_back(l);
    }
    for (auto& key : _blackboardKeys)
    {
        json bk;
        bk["name"] = key.name;
        bk["type"] = static_cast<int>(key.type);
        j["blackboardKeys"].push_back(bk);
    }
    fs::create_directories(fs::path(path).parent_path());

    std::ofstream file(path);
    if (!file.is_open())
        return;

    file << j.dump(4);
    file.close();

    _currentFileName = fs::path(path).filename().string();
    strcpy_s(_saveFileName, _currentFileName.c_str());

    Reload_AIControllersUsingBT(path);

}

void BT_View::Load_BT(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return;

    json j;
    file >> j;
    file.close();

    // =========================
    // 초기화
    // =========================
    _nodes.clear();
    _links.clear();
    _selectedNodeId = ed::NodeId();
    _blackboardKeys.clear();
    // =========================
    // Nodes
    // =========================
    for (auto& n : j["nodes"])
    {
        EditorNode node;

        node.id = ed::NodeId(n["id"].get<uint64_t>());
        node.type = (NodeType)n["type"];
        node.title = n["title"];
        node.position = ImVec2(n["x"], n["y"]);

        node.inputPin = ed::PinId(n["inputPin"].get<uint64_t>());

        if (n.contains("outputPins"))
        {
            for (auto& outputPin : n["outputPins"])
                node.outputPins.push_back(ed::PinId(outputPin.get<uint64_t>()));
        }
        else if (n.contains("outputPin"))
        {
            node.outputPins.push_back(ed::PinId(n["outputPin"].get<uint64_t>()));
        }

        strcpy_s(node.titleBuffer, node.title.c_str());

        node.className = n.value("className", std::string{});

        if (!node.className.empty())
        {
            node.runtimeNode = dynamic_pointer_cast<Engine::BNode>(
                GAME->Create_BTNodePrototype(node.className));

            Restore_BTNodeProperties(n, node.runtimeNode);
        }

        node.category = n.value("category", std::string{});
        node.isRuntimeBTNode = n.value("isRuntimeBTNode", false);

        _nodes.push_back(node);
    }

    uint64_t rootNodeId = 0;
    if (j.contains("rootNodeId"))
        rootNodeId = j["rootNodeId"].get<uint64_t>();

    for (auto& node : _nodes)
    {
        node.isRoot = (rootNodeId != 0 && node.id.Get() == rootNodeId);
    }

    if (FindRootNode() == nullptr && !_nodes.empty())
        _nodes[0].isRoot = true;
    // =========================
    // Links
    // =========================
    for (auto& l : j["links"])
    {
        EditorLink link;

        link.id = ed::LinkId(l["id"].get<uint64_t>());
        link.startPin = ed::PinId(l["start"].get<uint64_t>());
        link.endPin = ed::PinId(l["end"].get<uint64_t>());

        _links.push_back(link);
    }

  
    _idCounter = 1;

    for (auto& node : _nodes)
    {
        _idCounter = max(_idCounter, (uint64_t)node.id.Get() + 1);
        _idCounter = max(_idCounter, (uint64_t)node.inputPin.Get() + 1);

        for (const auto& outputPin : node.outputPins)
            _idCounter = max(_idCounter, (uint64_t)outputPin.Get() + 1);
    }

    for (auto& link : _links)
    {
        _idCounter = max(_idCounter, (uint64_t)link.id.Get() + 1);
    }

    // Older BT assets may have fewer output pins than the current editor expects.
    // Expand them only after the max serialized ID is known to avoid pin ID collisions.
    for (auto& node : _nodes)
        EnsureOutputPins(node, GetDefaultOutputPinCount(node.type));

    _currentFileName = fs::path(path).filename().string();
    strcpy_s(_saveFileName, _currentFileName.c_str());

    // bb
    if (j.contains("blackboardKeys"))
    {
        for (auto& bk : j["blackboardKeys"])
        {
            BlackboardKey key;
            key.name = bk["name"].get<std::string>();
            key.type = static_cast<Engine::BlackboardValueType>(bk["type"].get<int>());
            strcpy_s(key.nameBuffer, key.name.c_str());
            _blackboardKeys.push_back(key);
        }
    }


    _pendingContextRebuild = true;
    _needsInitialNodePlacement = true;
}

void BT_View::Open_BTAsset(const string& relativePath)
{
    if (relativePath.empty())
        return;

    Load_BT(relativePath);
    Set_Open(true);
}

void BT_View::Set_DebugTargetAIController(const Shared<Engine::AIController>& aiController)
{
    _debugAIController = aiController;
    Set_Open(true);
}

void BT_View::Draw_LoadPopup()
{
    if (_openLoadPopup)
    {
        ImGui::OpenPopup("Load BT");

        _openLoadPopup = false;
    }

    if (ImGui::BeginPopupModal("Load BT"))
    {
        for (const std::string& file : _btFiles)
        {
            if (ImGui::Selectable(file.c_str()))
            {
                Load_BT(_path + file);

                ImGui::CloseCurrentPopup();
            }
        }

        if (ImGui::Button("Close"))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void BT_View::Draw_SavePopup()
{
    if (_openSavePopup)
    {
        ImGui::OpenPopup("Save BT");
        _openSavePopup = false;
    }

    if (ImGui::BeginPopupModal("Save BT"))
    {
        ImGui::InputText("File Name", _saveFileName, sizeof(_saveFileName));

        if (ImGui::Button("Save"))
        {
            _pendingSavePath = Build_SavePath(_saveFileName);

            if (fs::exists(_pendingSavePath))
            {
                _openOverwritePopup = true;
            }
            else
            {
                Save_BT(_pendingSavePath);
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    if (_openOverwritePopup)
    {
        ImGui::OpenPopup("Overwrite BT");
        _openOverwritePopup = false;
    }

    if (ImGui::BeginPopupModal("Overwrite BT"))
    {
        ImGui::Text("File already exists. Overwrite?");

        if (ImGui::Button("Overwrite"))
        {
            Save_BT(_pendingSavePath);
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

void BT_View::Draw_Nodes()
{
    static bool firstFrame = true;

    for (EditorNode& node : _nodes)
    {
        if (firstFrame)
            ed::SetNodePosition(node.id, node.position);

        const bool isSelected = (node.id == _selectedNodeId);
        const auto debugIt = _debugNodeStates.find(node.id.Get());
        const bool hasDebugState =
            (debugIt != _debugNodeStates.end()) &&
            (debugIt->second.tick == _currentDebugTreeTick);

        int pushedStyleCount = 0;

        if (hasDebugState)
        {
            ed::PushStyleColor(ed::StyleColor_NodeBg, Get_DebugNodeColor(debugIt->second.state));
            ++pushedStyleCount;
        }

        if (isSelected)
        {
            ed::PushStyleColor(ed::StyleColor_NodeBorder, ImColor(120, 180, 255));
            ++pushedStyleCount;
        }

        ed::BeginNode(node.id);

        if (node.isRoot)
            ImGui::Text("[Root] %s", node.title.c_str());
        else
            ImGui::Text("%s", node.title.c_str());
        ImGui::Spacing();

        ed::BeginPin(node.inputPin, ed::PinKind::Input);
        ImGui::Text("-> In");
        ed::EndPin();

        if (CanNodeHaveChildren(node.type))
        {
            for (size_t i = 0; i < node.outputPins.size(); ++i)
            {
                ed::BeginPin(node.outputPins[i], ed::PinKind::Output);
                ImGui::Text("%s ->", GetOutputPinLabel(node, i));
                ed::EndPin();
            }
        }

        ed::EndNode();

        while (pushedStyleCount-- > 0)
            ed::PopStyleColor();
    }

    firstFrame = false;
}

BT_View::EditorNode* BT_View::FindRootNode()
{
    for (auto& node : _nodes)
    {
        if (node.isRoot)
            return &node;
    }

    return nullptr;
}

void BT_View::SetRootNode(EditorNode* newRoot)
{
    for (auto& node : _nodes)
        node.isRoot = false;

    if (newRoot != nullptr)
        newRoot->isRoot = true;
}

void BT_View::Draw_DebugTargetSelector()
{
    ImGui::SameLine();

    Shared<Engine::AIController> current = _debugAIController.lock();
    Shared<Engine::GameObject> selectedObject = EDITOR->Selected_Object();
    Shared<Engine::AIController> selectedAI =
        selectedObject ? selectedObject->Get_Component<Engine::AIController>() : nullptr;

    if (selectedAI != nullptr && ImGui::SmallButton("Use Selected"))
        _debugAIController = selectedAI;

    ImGui::SameLine();

    string preview = "(None)";

    if (current != nullptr)
    {
        Shared<Engine::GameObject> owner = current->Get_Owner();
        preview = owner ? Build_DebugTargetLabel(L"", owner) : "Selected AI";
    }

    ImGui::SetNextItemWidth(220.f);
    if (ImGui::BeginCombo("##BTDebugTarget", preview.c_str()))
    {
        const bool noneSelected = (current == nullptr);
        if (ImGui::Selectable("(None)", noneSelected))
            _debugAIController.reset();

        if (noneSelected)
            ImGui::SetItemDefaultFocus();

        GAME->Visit_CurrentLevelObjects(
            [&](const wstring& layerTag, const Shared<Engine::GameObject>& gameObject)
            {
                if (gameObject == nullptr)
                    return;

                Shared<Engine::AIController> aiController = gameObject->Get_Component<Engine::AIController>();
                if (aiController == nullptr)
                    return;

                const string label = Build_DebugTargetLabel(layerTag, gameObject);
                const bool isSelected = (current == aiController);

                if (ImGui::Selectable(label.c_str(), isSelected))
                    _debugAIController = aiController;

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            });

        ImGui::EndCombo();
    }
}

void BT_View::Refresh_DebugState()
{
    _debugNodeStates.clear();
    _currentDebugTreeTick = 0;

    Shared<Engine::AIController> aiController = _debugAIController.lock();
    if (aiController == nullptr)
        return;

    Shared<Engine::BehaviorTree> tree = aiController->Get_BehaviorTree();
    if (tree == nullptr)
        return;

    _currentDebugTreeTick = tree->Get_DebugTick();

    Visit_RuntimeNode(tree->Get_Root(),
        [&](const Shared<Engine::BNode>& node)
        {
            if (node == nullptr)
                return;

            DebugNodeState debugState{};
            debugState.state = node->Get_LastState();
            debugState.tick = node->Get_LastDebugTick();
            _debugNodeStates[node->Get_AssetNodeId()] = debugState;
        });
}

void BT_View::Reload_AIControllersUsingBT(const string& savedPath)
{
    const string normalizedSavedPath = fs::path(savedPath).generic_string();

    GAME->Visit_CurrentLevelObjects(
        [&](const wstring&, const Shared<Engine::GameObject>& gameObject)
        {
            if (gameObject == nullptr)
                return;

            Shared<Engine::AIController> aiController =
                gameObject->Get_Component<Engine::AIController>();

            if (aiController == nullptr)
                return;

            const string normalizedAssetPath =
                fs::path(aiController->Get_BehaviorTreeAssetPath()).generic_string();

            if (normalizedAssetPath != normalizedSavedPath)
                return;

            aiController->Reload_BehaviorTree();
        });
}

void BT_View::Visit_RuntimeNode(
    const Shared<Engine::BNode>& node,
    const function<void(const Shared<Engine::BNode>&)>& visitor)
{
    if (node == nullptr)
        return;

    visitor(node);

    if (auto composite = dynamic_pointer_cast<Engine::Composite>(node))
    {
        for (const auto& child : composite->Get_Children())
            Visit_RuntimeNode(child, visitor);

        return;
    }

    if (auto decorator = dynamic_pointer_cast<Engine::Decorator>(node))
        Visit_RuntimeNode(decorator->Get_Child(), visitor);
}

void BT_View::Refresh_BT_FileList()
{
    _btFiles.clear();

    for (const auto& entry : fs::directory_iterator(_path))
    {
        if (entry.path().extension() == ".json")
        {
            _btFiles.push_back(
                entry.path().filename().string()
            );
        }
    }
}


void BT_View::Draw_ToolBar()
{
    if (ImGui::Button("New BT"))
    {
        Create_NewBT();
    }

    ImGui::SameLine();

    if (ImGui::Button("Save"))
    {
        _openSavePopup = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("Load"))
    {
        Refresh_BT_FileList();

        _openLoadPopup = true;
    }
}

void BT_View::Draw_Links()
{
    for (auto& link : _links)
    {
        ed::Link(link.id, link.startPin, link.endPin);
    }
}

void BT_View::Handle_CreateLink()
{
    const bool isCreating = ed::BeginCreate();

    if (isCreating)
    {
        ed::PinId startPin = 0;
        ed::PinId endPin = 0;

        if (ed::QueryNewLink(&startPin, &endPin))
        {
            ed::PinId inputPin = 0;
            ed::PinId outputPin = 0;

            if (IsOutputPin(startPin) && IsInputPin(endPin))
            {
                outputPin = startPin;
                inputPin = endPin;
            }
            else if (IsInputPin(startPin) && IsOutputPin(endPin))
            {
                outputPin = endPin;
                inputPin = startPin;
            }

            if (inputPin && outputPin)
            {
                EditorNode* inputNode = FindNodeByPin(inputPin);
                EditorNode* outputNode = FindNodeByPin(outputPin);

                bool canCreate = true;

                if (!inputNode || !outputNode)
                    canCreate = false;
                else if (inputNode == outputNode)
                    canCreate = false;
                else if (!CanNodeHaveChildren(outputNode->type))
                    canCreate = false;
                else if (inputNode->isRoot)
                    canCreate = false;

                if (canCreate)
                {
                    for (const auto& link : _links)
                    {
                        if (link.endPin == inputPin)
                        {
                            canCreate = false;
                            break;
                        }
                    }
                }

                if (canCreate)
                {
                    for (const auto& link : _links)
                    {
                        if (link.startPin == outputPin)
                        {
                            canCreate = false;
                            break;
                        }
                    }
                }

                if (!canCreate)
                {
                    ed::RejectNewItem();
                }
                else if (ed::AcceptNewItem())
                {
                    EditorLink link;
                    link.id = ed::LinkId(GenerateID());
                    link.startPin = outputPin;
                    link.endPin = inputPin;
                    _links.push_back(link);
                }
            }
            else
            {
                ed::RejectNewItem();
            }
        }

        ed::PinId newNodePinId = 0;
        if (ed::QueryNewNode(&newNodePinId))
            ed::RejectNewItem();
    }

    ed::EndCreate();
}

uint64_t BT_View::GenerateID()
{
return _idCounter++;
}


BT_View::EditorNode* BT_View::FindNodeByPin(ed::PinId pin)
{
    for (auto& node : _nodes)
    {
        if (node.inputPin == pin)
            return &node;

        for (const auto& outputPin : node.outputPins)
        {
            if (outputPin == pin)
                return &node;
        }
    }

    return nullptr;
}

int BT_View::FindOutputPinIndex(const EditorNode& node, ed::PinId pin) const
{
    for (size_t i = 0; i < node.outputPins.size(); ++i)
    {
        if (node.outputPins[i] == pin)
            return static_cast<int>(i);
    }

    return -1;
}

bool BT_View::CanNodeHaveChildren(NodeType type) const
{
    return type == NodeType::Selector
        || type == NodeType::Sequence
        || type == NodeType::Parallel
        || type == NodeType::Decorator;
}

bool BT_View::IsInputPin(ed::PinId pin) const
{
    for (const auto& node : _nodes)
    {
        if (node.inputPin == pin)
            return true;
    }

    return false;
}

bool BT_View::IsOutputPin(ed::PinId pin) const
{
    for (const auto& node : _nodes)
    {
        if (!CanNodeHaveChildren(node.type))
            continue;

        for (const auto& outputPin : node.outputPins)
        {
            if (outputPin == pin)
                return true;
        }
    }

    return false;
}

std::string BT_View::Build_SavePath(const std::string& fileName) const
{
    std::string finalName = fileName;

    if (finalName.empty())
        finalName = "BT_Test";

    if (finalName.size() < 5 || finalName.substr(finalName.size() - 5) != ".json")
        finalName += ".json";

    return _path + finalName;
}

void BT_View::Connect_Runtime(ed::PinId startPin, ed::PinId endPin)
{
    EditorNode* parent = FindNodeByPin(startPin);
    EditorNode* child = FindNodeByPin(endPin);

    if (!parent || !child)
        return;

    //if (!parent->runtimeNode || !child->runtimeNode)
    //    return;

    //parent->runtimeNode->children.push_back(child->runtimeNode);
}
void BT_View::Handle_DeleteLink()
{
    int linkCount = ed::GetSelectedObjectCount();

    if (linkCount <= 0)
        return;

    std::vector<ed::LinkId> selectedLinks(linkCount);
    int actual = ed::GetSelectedLinks(selectedLinks.data(), linkCount);

    if (ImGui::IsKeyPressed(ImGuiKey_Delete))
    {
        for (int i = 0; i < actual; i++)
        {
            ed::LinkId linkId = selectedLinks[i];

            auto it = std::remove_if(_links.begin(), _links.end(),
                [linkId](const EditorLink& link)
                {
                    return link.id == linkId;
                });

            _links.erase(it, _links.end());
        }
    }
}

void BT_View::Handle_DeleteNode()
{
    if (ed::BeginDelete())
    {
        ed::NodeId nodeId;

        while (ed::QueryDeletedNode(&nodeId))
        {
            EditorNode* target = FindNodeById(nodeId);
            if (!target)
                continue;

            const bool wasRoot = target->isRoot;
            ed::PinId inputPin = target->inputPin;

           
            _links.erase(
                std::remove_if(_links.begin(), _links.end(),
                    [inputPin, target](const EditorLink& link)
                    {
                        if (link.endPin == inputPin)
                            return true;

                        for (const auto& outputPin : target->outputPins)
                        {
                            if (link.startPin == outputPin)
                                return true;
                        }

                        return false;
                    }),
                _links.end()
            );

            
            _nodes.erase(
                std::remove_if(_nodes.begin(), _nodes.end(),
                    [nodeId](const EditorNode& node)
                    {
                        return node.id == nodeId;
                    }),
                _nodes.end()
            );

            if (wasRoot)
            {
                EditorNode* newRoot = nullptr;

                for (auto& node : _nodes)
                {
                    bool hasParent = false;

                    for (const auto& link : _links)
                    {
                        if (link.endPin == node.inputPin)
                        {
                            hasParent = true;
                            break;
                        }
                    }

                    if (!hasParent)
                    {
                        newRoot = &node;
                        break;
                    }
                }

                SetRootNode(newRoot);
            }
        }

        ed::EndDelete();
    }
}

void BT_View::Handle_CreateNodePopup()
{
    ed::Suspend();

    if (ed::ShowBackgroundContextMenu())
    {
        _popupPos = ed::ScreenToCanvas(ImGui::GetMousePos());
        ImGui::OpenPopup("Create Node");
    }

    if (ImGui::BeginPopup("Create Node"))
    {
        if (ImGui::MenuItem("Selector"))
            Create_Node(NodeType::Selector);

        if (ImGui::MenuItem("Sequence"))
            Create_Node(NodeType::Sequence);

        if (ImGui::MenuItem("Parallel"))
            Create_Node(NodeType::Parallel);

        map<string, vector<string>> groupedNodes;

        GAME->Visit_BTNodeTypes([&](const TypeRegistry::BTNodeTypeInfo& info)
            {
                if (info.nodeCategory != "Task" && info.nodeCategory != "Decorator")
                    return;

                groupedNodes[info.nodeCategory].push_back(info.className);
            });

        for (auto& [category, classNames] : groupedNodes)
        {
            if (ImGui::BeginMenu(category.c_str()))
            {
                for (const auto& className : classNames)
                {
                    if (ImGui::MenuItem(className.c_str()))
                        Create_Node(className, category);
                }

                ImGui::EndMenu();
            }
        }

        ImGui::EndPopup();
    }

    ed::Resume();
}

BT_View::EditorNode* BT_View::FindNodeById(ed::NodeId id)
{
    for (auto& node : _nodes)
    {
        if (node.id == id)
            return &node;
    }
    return nullptr;
}
void BT_View::Update_NodePositions()
{
    for (auto& node : _nodes)
    {
        node.position = ed::GetNodePosition(node.id);
    }
}

void BT_View::EnsureOutputPins(EditorNode& node, size_t desiredCount)
{
    if (!CanNodeHaveChildren(node.type))
    {
        node.outputPins.clear();
        return;
    }

    while (node.outputPins.size() < desiredCount)
        node.outputPins.push_back(ed::PinId(GenerateID()));
}

size_t BT_View::GetDefaultOutputPinCount(NodeType type) const
{
    switch (type)
    {
    case NodeType::Decorator:
        return 1;

    case NodeType::Selector:
    case NodeType::Sequence:
    case NodeType::Parallel:
        return 2;

    default:
        return 0;
    }
}

const char* BT_View::GetOutputPinLabel(const EditorNode& node, size_t pinIndex) const
{
    static thread_local string label;

    if (node.type == NodeType::Parallel && pinIndex == 0)
        label = "Main";
    else
        label = "Child " + std::to_string(pinIndex);

    return label.c_str();
}



void BT_View::Draw_Blackboard()
{
    static char newKeyName[64] = "playerPos";
    static Engine::BlackboardValueType newKeyType = Engine::BlackboardValueType::Vec3;

    ImGui::TextUnformatted("Blackboard");
    ImGui::Separator();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 3.f));

    ImGui::TextDisabled("New");
    ImGui::SameLine(40.f);

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.48f);
    ImGui::InputText("##NewBlackboardKeyName", newKeyName, sizeof(newKeyName));

    ImGui::SameLine();

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.42f);
    if (ImGui::BeginCombo("##NewBlackboardKeyType", BlackboardTypeToString(newKeyType)))
    {
        if (ImGui::Selectable("Bool", newKeyType == Engine::BlackboardValueType::Bool))
            newKeyType = Engine::BlackboardValueType::Bool;
        if (ImGui::Selectable("Int", newKeyType == Engine::BlackboardValueType::Int))
            newKeyType = Engine::BlackboardValueType::Int;
        if (ImGui::Selectable("Float", newKeyType == Engine::BlackboardValueType::Float))
            newKeyType = Engine::BlackboardValueType::Float;
        if (ImGui::Selectable("Vec3", newKeyType == Engine::BlackboardValueType::Vec3))
            newKeyType = Engine::BlackboardValueType::Vec3;

        ImGui::EndCombo();
    }

    ImGui::SameLine();

    if (ImGui::SmallButton("+"))
    {
        string keyName = newKeyName;
        const size_t first = keyName.find_first_not_of(" \t");
        if (first == string::npos)
            keyName.clear();
        else
        {
            const size_t last = keyName.find_last_not_of(" \t");
            keyName = keyName.substr(first, last - first + 1);
        }

        if (!keyName.empty())
        {
            BlackboardKey key;
            key.name = keyName;
            strcpy_s(key.nameBuffer, key.name.c_str());
            key.type = newKeyType;
            _blackboardKeys.push_back(key);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    for (size_t i = 0; i < _blackboardKeys.size(); ++i)
    {
        BlackboardKey& key = _blackboardKeys[i];

        ImGui::PushID(static_cast<int>(i));

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.52f);
        if (ImGui::InputText("##BlackboardKeyName", key.nameBuffer, sizeof(key.nameBuffer)))
            key.name = key.nameBuffer;

        ImGui::SameLine();

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.34f);
        if (ImGui::BeginCombo("##BlackboardKeyType", BlackboardTypeToString(key.type)))
        {
            if (ImGui::Selectable("Bool", key.type == Engine::BlackboardValueType::Bool))
                key.type = Engine::BlackboardValueType::Bool;
            if (ImGui::Selectable("Int", key.type == Engine::BlackboardValueType::Int))
                key.type = Engine::BlackboardValueType::Int;
            if (ImGui::Selectable("Float", key.type == Engine::BlackboardValueType::Float))
                key.type = Engine::BlackboardValueType::Float;
            if (ImGui::Selectable("Vec3", key.type == Engine::BlackboardValueType::Vec3))
                key.type = Engine::BlackboardValueType::Vec3;

            ImGui::EndCombo();
        }

        ImGui::SameLine();

        if (ImGui::SmallButton("X"))
        {
            _blackboardKeys.erase(_blackboardKeys.begin() + i);
            ImGui::PopID();
            break;
        }

        ImGui::PopID();
    }

    ImGui::PopStyleVar();
}

const char* BT_View::BlackboardTypeToString(Engine::BlackboardValueType type) const
{
     switch (type)
     {
     case Engine::BlackboardValueType::Bool:  return "Bool";
     case Engine::BlackboardValueType::Int:   return "Int";
     case Engine::BlackboardValueType::Float: return "Float";
     case Engine::BlackboardValueType::Vec3:  return "Vec3";
     default:                         return "Unknown";
     }
}

Shared<BT_View> BT_View::Create()
{


    Shared<BT_View> pInstance =   make_shared<BT_View>();
    if(FAILED(pInstance->Initialize()))
    {
        LOG_ERROR("FAILED to Init BT_View");
    }
    return pInstance;
}

void BT_View::Free()
{
    if (_editorContext != nullptr)
    {
        ed::SetCurrentEditor(nullptr);
        ed::DestroyEditor(_editorContext);
        _editorContext = nullptr;
    }
    _nodes.clear();

    __super::Free();
}
