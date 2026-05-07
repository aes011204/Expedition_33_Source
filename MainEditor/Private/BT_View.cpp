
#include "BT_View.h"
#include "EditorInstance.h"
#include "TypeRegistry.h"
#include "GameInstance.h"
#include "BNode.h"
#include "Inspector.h"


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

    if (node->runtimeNode != nullptr)
    {
        Engine::ClassReflectionInfo& info = node->runtimeNode->Get_ReflectionInfo();
        Inspector::Draw_ReflectionSection(info.className, node->runtimeNode.get(), info);
        return;
    }

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
        static bool flag = true;
        ImGui::Checkbox("Condition", &flag);
        break;
    }

    default:
        break;
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
    node.outputPin = ed::PinId(GenerateID());

    node.type = type;


    switch (type)
    {
    case NodeType::Selector:
        node.title = "Selector";
        break;
    case NodeType::Sequence:
        node.title = "Sequence";
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

    _nodes.push_back(node);


    ed::SetNodePosition(node.id, _popupPos);
}

void BT_View::Create_Node(const string& className, const string& category)
{
    EditorNode node;

    node.id = ed::NodeId(GenerateID());
    node.inputPin = ed::PinId(GenerateID());
    node.outputPin = ed::PinId(GenerateID());

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

    node.position = _popupPos;

    node.runtimeNode = dynamic_pointer_cast<Engine::BNode>(
        GAME->Create_BTNodePrototype(className));

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
        n["outputPin"] = (uint64_t)node.outputPin.Get();

        n["className"] = node.className;
        n["category"] = node.category;
        n["isRuntimeBTNode"] = node.isRuntimeBTNode;

        j["nodes"].push_back(n);
    }

    for (auto& link : _links)
    {
        json l;

        l["id"] = (uint64_t)link.id.Get();
        l["start"] = (uint64_t)link.startPin.Get();
        l["end"] = (uint64_t)link.endPin.Get();

        j["links"].push_back(l);
    }

    fs::create_directories(fs::path(path).parent_path());

    std::ofstream file(path);
    if (!file.is_open())
        return;

    file << j.dump(4);
    file.close();

    _currentFileName = fs::path(path).filename().string();
    strcpy_s(_saveFileName, _currentFileName.c_str());

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
        node.outputPin = ed::PinId(n["outputPin"].get<uint64_t>());

        strcpy_s(node.titleBuffer, node.title.c_str());

        node.className = n.value("className", std::string{});

        if (!node.className.empty())
        {
            node.runtimeNode = dynamic_pointer_cast<Engine::BNode>(
                GAME->Create_BTNodePrototype(node.className));
        }

        node.category = n.value("category", std::string{});
        node.isRuntimeBTNode = n.value("isRuntimeBTNode", false);

        _nodes.push_back(node);
    }

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
        _idCounter = max(_idCounter, (uint64_t)node.outputPin.Get() + 1);
    }

    for (auto& link : _links)
    {
        _idCounter = max(_idCounter, (uint64_t)link.id.Get() + 1);
    }

    _currentFileName = fs::path(path).filename().string();
    strcpy_s(_saveFileName, _currentFileName.c_str());




    _pendingContextRebuild = true;
    _needsInitialNodePlacement = true;
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

        if (isSelected)
            ed::PushStyleColor(ed::StyleColor_NodeBg, ImColor(80, 80, 150));

        ed::BeginNode(node.id);

        ImGui::Text("%s", node.title.c_str());
        ImGui::Spacing();

        ed::BeginPin(node.inputPin, ed::PinKind::Input);
        ImGui::Text("-> In");
        ed::EndPin();

        if (CanNodeHaveChildren(node.type))
        {
            ed::BeginPin(node.outputPin, ed::PinKind::Output);
            ImGui::Text("Out ->");
            ed::EndPin();
        }

        ed::EndNode();

        if (isSelected)
            ed::PopStyleColor();
    }

    firstFrame = false;
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
        if (node.inputPin == pin || node.outputPin == pin)
            return &node;
    }

    return nullptr;
}

bool BT_View::CanNodeHaveChildren(NodeType type) const
{
    return type == NodeType::Selector
        || type == NodeType::Sequence
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
        if (node.outputPin == pin && CanNodeHaveChildren(node.type))
            return true;
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

            ed::PinId inputPin = target->inputPin;
            ed::PinId outputPin = target->outputPin;

           
            _links.erase(
                std::remove_if(_links.begin(), _links.end(),
                    [inputPin, outputPin](const EditorLink& link)
                    {
                        return (link.startPin == outputPin ||
                            link.endPin == inputPin);
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
