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

HRESULT GameObject::Initialize_Prototype()
{
    return S_OK;
}

HRESULT GameObject::Initialize(void* pArg)
{
    _transformCom = Transform::Create(_device, _context);

    if (nullptr == _transformCom)
        return E_FAIL;

    if (FAILED(_transformCom->Initialize(pArg)))
        return E_FAIL;

    wstring strTransfrom = TEXT("Com_Transform");

    if (_components.end() == _components.find(strTransfrom))
        _components.emplace(strTransfrom, static_pointer_cast<Component>(_transformCom));

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

void GameObject::Free()
{
    __super::Free();

    _components.clear();
    _transformCom.reset();
}
