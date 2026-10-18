#include "UI_SL_LockOn.h"

#include "Character.h"
#include "Client_Constants.h"
#include "SL_Dualliste.h"
#include "SL_Maelle.h"
#include "SL_TargetCom.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_SL_LockOn::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Clear_Target();

    return S_OK;
}

void UI_SL_LockOn::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void UI_SL_LockOn::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_SL_LockOn::Close()
{
    UI_RuntimePanelBase::Close();
}

void UI_SL_LockOn::Update_Target(const Shared<SL_Maelle>& player)
{
    Vec2 screenPos{};
    if (!Try_ResolveLockOnScreenPosition(player, screenPos))
    {
        Close();
        return;
    }

    Apply_EditorOffsets(screenPos);

    Open();

    if (_lockOnImage && _lockOnImage->Get_Transform())
        _lockOnImage->Get_Transform()->Set_Position(screenPos);
}

void UI_SL_LockOn::Clear_Target()
{
    Close();
}

HRESULT UI_SL_LockOn::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_SL_LockOn_Path;
    desc.prototypeTagPrefix = L"SLLockOn";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    return S_OK;
}

HRESULT UI_SL_LockOn::Bind_UI()
{
    _lockOnImage = Find_UI(_uiByName, L"Image_LockOn");

    CHECK_NULL(_lockOnImage, E_FAIL);

    _root = _lockOnImage;

    return S_OK;
}

bool UI_SL_LockOn::Try_ResolveLockOnScreenPosition(const Shared<SL_Maelle>& player, Vec2& outScreenPos) const
{
    if (player == nullptr)
        return false;

    const Shared<SL_TargetCom> targetCom = player->Get_Component<SL_TargetCom>();
    if (targetCom == nullptr || !targetCom->Is_LockOn())
        return false;

    const Shared<Character> lockedTarget = targetCom->Get_LockedTarget();
    const Shared<SL_Dualliste> dualliste = dynamic_pointer_cast<SL_Dualliste>(lockedTarget);
    if (dualliste == nullptr || dualliste->Is_Dead())
        return false;

    Vec3 worldPos{};
    if (!dualliste->Try_GetSocketWorldPosition("spine_03Socket", worldPos))
        return false;

    return Try_ProjectWorldToScreen(worldPos, outScreenPos);
}

Unique<UI_SL_LockOn> UI_SL_LockOn::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_SL_LockOn>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_SL_LockOn");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_SL_LockOn::Free()
{
    Close();

    _root.reset();
    _uiByName.clear();

    _lockOnImage.reset();

    Base::Free();
}
