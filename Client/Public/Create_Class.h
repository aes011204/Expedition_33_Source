#pragma once

#include "Client_Defines.h"

NS_BEGIN(Engine)
class AnimNotify;
class AnimNotifyState;
class BNode;
class Component;
class GameObject;
NS_END

NS_BEGIN(Client)

class Create_Class final
{
public:
    struct ActorDesc
    {
        string  staticClass{};
        uint32  levelIndex = ETOI(LevelType::GamePlay);
        wstring layerTag = TEXT("Layer_GameObject");
        json    properties = json::object();
        json    transformOverrides = json::object();
        void*   initializeArg = nullptr;
    };

    struct InstanceDesc
    {
        string  staticClass{};
        json    properties = json::object();
        void*   initializeArg = nullptr;
    };

    class ActorBuilder
    {
    public:
        explicit ActorBuilder(const string& staticClass);

    public:
        ActorBuilder& AtLevel(uint32 levelIndex);
        ActorBuilder& InLayer(const wstring& layerTag);
        ActorBuilder& InitializeArg(void* arg);

        ActorBuilder& Position(const Vec3& position);
        ActorBuilder& Rotation(const Vec3& eulerDegree);
        ActorBuilder& Scale(const Vec3& scale);

        ActorBuilder& Property(const string& propertyName, const json& value);

        Shared<GameObject> Create() const;
        Shared<GameObject> Spawn() const;

    private:
        ActorDesc _desc{};
    };

    class ComponentBuilder
    {
    public:
        explicit ComponentBuilder(const string& staticClass);

    public:
        ComponentBuilder& InitializeArg(void* arg);
        ComponentBuilder& Property(const string& propertyName, const json& value);

        Shared<Component> Create() const;

    private:
        InstanceDesc _desc{};
    };

    class BTNodeBuilder
    {
    public:
        explicit BTNodeBuilder(const string& staticClass);

    public:
        BTNodeBuilder& Property(const string& propertyName, const json& value);

        Shared<BNode> Create() const;

    private:
        InstanceDesc _desc{};
    };

    class NotifyBuilder
    {
    public:
        explicit NotifyBuilder(const string& staticClass);

    public:
        NotifyBuilder& Property(const string& propertyName, const json& value);

        Shared<AnimNotify> Create() const;

    private:
        InstanceDesc _desc{};
    };

    class NotifyStateBuilder
    {
    public:
        explicit NotifyStateBuilder(const string& staticClass);

    public:
        NotifyStateBuilder& Property(const string& propertyName, const json& value);

        Shared<AnimNotifyState> Create() const;

    private:
        InstanceDesc _desc{};
    };

public:
    // GameObject
    static ActorBuilder         Actor(const string& staticClass) { return ActorBuilder(staticClass); }
    // Component
    static ComponentBuilder     Component(const string& staticClass) { return ComponentBuilder(staticClass); }
    // BTNode
    static BTNodeBuilder        BTNode(const string& staticClass) { return BTNodeBuilder(staticClass); }
    // Notify
    static NotifyBuilder        Notify(const string& staticClass) { return NotifyBuilder(staticClass); }
    // NotifyState
    static NotifyStateBuilder   NotifyState(const string& staticClass) { return NotifyStateBuilder(staticClass); }

    template<typename T>
    static ActorBuilder Actor()
    {
        return ActorBuilder(T::StaticClassNameA());
    }

    template<typename T>
    static ComponentBuilder Component()
    {
        return ComponentBuilder(T::StaticClassNameA());
    }

    template<typename T>
    static BTNodeBuilder BTNode()
    {
        return BTNodeBuilder(T::StaticClassNameA());
    }

    template<typename T>
    static NotifyBuilder Notify()
    {
        return NotifyBuilder(T::StaticClassNameA());
    }

    template<typename T>
    static NotifyStateBuilder NotifyState()
    {
        return NotifyStateBuilder(T::StaticClassNameA());
    }

    static Shared<GameObject> SpawnActor(
        const string& staticClass,
        uint32 levelIndex,
        const wstring& layerTag);

    static Shared<GameObject> SpawnActorAt(
        const string& staticClass,
        uint32 levelIndex,
        const wstring& layerTag,
        const Vec3& position);

private:
    static void Apply_TransformOverrides(
        const Shared<GameObject>& gameObject,
        const json& overrides);

    static Shared<GameObject> Spawn_ActorImplementation(const ActorDesc& desc);
    // Engine 안붙이면 위에 Builder 랑 이름겹침
    static Shared<Engine::Component> Create_ComponentImplementation(const InstanceDesc& desc);
    static Shared<BNode> Create_BTNodeImplementation(const InstanceDesc& desc);
    static Shared<AnimNotify> Create_NotifyImplementation(const InstanceDesc& desc);
    static Shared<AnimNotifyState> Create_NotifyStateImplementation(const InstanceDesc& desc);
};

NS_END
