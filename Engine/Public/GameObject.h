#pragma once

#include "TransformCom.h"
#include "GameInstance.h"
#include <type_traits>

NS_BEGIN(Engine)

class ColliderCom;

class ENGINE_DLL GameObject abstract : public Base
{
public:
    typedef struct tagGameObjectDesc : public TransformCom::TRANSFORM_DESC
    {
        uint32		flag = {};
    }GAMEOBJECT_DESC;

public:
    GameObject(const ComPtr<Device>& device, const ComPtr<Context>& context);
    GameObject(const GameObject& prototype);
    virtual ~GameObject() = default;

public:
    virtual HRESULT Initialize_Prototype();
    virtual HRESULT Initialize(void* arg);
    virtual void    BeginPlay();

    virtual void    Priority_Update(float timeDelta);
    virtual void    Update(float timeDelta);
    virtual void    Late_Update(float timeDelta);
    virtual HRESULT Render();

    HRESULT         Try_BeginPlay();

public:
    virtual void    OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) {}
    virtual void    OnStayOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)  {}
    virtual void    OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)   {}

    virtual void    OnBlockBegin(Shared<ColliderCom> self, Shared<ColliderCom> other) {}
    virtual void    OnBlockStay(Shared<ColliderCom> self, Shared<ColliderCom> other) {}

public:
    Shared<GameObject> Get_Owner() const { return _owner.lock(); }
    void Set_Owner(Shared<GameObject> owner) { _owner = owner; }

public:
    // 일반적으로 컴포넌트 가져올 때 = Get_Component<ShaderCom>(); 이런느낌
    template<typename T>
    Shared<T> Get_Component()
    {
        static_assert(std::is_base_of_v<Component, T>,
            "Get_Component<T>() requires T to derive from Component.");
    
        for (auto& [_, component] : _components)
        {
            auto casted = dynamic_pointer_cast<T>(component);
            if (casted != nullptr)
                return casted;
        }
    
        return nullptr;
    }
    // 한 객체 안에, 동일 컴포넌트가 여러 개 있을 때 -> Get_Component<ShaderCom>(TEXT("Shader_VtxAnimMesh"));
    template<typename T>
    Shared<T> Get_Component(const wstring& componentTag)
    {
        static_assert(std::is_base_of_v<Component, T>,
            "Get_Component<T>() requires T to derive from Component.");
    
        auto iter = _components.find(componentTag);
        if (iter == _components.end())
            return nullptr;
    
        return dynamic_pointer_cast<T>(iter->second);
    }

    // 트랜스폼은 자주 사용하니까
    Shared<TransformCom> Get_Transform() const { return _transformCom; }

public:
    template<typename T>
    HRESULT Add_Component(
        uint32 prototpyeLevelIndex, const wstring& prototypeTag, Shared<T>& outCom, void* arg = nullptr)
    {
        static_assert(std::is_base_of_v<Component, T>,
            "Add_Component<T>() requires T to derive from Component.");

        if (_components.end() != _components.find(prototypeTag))
            return E_FAIL;

        Shared<Component> component =
            dynamic_pointer_cast<Component>(
                GAME->Clone_Prototype(
                    Prototype::Comopnent,
                    prototpyeLevelIndex,
                    prototypeTag,
                    arg));

        if (nullptr == component)
            return E_FAIL;

        component->Set_Owner(this->GetSharedPtr<GameObject>());

        outCom = dynamic_pointer_cast<T>(component);
        if (nullptr == outCom)
            return E_FAIL;


        _components.emplace(prototypeTag, component);

        return S_OK;
    }

protected:
    ComPtr<Device>          _device = { nullptr };
    ComPtr<Context>         _context = { nullptr };
    Shared<TransformCom>    _transformCom = { nullptr };

    ObjectType              _objectType = ObjectType::None;

protected:
    map<const wstring, Shared<Component>> _components;

protected:
    Weak<GameObject> _owner;
    bool _hasBegunPlay = false;

public:
    virtual Shared<GameObject> Clone(void* arg) = 0;
    virtual void Free() override;
};

NS_END



