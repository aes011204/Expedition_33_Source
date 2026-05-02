#include "GameObject.h"
#include "GameInstance.h"

GameObject::GameObject(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device { device }
    , _context { context }
{
}

GameObject::GameObject(const GameObject& prototype)
    : _device{ prototype._device }
    , _context{ prototype._context }
    , _hasBegunPlay(false)
{
}

HRESULT GameObject::Initialize_Prototype()
{
    return S_OK;
}

HRESULT GameObject::Initialize(void* arg)
{
    _transformCom = TransformCom::Create(_device, _context);

    if (nullptr == _transformCom)
        return E_FAIL;

    if (FAILED(_transformCom->Initialize(arg)))
        return E_FAIL;

    wstring transformTag = TEXT("Com_Transform");

    if (_components.end() == _components.find(transformTag))
        _components.emplace(transformTag, static_pointer_cast<Component>(_transformCom));

    return S_OK;
}

void GameObject::BeginPlay()
{

}

void GameObject::Priority_Update(float timeDelta)
{
}

void GameObject::Update(float timeDelta)
{
}

void GameObject::Late_Update(float timeDelta)
{
}

HRESULT GameObject::Render()
{
    return S_OK;
}

HRESULT GameObject::Try_BeginPlay()
{
    if (_hasBegunPlay)
        return S_OK;

    BeginPlay();
    _hasBegunPlay = true;

    return S_OK;
}

void GameObject::Free()
{
    __super::Free();

    _components.clear();
    _transformCom.reset();
}
