#include "UI_MarkEffect.h"

#include "Character_BattleBase.h"
#include "Client_Constants.h"
#include "CombatStatCom.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"
#include "PartObject.h"
#include "TransformCom.h"

using namespace UI_Utils;

namespace
{
    constexpr Vec3 kMarkWorldAnchorOffset = Vec3(0.f, 1.2f, 0.f);
    constexpr Vec2 kMarkScreenBaseOffset = Vec2(0.f, -10.f);
}

HRESULT UI_MarkEffect::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Clear_Target();

    return S_OK;
}

void UI_MarkEffect::Update(float timeDelta)
{
    if (!_hasTarget)
        return;

    if (!Is_ValidTarget(_target))
    {
        Clear_Target();
        return;
    }

    Vec3 worldPos{};
    if (!Try_GetMarkWorldPosition(_target, worldPos))
    {
        Clear_Target();
        return;
    }

    Vec2 screenPos{};
    if (!Try_ProjectWorldToScreen(worldPos, screenPos))
    {
        Close();
        return;
    }

    screenPos += kMarkScreenBaseOffset;
    Apply_EditorOffsets(screenPos);

    if (_markEffectPanel && _markEffectPanel->Get_Transform())
        _markEffectPanel->Get_Transform()->Set_Position(screenPos);

    if (!Is_Open())
    {
        Open();

        _markEffectPanel->Play_UIAnimation(L"Mark_Effect_Loop", true);
    }
}

void UI_MarkEffect::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_MarkEffect::Close()
{
    if (_markEffectPanel)
        _markEffectPanel->Stop_UIAnimation();

    UI_RuntimePanelBase::Close();
}

void UI_MarkEffect::Show_Target(const BattleChar& target)
{
    if (!Is_ValidTarget(target) || _markEffectPanel == nullptr)
        return;

    _target = target;
    _hasTarget = true;

    Open();

    _markEffectPanel->Play_UIAnimation(L"Mark_Effect_Loop", true);
}

void UI_MarkEffect::Clear_Target()
{
    _target = {};
    _hasTarget = false;

    Close();
}

HRESULT UI_MarkEffect::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_MarkEffect_Path;
    desc.animationPath = Constants::UI_Anim::UI_MarkEffectLoopAnim_Path;
    desc.prototypeTagPrefix = L"MarkEffect";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    return S_OK;
}

HRESULT UI_MarkEffect::Bind_UI()
{
    _root = Find_UI(_uiByName, L"Panel_Mark_Effect");
    CHECK_NULL(_root, E_FAIL);

    _markEffectPanel = Find_UI(_uiByName, L"Panel_Mark_Effect");
    CHECK_NULL(_markEffectPanel, E_FAIL);

    return S_OK;
}

bool UI_MarkEffect::Is_ValidTarget(const BattleChar& target) const
{
    if (target.actor == nullptr || target.statCom == nullptr)
        return false;

    if (target.actor->Get_Transform() == nullptr)
        return false;

    if (target.statCom->Is_Player())
        return false;

    return target.statCom->Get_AliveStatus() == EAliveStatus::Alive;
}

bool UI_MarkEffect::Try_GetMarkWorldPosition(const BattleChar& target, Vec3& outWorldPos) const
{
    if (!Is_ValidTarget(target))
        return false;

    if (PartObject* body = target.actor->Get_PartObject(L"Part_Body"))
    {
        const Matrix& bodyWorld = body->Get_CombinedWorldMatrix();
        outWorldPos = Vec3(bodyWorld._41, bodyWorld._42, bodyWorld._43);
        outWorldPos += kMarkWorldAnchorOffset;
        return true;
    }

    outWorldPos = target.actor->Get_Transform()->Get_WorldPosition();
    outWorldPos += kMarkWorldAnchorOffset;
    return true;
}

Unique<UI_MarkEffect> UI_MarkEffect::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_MarkEffect>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_MarkEffect");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_MarkEffect::Free()
{
    UI_RuntimePanelBase::Free();
}
