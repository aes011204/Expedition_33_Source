#pragma once

#include "Transform.h"
#include "GameInstance.h"
#include <type_traits>

NS_BEGIN(Engine)

class ENGINE_DLL GameObject abstract : public Base
{
public:
    typedef struct tagGameObjectDesc : public Transform::TRANSFORM_DESC
    {
        uint32		iFlag = {};
    }GAMEOBJECT_DESC;

public:
    GameObject(ComPtr<Device> device, ComPtr<Context> context);
    GameObject(const GameObject& Prototype);
    virtual ~GameObject() = default;

public:
    virtual HRESULT Initialize_Prototype();
    virtual HRESULT Initialize(void* pArg);
    virtual void    Priority_Update(float fTimeDelta);
    virtual void    Update(float fTimeDelta);
    virtual void    Late_Update(float fTimeDelta);
    virtual HRESULT Render();

public:
    // 일반적으로 컴포넌트 가져올 때 = Get_Component<Shader>(); 이런느낌
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
    // 한 객체 안에, 동일 컴포넌트가 여러 개 있을 때 -> Get_Component<Shader>(TEXT("Shader_VtxAnimMesh"));
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
    Shared<Transform> Get_Transform() const { return _transformCom; }

public:
    template<typename T>
    HRESULT Add_Component(
        uint32 prototpyeLevelIndex, const wstring& strPrototypeTag, Shared<T>& outCom, void* arg = nullptr)
    {
        static_assert(std::is_base_of_v<Component, T>,
            "Add_Component<T>() requires T to derive from Component.");

        if (_components.end() != _components.find(strPrototypeTag))
            return E_FAIL;

        Shared<Component> component =
            dynamic_pointer_cast<Component>(
                GAME->Clone_Prototype(
                    Prototype::Comopnent,
                    prototpyeLevelIndex,
                    strPrototypeTag,
                    arg));

        if (nullptr == component)
            return E_FAIL;

        outCom = dynamic_pointer_cast<T>(component);
        if (nullptr == outCom)
            return E_FAIL;

        _components.emplace(strPrototypeTag, component);

        return S_OK;
    }

protected:
    ComPtr<Device>      _device = { nullptr };
    ComPtr<Context>     _context = { nullptr };
    Shared<Transform>  _transformCom = { nullptr };

protected:
    map<const wstring, Shared<Component>>		_components;

public:
    virtual Shared<GameObject> Clone(void* pArg) = 0;
    virtual void Free() override;
};

NS_END



