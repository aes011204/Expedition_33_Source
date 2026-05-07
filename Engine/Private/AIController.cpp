#include "AIController.h"
#include "Service.h"
#include "PathFollower.h"
#include "BehaviorTree.h"
#include "BlackBoard.h"
#include "Decorator.h"
#include "GameObject.h"
#include "Selector.h"
#include "Sequence.h"

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

    if (!_behaviorTreeAssetPath.empty())
        CHECK_FAILED(Load_BehaviorTreeAsset(), E_FAIL);

    return S_OK;
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

    if (_PF)
    {
        Vec3 pos = Get_Owner()->Get_Transform()->Get_Position();
        _PF->Update(timeDelta, pos, 1.5f);
        Get_Owner()->Get_Transform()->Set_Position(pos);
    }
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
    Vec3 pos = Get_Owner()->Get_Transform()->Get_Position();
    _PF->Request_Move(pos,target);

}

void AIController::Stop_Move()
{
    Vec3 pos = Get_Owner()->Get_Transform()->Get_Position();
    _PF->Stop_Move(pos);
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

    struct FLoadedNode
    {
        uint64 id = 0;
        uint64 inputPin = 0;
        uint64 outputPin = 0;
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
        loaded.outputPin = n["outputPin"].get<uint64>();
        loaded.runtimeNode = Create_RuntimeBTNode(n);

        if (!loaded.runtimeNode)
            continue;

        Restore_BTNodeProperties(loaded.runtimeNode, n);

        loadedNodes.emplace(loaded.id, loaded);
        inputPinToNodeId.emplace(loaded.inputPin, loaded.id);
        outputPinToNodeId.emplace(loaded.outputPin, loaded.id);
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
    for (auto& [id, loaded] : loadedNodes)
    {
        if (!childNodeIds.contains(id))
        {
            rootNode = loaded.runtimeNode;
            break;
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

    if (type == 0)
        return Selector::Create();

    if (type == 1)
        return Sequence::Create();

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

        case PropertyType::Enum:
            *reinterpret_cast<int*>(address) = nodeJson[key].get<int>();
            break;

        default:
            break;
        }
    }

    return S_OK;
}
