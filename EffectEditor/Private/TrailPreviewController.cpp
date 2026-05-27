#include "TrailPreviewController.h"

#include "EffectEditorInstance.h"
#include "ModelCom.h"
#include "PlayableParts.h"
#include "TrailPreviewPlayerObject.h"

NS_BEGIN(EffectEditor)

TrailPreviewController::TrailPreviewController(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

TrailPreviewController::TrailPreviewController(const TrailPreviewController& prototype)
    : GameObject{ prototype }
{
}

HRESULT TrailPreviewController::Initialize(void* arg)
{
    const PreviewDesc* previewDesc = static_cast<PreviewDesc*>(arg);
    CHECK_NULL(previewDesc, E_FAIL);
    CHECK_NULL(previewDesc->player, E_FAIL);

    _player = previewDesc->player;

    CHECK_FAILED(__super::Initialize(arg), E_FAIL);

    Set_Name(L"TrailPreview_Controller");
    Set_Visible(false);
    Set_PlayerVisible(false);
    Reset_PreviewPose();

    return S_OK;
}

void TrailPreviewController::Priority_Update(float)
{
    const bool visible =
        nullptr != EDITOR &&
        EDITOR->Is_TrailPreviewVisible();

    Set_Visible(visible);
    Set_PlayerVisible(visible);

    if (!visible)
    {
        Reset_PreviewPose();
        return;
    }

    if (EDITOR->IsPlaying())
        Ensure_AnimationMode(PreviewAnimationMode::Attack);
    else if (!EDITOR->IsPaused())
        Ensure_AnimationMode(PreviewAnimationMode::Rest);
}

void TrailPreviewController::Update(float)
{
}

void TrailPreviewController::Set_PlayerVisible(bool visible)
{
    const Shared<TrailPreviewPlayerObject> player = _player.lock();
    if (nullptr != player)
        player->Set_PreviewVisible(visible);
}

void TrailPreviewController::Reset_PreviewPose()
{
    _animationInitialized = false;
    _currentMode = PreviewAnimationMode::Rest;

    Ensure_AnimationMode(PreviewAnimationMode::Rest);
}

Shared<ModelCom> TrailPreviewController::Get_BodyModel() const
{
    const Shared<TrailPreviewPlayerObject> player = _player.lock();
    if (nullptr == player)
        return nullptr;

    const PlayableParts* bodyPart =
        dynamic_cast<PlayableParts*>(player->Get_PartObject(L"Part_Body"));
    if (nullptr == bodyPart)
        return nullptr;

    return bodyPart->Get_Model();
}

void TrailPreviewController::Ensure_AnimationMode(PreviewAnimationMode mode)
{
    const Shared<ModelCom> bodyModel = Get_BodyModel();
    if (nullptr == bodyModel)
        return;

    if (_animationInitialized && _currentMode == mode)
        return;

    const char* animationName =
        mode == PreviewAnimationMode::Attack
        ? kAttackAnimationName
        : kRestAnimationName;

    if (!Has_Animation(bodyModel, animationName))
    {
        LOG_WARN_ONCE("[TrailPreview] animation '{}' not found.", animationName);
        _animationInitialized = true;
        _currentMode = mode;
        return;
    }

    bodyModel->Set_Animation(animationName, mode == PreviewAnimationMode::Attack, 0.f);
    bodyModel->Set_FrameSpeed(1.f);
    bodyModel->Set_CurrentAnimationTimeSec(0.f);
    bodyModel->Refresh_CurrentAnimationPose();

    _animationInitialized = true;
    _currentMode = mode;
}

bool TrailPreviewController::Has_Animation(const Shared<ModelCom>& model, const char* animationName) const
{
    if (nullptr == model || nullptr == animationName)
        return false;

    const uint32 numAnimations = static_cast<uint32>(model->Get_NumAnimations());
    for (uint32 animationIndex = 0; animationIndex < numAnimations; ++animationIndex)
    {
        const char* currentName = model->Get_AnimationName(animationIndex);
        if (nullptr != currentName && strcmp(currentName, animationName) == 0)
            return true;
    }

    return false;
}

Shared<TrailPreviewController> TrailPreviewController::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context)
{
    auto instance = make_shared<TrailPreviewController>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : TrailPreviewController");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> TrailPreviewController::Clone(void* arg)
{
    auto instance = make_shared<TrailPreviewController>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : TrailPreviewController");
        MSG_BOX("Failed to Clone : TrailPreviewController");
        return nullptr;
    }

    return instance;
}

void TrailPreviewController::Free()
{
    __super::Free();
}

NS_END
