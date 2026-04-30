#include "GameObject.h"
#include "GameInstance.h"

GameObject::GameObject(ComPtr<Device> device, ComPtr<Context> context)
    : _device { device }
    , _context { context }
{
}

GameObject::GameObject(const GameObject& Prototype)
    : _device{ Prototype._device }
    , _context{ Prototype._context }
{
}

Component* GameObject::Get_Component(const wstring& strComponentTag)
{
    auto iter = _components.find(strComponentTag);

    if(iter == _components.end())
        return nullptr;

    return iter->second.get();
}

HRESULT GameObject::Initialize_Prototype()
{
    return S_OK;
}

HRESULT GameObject::Initialize(void* pArg)
{
    Shared<Transform> pTransform = Transform::Create(_device, _context);
    _transformCom = pTransform.get();
    if (nullptr == _transformCom)
        return E_FAIL;

    if (FAILED(_transformCom->Initialize(pArg)))
        return E_FAIL;

    if (nullptr == Get_Component(g_strTransformTag))
        _components.emplace(g_strTransformTag, static_pointer_cast<Component>(pTransform));

    return S_OK;
}

void GameObject::Priority_Update(float fTimeDelta)
{
}

void GameObject::Update(float fTimeDelta)
{
}

void GameObject::Late_Update(float fTimeDelta)
{
}

HRESULT GameObject::Render()
{
    return S_OK;
}

HRESULT GameObject::Add_Component(uint32 iPrototypeLevelIndex, const wstring& strPrototypeTag, const wstring& strComponentTag, Component** ppOut, void* pArg)
{
    if (nullptr != Get_Component(strComponentTag))
        return E_FAIL;

    Shared<Component> pComponent = static_pointer_cast<Component>(GAME->Clone_Prototype(Prototype::COMPONENT, iPrototypeLevelIndex, strPrototypeTag, pArg));
    if (nullptr == pComponent)
        return E_FAIL;

    _components.emplace(strComponentTag, pComponent);
    *ppOut = pComponent.get();

    return S_OK;
}

void GameObject::Free()
{
    __super::Free();
    _components.clear();
    _transformCom = nullptr;
}
