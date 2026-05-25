#include "AIController.h"
#include "Service.h"
#include "PathFollower.h"
#include "BehaviorTree.h"
#include "BlackBoard.h"
#include "Composite.h"
#include "Decorator.h"
#include "GameObject.h"
#include "Helper_String.h"
#include "Parallel.h"
#include "Selector.h"
#include "Sequence.h"

namespace Client
{
    class MW_Lancelier;
}

IMPLEMENT_REFLECTION(AIController)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_STRING("Behavior Tree Asset", _behaviorTreeAssetPath);

    return true;
}

AIController::AIController(const ComPtr<Device>& device, const ComPtr<Context>& context)
    :Component(device, context)
{
}

AIController::AIController(const AIController& prototype)
    : Component(prototype)
{
}

AIController::~AIController()
{
}

HRESULT AIController::Initialize_Prototype()
{
    return Component::Initialize_Prototype();
}

HRESULT AIController::Initialize(void* arg)
{
    _BB = BlackBoard::Create();
    _PF = PathFollower::Create();

    CHECK_NULL(_BB, E_FAIL);
    CHECK_NULL(_PF, E_FAIL);

    //if (!_behaviorTreeAssetPath.empty())
    //    CHECK_FAILED(Load_BehaviorTreeAsset(), E_FAIL);

    return S_OK;
}

void AIController::BeginPlay()
{
    Component::BeginPlay();

    if (!_editorStopHandle.IsValid())
    {
        _editorStopHandle = GAME->Get_DelegateHub().OnEditorStopRuntime.Add(this, &AIController::On_EditorStopRuntime);
    }
}

void AIController::On_EditorStopRuntime()
{
    Stop_Move();
}

void AIController::Update(float timeDelta)
{

    for (auto& service : _services)
    {
        if (service)
            service->Update(timeDelta);
    }

    if (_BT)
        _BT->Update(timeDelta);

        Vec3 pos = Get_Owner()->Get_Transform()->Get_Position();
    if (_PF)
    {
        auto tmp = Get_Owner()->Get_Transform()->Get_SpeedPerSec() ;

       //LOG_DEBUG("{}", tmp);
        _PF->Update(timeDelta, pos, tmp);
        Get_Owner()->Get_Transform()->Set_Position(pos);

    }

}


void AIController::From_Json(const json& data)
{
	__super::From_Json(data);

	if (!_behaviorTreeAssetPath.empty())
		Load_BehaviorTreeAsset();
}

void AIController::Run_BT_Once()
{
    
        if (_BT == nullptr)
            return;

        _BT->Update(0.f);
    
}

void AIController::Set_BehaviorTree(shared_ptr<BehaviorTree> tree)
{
    _BT = tree;

    if (_BT)
        _BT->Bind_Owner(static_pointer_cast<AIController>(shared_from_this()));
}

bool AIController::Is_Finished()
{
    return _PF->Is_Finished();

}

void AIController::Request_Move(Vec3& target)
{
    auto owner = Get_Owner();
    if (!owner || !_PF)
        return;

    Vec3 pos = Get_Owner()->Get_Transform()->Get_Position();
    _PF->Request_Move(pos,target);

    _isMoving = true;
}

void AIController::Stop_Move()
{
    auto owner = Get_Owner();
    if (!owner || !_PF)
        return;

    const Shared<TransformCom> transform = owner->Get_Transform();
    if (transform == nullptr)
        return; 

    Vec3 pos = transform->Get_Position();
    _PF->Stop_Move(pos);
    _isMoving = false;
}

HRESULT AIController::Load_BehaviorTreeAsset()
{
    if (_behaviorTreeAssetPath.empty())
        return S_FALSE;

    Shared<BehaviorTree> tree = Instantiate_BehaviorTreeJson(_behaviorTreeAssetPath);
    CHECK_NULL(tree, E_FAIL);

    Set_BehaviorTree(tree);
    return S_OK;
}

HRESULT AIController::Reload_BehaviorTree()
{
    return Load_BehaviorTreeAsset();
}

Shared<AIController> AIController::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<AIController>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : AIController");
        return nullptr;
    }

    return instance;
}

Shared<Component> AIController::Clone(void* arg)
{
    auto clone = make_shared<AIController>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : AIController");
        return nullptr;
    }

    return clone;
}
void AIController::Free()
{
    if (_editorStopHandle.IsValid())
    {
        GAME->Get_DelegateHub().OnEditorStopRuntime.Remove(_editorStopHandle);
        _editorStopHandle.Reset();
    }

    __super::Free();

}
Shared<BehaviorTree> AIController::Instantiate_BehaviorTreeJson(const fs::path& path)
{
    if (!fs::exists(path))
        return nullptr;

    std::ifstream file(path);
    if (!file.is_open())
        return nullptr;

    json root;
    file >> root;
    file.close();

    if (!root.is_object() || !root.contains("nodes"))
        return nullptr;

    uint64 rootNodeId = 0;
    if (root.contains("rootNodeId"))
        rootNodeId = root["rootNodeId"].get<uint64>();

    struct FLoadedNode
    {
        uint64 id = 0;
        uint64 inputPin = 0;
        vector<uint64> outputPins = {};
        Shared<BNode> runtimeNode = nullptr;
    };

    unordered_map<uint64, FLoadedNode> loadedNodes;
    unordered_map<uint64, uint64> inputPinToNodeId;
    unordered_map<uint64, uint64> outputPinToNodeId;
    unordered_set<uint64> childNodeIds;

    for (auto& n : root["nodes"])
    {
        FLoadedNode loaded{};
        loaded.id = n["id"].get<uint64>();
        loaded.inputPin = n["inputPin"].get<uint64>();

        if (n.contains("outputPins"))
        {
            for (auto& outputPin : n["outputPins"])
                loaded.outputPins.push_back(outputPin.get<uint64>());
        }
        else if (n.contains("outputPin"))
        {
            loaded.outputPins.push_back(n["outputPin"].get<uint64>());
        }

        loaded.runtimeNode = Create_RuntimeBTNode(n);

        if (!loaded.runtimeNode)
            continue;

        loaded.runtimeNode->Set_AssetNodeId(loaded.id);
        Restore_BTNodeProperties(loaded.runtimeNode, n);

        loadedNodes.emplace(loaded.id, loaded);
        inputPinToNodeId.emplace(loaded.inputPin, loaded.id);

        for (const uint64 outputPin : loaded.outputPins)
            outputPinToNodeId.emplace(outputPin, loaded.id);
    }

    for (auto& l : root["links"])
    {
        const uint64 startPin = l["start"].get<uint64>();
        const uint64 endPin = l["end"].get<uint64>();

        auto parentIt = outputPinToNodeId.find(startPin);
        auto childIt = inputPinToNodeId.find(endPin);

        if (parentIt == outputPinToNodeId.end() || childIt == inputPinToNodeId.end())
            continue;

        Shared<BNode> parent = loadedNodes[parentIt->second].runtimeNode;
        Shared<BNode> child = loadedNodes[childIt->second].runtimeNode;

        if (!parent || !child)
            continue;

        if (auto composite = dynamic_pointer_cast<Composite>(parent))
            composite->Add_Child(child);
        else if (auto decorator = dynamic_pointer_cast<Decorator>(parent))
            decorator->Set_Child(child);

        childNodeIds.insert(childIt->second);
    }

    Shared<BNode> rootNode = nullptr;

    if (rootNodeId != 0)
    {
        auto rootIt = loadedNodes.find(rootNodeId);
        if (rootIt != loadedNodes.end())
            rootNode = rootIt->second.runtimeNode;
    }

    if (rootNode == nullptr)
    {
        for (auto& [id, loaded] : loadedNodes)
        {
            if (!childNodeIds.contains(id))
            {
                rootNode = loaded.runtimeNode;
                break;
            }
        }
    }

    CHECK_NULL(rootNode, nullptr);

    Shared<BehaviorTree> tree = make_shared<BehaviorTree>();
    tree->Set_Root(rootNode);
    return tree;
}
Shared<BehaviorTree> AIController::Instantiate_BehaviorTreeJson(const string& path)
{
    return Instantiate_BehaviorTreeJson(fs::path(String::ToWString(path)));
}
Shared<BNode> AIController::Create_RuntimeBTNode(const json& nodeJson) const
{
    const string className = nodeJson.value("className", string{});
    const string title = nodeJson.value("title", string{});
    const int type = nodeJson.value("type", -1);

    if (!className.empty())
        return dynamic_pointer_cast<BNode>(GAME->Create_BTNodePrototype(className));

    if (title == "Selector")
        return Selector::Create();

    if (title == "Sequence")
        return Sequence::Create();

    if (title == "Decorator")
        return nullptr;

    if (title == "Parallel")
        return Parallel::Create();

    if (type == 0)
        return Selector::Create();

    if (type == 1)
        return Sequence::Create();

    if (type == 4)
        return Parallel::Create();

    return nullptr;
}
HRESULT AIController::Restore_BTNodeProperties(const Shared<BNode>& node, const json& nodeJson) const
{
    CHECK_NULL(node, E_FAIL);

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

        case PropertyType::WString:
            *static_cast<wstring*>(address) = String::ToWString(nodeJson[key].get<string>());
            break;

        case PropertyType::Enum:
            *reinterpret_cast<int*>(address) = nodeJson[key].get<int>();
            break;

        default:
            break;
        }
    }

    return S_OK;
}
