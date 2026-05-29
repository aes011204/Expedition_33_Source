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
    virtual HRESULT Render_Shadow() { return S_OK; }
    virtual HRESULT RenderPickingID(uint32 id) { return S_OK; }

    // Static Shadow 캐싱할 때 이 오브젝트가 차지하는 월드 Bounds 넘겨주기
    virtual bool Try_Get_ShadowWorldBounds(BoundingBox& outWorldBounds) const { return false;}

    HRESULT         Try_BeginPlay();

    virtual ClassReflectionInfo& Get_ReflectionInfo()
    {
        static ClassReflectionInfo emptyInfo{};
        return emptyInfo;
    }

    virtual json    To_Json() const;
    virtual void    From_Json(const json& data);

    void            Set_SourcePrefabName(const string& prefabName) { _sourcePrefabName = prefabName; }
    const string&   Get_SourcePrefabName() const { return _sourcePrefabName; }

public:
    virtual void    OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) {}
    virtual void    OnStayOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)  {}
    virtual void    OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)   {}

    virtual void    OnBlockBegin(Shared<ColliderCom> self, Shared<ColliderCom> other) {}
    virtual void    OnBlockStay(Shared<ColliderCom> self, Shared<ColliderCom> other) {}

public:
    Shared<GameObject> Get_Owner() const { return _owner.lock(); }
    void Set_Owner(Shared<GameObject> owner) { _owner = owner; }
    bool Rename_ComponentTag(const wstring& oldTag, const wstring& newTag);

    virtual ObjectType Get_ObjectType() const { return ObjectType::None; }

    bool Is_Visible() const { return _isVisible; }
    void Set_Visible(bool visible) { _isVisible = visible; }

public: /* 그림자 */
    void Set_ShadowCasterType(ShadowCasterType type) { _shadowCasterType = type; }
    ShadowCasterType Get_ShadowCasterType() const { return _shadowCasterType; }

    bool Casts_DynamicShadow() const { return _shadowCasterType == ShadowCasterType::Dynamic; }
    bool Casts_StaticShadow() const { return _shadowCasterType == ShadowCasterType::Static; }

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

    // 컴포넌트 순회용. 반환 ㄴ
    template<typename Visitor>
    void Visit_Components(Visitor&& visitor) const
    {
        for (const auto& [tag, component] : _components)
        {
            visitor(tag, component);
        }
    }

protected:
    ComPtr<Device>          _device = { nullptr };
    ComPtr<Context>         _context = { nullptr };
    Shared<TransformCom>    _transformCom = { nullptr };

    ObjectType              _objectType = ObjectType::None;
    ShadowCasterType        _shadowCasterType = ShadowCasterType::None;

    string                  _sourcePrefabName = {};

protected:
    map<const wstring, Shared<Component>> _components;

protected:
    Weak<GameObject> _owner;
    bool _hasBegunPlay = false;
    bool _isVisible = true;

public:
    virtual Shared<GameObject> Clone(void* arg) = 0;
    virtual void Free() override;
};

NS_END



