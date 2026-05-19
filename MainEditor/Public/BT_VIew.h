#pragma once

#include "Editor_Window.h"
#include "Reflection_Types.h"
#include <any>
#include <functional>
#include <unordered_map>

namespace Engine
{
    class AIController;
    class BehaviorTree;
    class BNode;
}

namespace ed = ax::NodeEditor;

NS_BEGIN(MainEditor)

class BT_View : public Editor_Window
{
public:
    enum class NodeType { Selector, Sequence, Task, Decorator, Parallel };
    enum class PinType { Input, Output };

    struct EditorNode
    {
        NodeType type;
        ed::NodeId id;

        ed::PinId inputPin;
        PinType inputType = PinType::Input;
        vector<ed::PinId> outputPins;

        std::string title;
        ImVec2 position;
        char titleBuffer[128];

        std::string className;
        std::string category;
        bool isRuntimeBTNode = false;

        Shared<Engine::BNode> runtimeNode = nullptr;

        bool isRoot = false;

    };

    struct EditorLink
    {
        ed::LinkId id;
        ed::PinId startPin;
        ed::PinId endPin;
    };

    struct BlackboardKey
    {
        std::string name;
        Engine::BlackboardValueType type = Engine::BlackboardValueType::Vec3;
        char nameBuffer[64] = {};
    };

    struct DebugNodeState
    {
        Engine::BTNodeState state = Engine::BTNodeState::FAILED;
        uint64 tick = 0;
    };

public:
    BT_View();
    virtual ~BT_View();

public:
    HRESULT Initialize() override;
    void Update(float timeDelta) override;
    void Update_SelectedNode();
    void Recreate_EditorContext();
    void Render() override;

    uint64_t _idCounter = 1;

    uint64_t GenerateID();

    void Inspactor();

    void Create_Node(NodeType type);
    void Create_Node(const string& className, const string& category);
    void Create_NewBT();

    void Save_BT(const std::string& path);
    void Load_BT(const std::string& path);
    void Open_BTAsset(const string& relativePath);
    void Set_DebugTargetAIController(const Shared<Engine::AIController>& aiController);

    void Draw_LoadPopup();
    void Draw_SavePopup();
    void Draw_Nodes();
    void Draw_ToolBar();
    void Draw_BT();
    void Draw_Links();
    void Refresh_BT_FileList();

    void Handle_CreateLink();
    void Connect_Runtime(ed::PinId startPin, ed::PinId endPin);
    void Handle_DeleteLink();
    void Handle_DeleteNode();
    void Handle_CreateNodePopup();
    EditorNode* FindNodeById(ed::NodeId id);
    void Update_NodePositions();
    EditorNode* FindNodeByPin(ed::PinId pin);
    int FindOutputPinIndex(const EditorNode& node, ed::PinId pin) const;
    bool CanNodeHaveChildren(NodeType type) const;
    bool IsInputPin(ed::PinId pin) const;
    bool IsOutputPin(ed::PinId pin) const;
    string Build_SavePath(const string& fileName) const;

    void Draw_Blackboard();
    void Draw_BlackboardRuntimeValues();
    string FormatBlackboardValue(Engine::BlackboardValueType type, const std::any* value) const;
    const char* BlackboardTypeToString(Engine::BlackboardValueType type) const;

    EditorNode* FindRootNode();
    void SetRootNode(EditorNode* newRoot);
    void EnsureOutputPins(EditorNode& node, size_t desiredCount);
    size_t GetDefaultOutputPinCount(NodeType type) const;
    const char* GetOutputPinLabel(const EditorNode& node, size_t pinIndex) const;
private: // save, load
    bool _openLoadPopup = false;
    bool _openSavePopup = false;
    bool _openOverwritePopup = false;

    std::vector<string> _btFiles;
    char _saveFileName[256] = "BT_Test";
    std::string _pendingSavePath = {};
    std::string _currentFileName = "Untitled";
    bool _pendingContextRebuild = false;
    bool _needsInitialNodePlacement = false;

    string _path = "../../../Client/Bin/Resources/Data/json/BT/";

private: // Editor
    ax::NodeEditor::EditorContext* _editorContext = {};

    vector<EditorNode> _nodes;
    vector<EditorLink> _links;

    bool _openNodePopup = false;
    ImVec2 _popupPos;
    ed::NodeId _selectedNodeId;

    vector<BlackboardKey> _blackboardKeys;

private: // BT
    Weak<Engine::AIController> _debugAIController;
    unordered_map<uint64, DebugNodeState> _debugNodeStates;
    uint64 _currentDebugTreeTick = 0;

private:
    void Draw_DebugTargetSelector();
    void Refresh_DebugState();
    void Reload_AIControllersUsingBT(const string& savedPath);
    void Visit_RuntimeNode(
        const Shared<Engine::BNode>& node,
        const function<void(const Shared<Engine::BNode>&)>& visitor);

public:
    static Shared<BT_View> Create();
    virtual void Free() override;
};

NS_END
