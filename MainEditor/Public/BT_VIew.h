#pragma once
#include"Editor_Window.h"

namespace Engine
{
    class BehaviorTree;
    class BNode;
}
namespace ed = ax::NodeEditor;

NS_BEGIN(MainEditor)


    class BT_View : public Editor_Window
{
public:


    enum class NodeType { Selector, Sequence, Task, Decorator };
    enum class PinType { Input, Output };

    struct EditorNode
    {

        NodeType type;
        ed::NodeId id;

        ed::PinId inputPin;
        ed::PinId outputPin;

        PinType inputType = PinType::Input;
        PinType outputType = PinType::Output;

        std::string title;

        ImVec2 position;

        char titleBuffer[128];

        std::string className;
        std::string category;
        bool isRuntimeBTNode = false;

        Shared<Engine::BNode> runtimeNode = nullptr;
    };
   

    struct EditorLink
    {
        ed::LinkId id;

        ed::PinId startPin;
        ed::PinId endPin;
    };


public:
    BT_View();
    virtual ~BT_View();


public:
    HRESULT Initialize() override;
    void    Update(float timeDelta) override;
    void Update_SelectedNode();
    void Recreate_EditorContext();
    //  void    Pre_Render() override;
    void    Render() override;

    uint64_t _idCounter = 1;

    uint64_t GenerateID();

    void Inspactor();

    void Create_Node(NodeType type);
    void Create_Node(const string& className, const string& category);
    void Create_NewBT();

    void Save_BT(const std::string& path);
    void Load_BT(const std::string& path);

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
    bool CanNodeHaveChildren(NodeType type) const;
    bool IsInputPin(ed::PinId pin) const;
    bool IsOutputPin(ed::PinId pin) const;
    std::string Build_SavePath(const std::string& fileName) const;

private: // save , load
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
private:// Editor
    ax::NodeEditor::EditorContext* _editorContext = {};

    vector<EditorNode> _nodes;
    vector<EditorLink> _links;

    bool _openNodePopup = false;
    ImVec2 _popupPos;

    ed::NodeId _selectedNodeId;

  
private:// BT

    Weak<BehaviorTree> _debugTarget;
    Weak<BehaviorTree> _pendingDebugTarget;
public:
    static Shared<BT_View> Create();
    virtual void Free() override;
};



NS_END
