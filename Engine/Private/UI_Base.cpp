#include "UI_Base.h"

UI_Base::UI_Base()
{
}

UI_Base::UI_Base(const UI_Base& Prototype)
    : _isOpen{ false }
    , _sortOrder{ Prototype._sortOrder }
{
    Set_Name(Prototype.Get_Name());

    if (nullptr != Prototype._transform)
        _transform = make_unique<UI_Transform>(*Prototype._transform);
}

HRESULT UI_Base::Initialize_Prototype()
{
    _transform = UI_Transform::Create();
    CHECK_NULL(_transform, E_FAIL);

    return S_OK;
}

HRESULT UI_Base::Initialize(void* Arg)
{
    UI_DESC Desc{};

    if (nullptr != Arg)
        Desc = *static_cast<UI_DESC*>(Arg);

    _sortOrder = Desc.sortOrder;
    _isOpen = Desc.isOpenOnCreate;

    _transform = UI_Transform::Create(&Desc.transformDesc);
    CHECK_NULL(_transform, E_FAIL);

    return S_OK;
}

void UI_Base::Open()
{
    _isOpen = true;
}

void UI_Base::Close()
{
    _isOpen = false;
}

void UI_Base::Update(float TimeDelta)
{
}

HRESULT UI_Base::Render()
{
    return S_OK;
}

void UI_Base::Free()
{
    if (nullptr != _transform)
    {
        _transform->Free();
        _transform.reset();
    }

    __super::Free();
}
