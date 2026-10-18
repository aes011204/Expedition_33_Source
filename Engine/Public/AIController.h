#pragma once

#include "Component.h"
#include "Delegate.h"

namespace Client
{
struct BattleChar;
}

NS_BEGIN(Engine)

class ENGINE_DLL AIController :
    public Component
{

    GENERATED_COMPONENT(AIController)
public:

    AIController(const ComPtr<Device>& device, const ComPtr<Context>& context);
    AIController(const AIController& prototype);
    virtual ~AIController();
public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void BeginPlay() override;
    void Update(float timeDelta);
  
public:
    void On_EditorStopRuntime();
    const string& Get_BehaviorTreeAssetPath() const { return _behaviorTreeAssetPath; }
    void Set_BehaviorTreeAssetPath(const string& path) { _behaviorTreeAssetPath = path; }
    shared_ptr<class BlackBoard> Get_BlackBoard() { return _BB; }
    shared_ptr<class BehaviorTree> Get_BehaviorTree() { return _BT; }
    void Run_BT_Once();

    void Set_BehaviorTree(shared_ptr<class BehaviorTree> tree);

    bool Is_Finished();
    void Request_Move(Vec3& target);
    void Stop_Move();
    HRESULT Load_BehaviorTreeAsset();

    HRESULT Reload_BehaviorTree();
    void From_Json(const json& data);

    bool Get_IsMoving() { return _isMoving; };
    bool Has_MoveIntent() const { return _isMoving && _moveWorldDir.LengthSquared() > 0.f; }
    const Vec3& Get_MoveWorldDir() const { return _moveWorldDir; }
protected:
    shared_ptr<class BlackBoard> _BB = {};
    shared_ptr<class BehaviorTree> _BT = {};

    shared_ptr<class PathFollower> _PF = {};

    vector<shared_ptr<class Service>> _services;

    string _behaviorTreeAssetPath = {};

    Vec3 _testPos = {};

    bool _isMoving = {};
    Vec3 _moveWorldDir = Vec3::Zero;
    FDelegateHandle _editorStopHandle = {};

    static Shared<AIController> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<Component> Clone(void* arg) ;
    virtual void Free() override;
private:
    Shared<class BehaviorTree> Instantiate_BehaviorTreeJson(const fs::path& path);
    Shared<class BehaviorTree> Instantiate_BehaviorTreeJson(const string& path);

    Shared<class BNode> Create_RuntimeBTNode(const json& nodeJson) const;
    HRESULT Restore_BTNodeProperties(const Shared<class BNode>& node, const json& nodeJson) const;

};

NS_END
