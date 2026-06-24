#include "UI_GradientSkillPreviewTest.h"

#include "BattleTurnCamera.h"
#include "Body_Player.h"
#include "Camera.h"
#include "Character_BattleBase.h"
#include "Client_Constants.h"
#include "ClientInstance.h"
#include "ContainerObject.h"
#include "EffectCom.h"
#include "GameInstance.h"
#include "LightingPipeline.h"
#include "ModelCom.h"
#include "Monster_BattleBase.h"
#include "PartObject.h"
#include "PostProcessPipeline.h"
#include "PreviewCamera.h"
#include "PreviewPlayer.h"
#include "RenderTarget.h"
#include "Spawn_Prefab.h"
#include "TransformCom.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"
#include "UIPreviewManager.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kGradientSkillPreviewTestJsonPath[] =
        L"UI/UI_Json/CommonSystem/UI_GradientSkillPreviewTest.json";

    constexpr wchar_t kGradientSkillPreviewTestRootName[] =
        L"Panel_GradientSkillPreviewTest";

    constexpr wchar_t kGradientSkillPreviewTestImageName[] =
        L"Image_GradientSkillPreviewTest_RT";

    constexpr uint32 kGradientPreviewTestWidth = 960;
    constexpr uint32 kGradientPreviewTestHeight = 540;

    constexpr Vec3 kPreviewLightDirection = Vec3(-0.42f, -0.78f, 0.46f);
    constexpr Vec4 kPreviewLightDiffuse = Vec4(0.92f, 0.89f, 0.84f, 1.f);
    constexpr Vec4 kPreviewLightAmbient = Vec4(0.16f, 0.16f, 0.18f, 1.f);
    constexpr Vec4 kPreviewLightSpecular = Vec4(0.75f, 0.72f, 0.68f, 1.f);
    constexpr float kPreviewShadowIntensity = 0.58f;

    constexpr Vec3 kPreviewRimColor = Vec3(0.82f, 0.81f, 0.78f);
    constexpr float kPreviewRimIntensity = 0.24f;
    constexpr float kPreviewRimPower = 2.7f;
    constexpr float kPreviewHairRimIntensity = 0.11f;
    constexpr float kPreviewSkillPhaseMaxSec = 3.0f;
    constexpr float kPreviewSkillLoopDelaySec = 0.35f;
    constexpr wchar_t kPreviewDioramaLayerTag[] = L"Layer_UIPreview";

    uint32 Pack_RGBA(uint8 r, uint8 g, uint8 b, uint8 a = 255)
    {
        return (static_cast<uint32>(a) << 24) |
            (static_cast<uint32>(b) << 16) |
            (static_cast<uint32>(g) << 8) |
            static_cast<uint32>(r);
    }

    class ScopedRenderTargetState final
    {
    public:
        explicit ScopedRenderTargetState(const ComPtr<Context>& context)
            : _context{ context }
        {
            if (_context == nullptr)
                return;

            _viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
            _context->RSGetViewports(&_viewportCount, _viewports);
            _context->OMGetRenderTargets(1, _rtv.ReleaseAndGetAddressOf(), _dsv.ReleaseAndGetAddressOf());
            _context->OMGetBlendState(_blendState.ReleaseAndGetAddressOf(), _blendFactor, &_sampleMask);
            _context->OMGetDepthStencilState(_depthStencilState.ReleaseAndGetAddressOf(), &_stencilRef);
            _context->RSGetState(_rasterizerState.ReleaseAndGetAddressOf());
        }

        ~ScopedRenderTargetState()
        {
            if (_context == nullptr)
                return;

            ID3D11RenderTargetView* rtvs[] = { _rtv.Get() };
            _context->OMSetRenderTargets(1, rtvs, _dsv.Get());

            if (_viewportCount > 0)
                _context->RSSetViewports(_viewportCount, _viewports);

            _context->OMSetBlendState(_blendState.Get(), _blendFactor, _sampleMask);
            _context->OMSetDepthStencilState(_depthStencilState.Get(), _stencilRef);
            _context->RSSetState(_rasterizerState.Get());
        }

    private:
        ComPtr<Context> _context{};
        ComPtr<ID3D11RenderTargetView> _rtv{};
        ComPtr<ID3D11DepthStencilView> _dsv{};
        ComPtr<ID3D11BlendState> _blendState{};
        ComPtr<ID3D11DepthStencilState> _depthStencilState{};
        ComPtr<ID3D11RasterizerState> _rasterizerState{};
        D3D11_VIEWPORT _viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
        uint32 _viewportCount = 0;
        float _blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
        uint32 _sampleMask = 0xffffffff;
        uint32 _stencilRef = 0;
    };

    class ScopedActiveCamera final
    {
    public:
        explicit ScopedActiveCamera(const Shared<Camera>& nextCamera)
            : _previousCamera{ GAME->Get_ActiveCamera() }
        {
            if (nextCamera)
                GAME->Change_ActiveCamera(nextCamera);
        }

        ~ScopedActiveCamera()
        {
            GAME->Change_ActiveCamera(_previousCamera);
        }

    private:
        Shared<Camera> _previousCamera{};
    };

    float Resolve_BattleTurnGradientPreviewFovY(float fallbackFovY)
    {
        const Shared<BattleTurnCamera> battleTurnCamera =
            dynamic_pointer_cast<BattleTurnCamera>(GAME->Get_CameraByType(ObjectType::BattleTurnCamera));
        if (battleTurnCamera == nullptr)
            return fallbackFovY;

        return battleTurnCamera->Get_Preset(BattleCameraMode::PLAYERATTACK).camera.fovy;
    }

    Body_Player* Get_BattleHeroBody(const Shared<Character_BattleBase>& battleHero)
    {
        if (battleHero == nullptr)
            return nullptr;

        return dynamic_cast<Body_Player*>(battleHero->Get_PartObject(TEXT("Part_Body")));
    }

    void Play_BattleHeroAnimation(
        const Shared<Character_BattleBase>& battleHero,
        const vector<string>& animationKeys,
        size_t phaseIndex)
    {
        if (phaseIndex >= animationKeys.size())
            return;

        Body_Player* body = Get_BattleHeroBody(battleHero);
        if (body == nullptr)
            return;

        const Shared<ModelCom> model = body->Get_Model();
        if (model == nullptr)
            return;

        // UI 설명용 프리뷰라 실제 BattleActionExecutor/피해/턴 처리는 호출하지 않는다.
        model->Set_Animation(animationKeys[phaseIndex], 0.15f, false, 0.f);
    }
}

HRESULT UI_GradientSkillPreviewTest::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close_Immediately();

    return S_OK;
}

void UI_GradientSkillPreviewTest::Update(float timeDelta)
{
    if (!Is_Open())
        return;

    Update_RenderTarget(timeDelta);
}

void UI_GradientSkillPreviewTest::Open()
{
    if (_root == nullptr)
        return;

    UI_RuntimePanelBase::Open();

    for (const auto& [name, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(name);

        if (ui)
            ui->Open();
    }

    Bind_RenderTargetToImage();

    GAME->Unlock_Mouse();
    GAME->Set_GameplayPaused(true);
    GAME->Set_GameplayInputMode(GameplayInputMode::UIOnly); // 테스트 UI가 열린 동안 4번/ESC 입력을 월드 액션과 분리한다.
}

void UI_GradientSkillPreviewTest::Close()
{
    Close_Immediately();
}

void UI_GradientSkillPreviewTest::Close_Immediately()
{
    Restore_PreviewRenderOverrides();
    Clear_RenderTargetImage();
    Clear_PreviewDioramaActors();

    UI_RuntimePanelBase::Close();

    for (const auto& [name, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(name);

        if (ui)
            ui->Close();
    }

    _elapsedSec = 0.f;
    _previewResourcesPrepared = false;
    _previewSkillKeys.clear();
    _previewSkillPhaseIndex = 0;
    _previewSkillTimer = 0.f;
    _previewSkillPlaying = false;

    GAME->Set_GameplayPaused(false);
    GAME->Set_GameplayInputMode(GameplayInputMode::Normal);
    GAME->Lock_Mouse();
}

HRESULT UI_GradientSkillPreviewTest::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = kGradientSkillPreviewTestJsonPath;
    desc.prototypeTagPrefix = L"GradientSkillPreviewTest";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, kGradientSkillPreviewTestRootName);
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_GradientSkillPreviewTest::Bind_UI()
{
    _previewImage = Find_TypedUI<UI_Image>(
        _uiByName,
        kGradientSkillPreviewTestImageName,
        "GradientSkillPreviewTest");

    CHECK_NULL(_previewImage, E_FAIL);

    return S_OK;
}

HRESULT UI_GradientSkillPreviewTest::Ensure_RenderTarget()
{
    if (_previewTarget)
        return S_OK;

    _previewTarget = Engine::RenderTarget::Create(
        L"RT_GradientSkillPreviewTest",
        GAME->Get_Device(),
        GAME->Get_Context(),
        kGradientPreviewTestWidth,
        kGradientPreviewTestHeight,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        Vec4(0.f, 0.f, 0.f, 1.f),
        true);

    CHECK_NULL(_previewTarget, E_FAIL);

    return S_OK;
}

void UI_GradientSkillPreviewTest::Bind_RenderTargetToImage()
{
    if (_previewImage == nullptr)
        return;

    if (FAILED(Ensure_RenderTarget()))
        return;

    // 테스트 UI의 이미지가 파일 텍스처 대신 실시간 RenderTarget을 표시하게 한다.
    _previewImage->Set_ExternalShaderResource(_previewTarget->Get_SRV());
}

void UI_GradientSkillPreviewTest::Update_RenderTarget(float timeDelta)
{
    if (FAILED(Ensure_RenderTarget()))
        return;

    if (Try_RenderPreviewHeroToTarget(timeDelta))
        return;

    Update_TestPatternRenderTarget(timeDelta);
}

bool UI_GradientSkillPreviewTest::Try_RenderPreviewHeroToTarget(float timeDelta)
{
    if (_previewTarget == nullptr || GAME->Get_Context() == nullptr)
        return false;

    const Shared<UIPreviewManager> previewManager = CLIENT->Get_PreviewManager();
    if (previewManager == nullptr)
        return false;

    if (!_previewResourcesPrepared)
    {
        if (FAILED(previewManager->Prepare_PreviewResources()))
            return false;

        _previewResourcesPrepared = true;
    }

    const Shared<PreviewCamera> previewCamera = previewManager->Get_PreviewCamera();
    if (previewCamera == nullptr)
        return false;

    if (FAILED(Ensure_PreviewDioramaActors()))
        return false;

    if (_previewBattleHero == nullptr ||
        _previewPotatoBag == nullptr ||
        _previewBattleHero->Get_Transform() == nullptr)
        return false;

    Update_PreviewDioramaActors(timeDelta);

    previewCamera->Set_Distance(5.8f);
    previewCamera->Set_LookOffset(Vec3(1.0f, 1.25f, 0.f));
    previewCamera->Set_FollowSpeed(30.f);
    previewCamera->Set_FovY(Resolve_BattleTurnGradientPreviewFovY(previewCamera->Get_FovY()));
    previewCamera->Set_TargetTransform(_previewBattleHero->Get_Transform());
    previewCamera->Update(timeDelta);

    const bool wasHeroVisible = _previewBattleHero->Is_Visible();
    const bool wasPotatoBagVisible = _previewPotatoBag->Is_Visible();
    _previewBattleHero->Set_Visible(true);
    _previewPotatoBag->Set_Visible(true);

    ScopedRenderTargetState restoreRenderState{ GAME->Get_Context() };
    ScopedActiveCamera restoreCamera{ static_pointer_cast<Camera>(previewCamera) };
    Apply_PreviewRenderOverrides();

    ID3D11ShaderResourceView* nullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
    GAME->Get_Context()->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);
    GAME->Get_Context()->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);

    if (FAILED(_previewTarget->Begin(true, true)))
    {
        Restore_PreviewRenderOverrides();
        _previewBattleHero->Set_Visible(wasHeroVisible);
        _previewPotatoBag->Set_Visible(wasPotatoBagVisible);
        return false;
    }

    HRESULT renderResult = Render_PreviewActorParts(_previewPotatoBag.get());
    if (SUCCEEDED(renderResult))
        renderResult = Render_PreviewActorParts(_previewBattleHero.get());

    GAME->Get_Context()->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);
    GAME->Get_Context()->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);

    Restore_PreviewRenderOverrides();
    _previewBattleHero->Set_Visible(wasHeroVisible);
    _previewPotatoBag->Set_Visible(wasPotatoBagVisible);
    return SUCCEEDED(renderResult);
}

HRESULT UI_GradientSkillPreviewTest::Ensure_PreviewDioramaActors()
{
    if (_previewBattleHero && _previewPotatoBag)
        return S_OK;

    Clear_PreviewDioramaActors();

    _previewBattleHero = dynamic_pointer_cast<Character_BattleBase>(
        Spawn_Prefab::Prefab(Constants::Prefab::PB_Maelle)
        .AtLevel(ETOI(LevelType::Static))
        .InLayer(kPreviewDioramaLayerTag)
        .Position(Vec3(-1.1f, 0.f, 0.f))
        .Rotation(Vec3(0.f, 90.f, 0.f))
        .Spawn());

    _previewPotatoBag = dynamic_pointer_cast<Monster_BattleBase>(
        Spawn_Prefab::Prefab(Constants::Prefab::MB_PotatoBag)
        .AtLevel(ETOI(LevelType::Static))
        .InLayer(kPreviewDioramaLayerTag)
        .Position(Vec3(1.25f, 0.f, 0.f))
        .Rotation(Vec3(0.f, -90.f, 0.f))
        .Spawn());

    if (_previewBattleHero == nullptr || _previewPotatoBag == nullptr)
    {
        Clear_PreviewDioramaActors();
        return E_FAIL;
    }

    _previewBattleHero->Set_BattleColliderActive(false);
    _previewPotatoBag->Set_BattleColliderActive(false);
    _previewBattleHero->Set_Visible(false);
    _previewPotatoBag->Set_Visible(false);

    Layout_PreviewDioramaActors();
    Start_PreviewSkillPlayback();

    return S_OK;
}

void UI_GradientSkillPreviewTest::Clear_PreviewDioramaActors()
{
    if (_previewBattleHero)
    {
        _previewBattleHero->Set_Visible(false);
        if (const Shared<EffectCom> effectCom = _previewBattleHero->Get_Component<EffectCom>())
        {
            effectCom->Clear_PreviewRim();
            effectCom->Stop_AllAttachedEffects();
        }

        GAME->Remove_GameObject(ETOI(LevelType::Static), _previewBattleHero);
        _previewBattleHero.reset();
    }

    if (_previewPotatoBag)
    {
        _previewPotatoBag->Set_Visible(false);
        if (const Shared<EffectCom> effectCom = _previewPotatoBag->Get_Component<EffectCom>())
        {
            effectCom->Clear_PreviewRim();
            effectCom->Stop_AllAttachedEffects();
        }

        GAME->Remove_GameObject(ETOI(LevelType::Static), _previewPotatoBag);
        _previewPotatoBag.reset();
    }
}

void UI_GradientSkillPreviewTest::Layout_PreviewDioramaActors()
{
    if (_previewBattleHero && _previewBattleHero->Get_Transform())
    {
        _previewBattleHero->Get_Transform()->Set_WorldPosition(Vec3(-1.1f, 0.f, 0.f));
        _previewBattleHero->Get_Transform()->Set_WorldRotationEuler(Vec3(0.f, 90.f, 0.f));
        _previewBattleHero->Get_Transform()->Update_WorldMatrix();
    }

    if (_previewPotatoBag && _previewPotatoBag->Get_Transform())
    {
        _previewPotatoBag->Get_Transform()->Set_WorldPosition(Vec3(1.25f, 0.f, 0.f));
        _previewPotatoBag->Get_Transform()->Set_WorldRotationEuler(Vec3(0.f, -90.f, 0.f));
        _previewPotatoBag->Get_Transform()->Update_WorldMatrix();
    }
}

void UI_GradientSkillPreviewTest::Update_PreviewDioramaActors(float timeDelta)
{
    const float safeDelta = max(0.f, timeDelta);
    Update_PreviewSkillPlayback(safeDelta);

    const auto updateActor = [safeDelta](const Shared<Character_BattleBase>& actor)
    {
        if (actor == nullptr)
            return;

        actor->Try_BeginPlay();

        if (actor->Get_Transform())
            actor->Get_Transform()->Update_WorldMatrix();

        for (const auto& [partTag, partObject] : actor->Get_PartObjects())
        {
            UNREFERENCED_PARAMETER(partTag);

            if (partObject == nullptr)
                continue;

            partObject->Try_BeginPlay();
            partObject->Priority_Update(safeDelta);
            partObject->Update(safeDelta);
            partObject->Late_Update(safeDelta);
        }

        if (const Shared<EffectCom> effectCom = actor->Get_Component<EffectCom>())
            effectCom->Tick_AttachedEffectInstances(safeDelta);
    };

    updateActor(_previewBattleHero);
    updateActor(_previewPotatoBag);
}

void UI_GradientSkillPreviewTest::Start_PreviewSkillPlayback()
{
    _previewSkillKeys = Build_PreviewAnimationKeys(_previewSkillKey);
    _previewSkillPhaseIndex = 0;
    _previewSkillTimer = 0.f;
    _previewSkillPlaying = !_previewSkillKeys.empty();

    if (!_previewSkillPlaying)
        return;

    Play_BattleHeroAnimation(_previewBattleHero, _previewSkillKeys, _previewSkillPhaseIndex);
}

void UI_GradientSkillPreviewTest::Update_PreviewSkillPlayback(float timeDelta)
{
    if (!_previewSkillPlaying || _previewSkillKeys.empty())
        return;

    _previewSkillTimer += timeDelta;

    Body_Player* body = Get_BattleHeroBody(_previewBattleHero);
    const bool phaseFinished =
        (body != nullptr && body->Is_AnimationFinished()) ||
        _previewSkillTimer >= kPreviewSkillPhaseMaxSec;
    if (!phaseFinished)
        return;

    if (_previewSkillPhaseIndex + 1 >= _previewSkillKeys.size())
    {
        if (_previewSkillTimer < kPreviewSkillPhaseMaxSec + kPreviewSkillLoopDelaySec)
            return;

        _previewSkillPhaseIndex = 0;
    }
    else
    {
        ++_previewSkillPhaseIndex;
    }

    _previewSkillTimer = 0.f;
    Play_BattleHeroAnimation(_previewBattleHero, _previewSkillKeys, _previewSkillPhaseIndex);
}

vector<string> UI_GradientSkillPreviewTest::Build_PreviewAnimationKeys(const string& skillKey) const
{
    vector<string> animationKeys{};

    const Skill_Data* skillData = CLIENT->Get_Skill(skillKey);
    if (skillData == nullptr)
        return animationKeys;

    animationKeys.reserve(skillData->AnimPhases.size());
    for (const BattleActionPhase& phase : skillData->AnimPhases)
    {
        if (!phase.animationKey.empty())
            animationKeys.push_back(phase.animationKey);
    }

    return animationKeys;
}

HRESULT UI_GradientSkillPreviewTest::Render_PreviewActorParts(Engine::ContainerObject* actor)
{
    if (actor == nullptr)
        return E_FAIL;

    actor->Try_BeginPlay();

    for (const auto& [partTag, partObject] : actor->Get_PartObjects())
    {
        UNREFERENCED_PARAMETER(partTag);

        if (partObject == nullptr || !partObject->Is_Visible())
            continue;

        CHECK_FAILED(partObject->Render(), E_FAIL);
    }

    return S_OK;
}

void UI_GradientSkillPreviewTest::Update_TestPatternRenderTarget(float timeDelta)
{
    _elapsedSec = fmodf(_elapsedSec + max(0.f, timeDelta), 10.f);

    ComPtr<Context> context = GAME->Get_Context();
    if (context == nullptr || _previewTarget->Get_Texture() == nullptr)
        return;

    static vector<uint32> pixels(kGradientPreviewTestWidth * kGradientPreviewTestHeight);

    const float wave = 0.5f + 0.5f * sinf(_elapsedSec * 2.4f);
    const int32 boxSize = 96;
    const int32 boxX = static_cast<int32>(
        wave * static_cast<float>(kGradientPreviewTestWidth - boxSize));
    const int32 boxY = static_cast<int32>(
        (0.5f + 0.5f * sinf(_elapsedSec * 1.3f + 1.1f)) *
        static_cast<float>(kGradientPreviewTestHeight - boxSize));
    const int32 scanX = static_cast<int32>(
        fmodf(_elapsedSec * 180.f, static_cast<float>(kGradientPreviewTestWidth)));

    for (uint32 y = 0; y < kGradientPreviewTestHeight; ++y)
    {
        for (uint32 x = 0; x < kGradientPreviewTestWidth; ++x)
        {
            const float u = static_cast<float>(x) / static_cast<float>(kGradientPreviewTestWidth - 1);
            const float v = static_cast<float>(y) / static_cast<float>(kGradientPreviewTestHeight - 1);

            uint8 r = static_cast<uint8>(35.f + 85.f * u);
            uint8 g = static_cast<uint8>(12.f + 30.f * wave);
            uint8 b = static_cast<uint8>(78.f + 120.f * (1.f - v));

            if ((x % 80u) < 2u || (y % 60u) < 2u)
            {
                r = static_cast<uint8>(min<int32>(255, r + 45));
                g = static_cast<uint8>(min<int32>(255, g + 45));
                b = static_cast<uint8>(min<int32>(255, b + 45));
            }

            if (abs(static_cast<int32>(x) - scanX) < 4)
            {
                r = 255;
                g = 185;
                b = 70;
            }

            if (static_cast<int32>(x) >= boxX &&
                static_cast<int32>(x) < boxX + boxSize &&
                static_cast<int32>(y) >= boxY &&
                static_cast<int32>(y) < boxY + boxSize)
            {
                r = 255;
                g = static_cast<uint8>(80 + 120 * wave);
                b = 235;
            }

            pixels[y * kGradientPreviewTestWidth + x] = Pack_RGBA(r, g, b);
        }
    }

    // 2차 테스트는 RT가 실시간으로 갱신되는지 보기 위해 CPU 패턴을 직접 업로드한다.
    context->UpdateSubresource(
        _previewTarget->Get_Texture().Get(),
        0,
        nullptr,
        pixels.data(),
        kGradientPreviewTestWidth * sizeof(uint32),
        0);
}

void UI_GradientSkillPreviewTest::Clear_RenderTargetImage()
{
    if (_previewImage)
        _previewImage->Clear_ExternalShaderResource();
}

void UI_GradientSkillPreviewTest::Apply_PreviewRenderOverrides()
{
    const Shared<PostProcessPipeline> postProcessPipeline = GAME->Get_PostProcessPipeline();
    const Shared<LightingPipeline> lightingPipeline = GAME->Get_LightingPipeline();
    const LIGHT_DESC* primaryLightDesc = GAME->Get_PrimaryShadowLightDesc();
    const ShadowLightDesc* shadowLightDesc = GAME->Get_ShadowLightDesc();

    if (primaryLightDesc == nullptr)
    {
        const uint32 lightCount = GAME->Get_LightCount();
        for (uint32 lightIndex = 0; lightIndex < lightCount; ++lightIndex)
        {
            const LIGHT_DESC* lightDesc = GAME->Get_LightDesc(lightIndex);
            if (lightDesc == nullptr || lightDesc->type != LightType::Directional)
                continue;

            primaryLightDesc = lightDesc;
            break;
        }
    }

    if (!_previewRenderOverrideState.isCaptured)
    {
        _previewRenderOverrideState.isCaptured = true;
        _previewRenderOverrideState.cascadeLevel = GAME->Get_ShadowCascadeLevel();
        _previewRenderOverrideState.hasPrimaryLight = (primaryLightDesc != nullptr);
        _previewRenderOverrideState.hasShadowLight = (shadowLightDesc != nullptr);
        _previewRenderOverrideState.hasPostProcess = (postProcessPipeline != nullptr);
        _previewRenderOverrideState.hasLighting = (lightingPipeline != nullptr);

        if (primaryLightDesc != nullptr)
            _previewRenderOverrideState.primaryLight = *primaryLightDesc;

        if (shadowLightDesc != nullptr)
            _previewRenderOverrideState.shadowLight = *shadowLightDesc;

        if (postProcessPipeline != nullptr)
        {
            _previewRenderOverrideState.postProcess = postProcessPipeline->Get_PostProcessParams();
            _previewRenderOverrideState.postProcessFeatures = postProcessPipeline->Get_PostProcessFeatures();
        }

        if (lightingPipeline != nullptr)
        {
            _previewRenderOverrideState.deferredLighting = lightingPipeline->Get_DeferredLightingParams();
            _previewRenderOverrideState.iblEnabled = lightingPipeline->Is_IBLEnabled();
        }
    }

    if (_previewRenderOverrideState.hasPrimaryLight)
    {
        LIGHT_DESC previewLight = _previewRenderOverrideState.primaryLight;
        Vec3 previewLightDir = kPreviewLightDirection;
        if (previewLightDir.LengthSquared() > 0.0001f)
            previewLightDir.Normalize();

        previewLight.direction = Vec4(previewLightDir.x, previewLightDir.y, previewLightDir.z, 0.f);
        previewLight.diffuse = kPreviewLightDiffuse;
        previewLight.ambient = kPreviewLightAmbient;
        previewLight.specular = kPreviewLightSpecular;
        GAME->Update_PrimaryShadowLightDesc(previewLight);
        GAME->Set_ShadowCascadeLevel(Engine::CascadeLevel::Close);
    }

    if (_previewRenderOverrideState.hasShadowLight)
    {
        ShadowLightDesc previewShadow = _previewRenderOverrideState.shadowLight;
        previewShadow.intensity = max(previewShadow.intensity, kPreviewShadowIntensity);
        GAME->Update_ShadowLight(previewShadow);
    }

    if (_previewRenderOverrideState.hasPostProcess && postProcessPipeline != nullptr)
    {
        Engine::PostProcessCB previewPostProcess = _previewRenderOverrideState.postProcess;
        previewPostProcess.exposure = 0.60f;
        previewPostProcess.gamma = 1.33f;
        previewPostProcess.bloomThreshold = 0.78f;
        previewPostProcess.bloomIntensity = 0.64f;
        previewPostProcess.bloomScatter = 0.88f;
        postProcessPipeline->Set_PostProcessFeatures(
            Engine::PostProcessFeature::HDR |
            Engine::PostProcessFeature::ToneMapping |
            Engine::PostProcessFeature::Exposure |
            Engine::PostProcessFeature::Bloom |
            Engine::PostProcessFeature::Gamma);
        postProcessPipeline->Set_PostProcessParams(previewPostProcess);
    }

    if (_previewRenderOverrideState.hasLighting && lightingPipeline != nullptr)
    {
        Engine::DeferredLightingCB previewDeferredLighting = _previewRenderOverrideState.deferredLighting;
        previewDeferredLighting.iblIntensity = 0.64f;
        previewDeferredLighting.iblDiffuseIntensity = 0.34f;
        previewDeferredLighting.iblSpecularIntensity = 1.18f;
        lightingPipeline->Set_DeferredLightingParams(previewDeferredLighting);
        lightingPipeline->Set_IBLEnabled(true);
    }

    Apply_PreviewRimState();
}

void UI_GradientSkillPreviewTest::Restore_PreviewRenderOverrides()
{
    if (!_previewRenderOverrideState.isCaptured)
        return;

    Clear_PreviewRimState();

    const Shared<PostProcessPipeline> postProcessPipeline = GAME->Get_PostProcessPipeline();
    const Shared<LightingPipeline> lightingPipeline = GAME->Get_LightingPipeline();

    if (_previewRenderOverrideState.hasPrimaryLight)
    {
        GAME->Update_PrimaryShadowLightDesc(_previewRenderOverrideState.primaryLight);
        GAME->Set_ShadowCascadeLevel(_previewRenderOverrideState.cascadeLevel);
    }

    if (_previewRenderOverrideState.hasShadowLight)
        GAME->Update_ShadowLight(_previewRenderOverrideState.shadowLight);

    if (_previewRenderOverrideState.hasPostProcess && postProcessPipeline != nullptr)
    {
        postProcessPipeline->Set_PostProcessFeatures(_previewRenderOverrideState.postProcessFeatures);
        postProcessPipeline->Set_PostProcessParams(_previewRenderOverrideState.postProcess);
    }

    if (_previewRenderOverrideState.hasLighting && lightingPipeline != nullptr)
    {
        lightingPipeline->Set_DeferredLightingParams(_previewRenderOverrideState.deferredLighting);
        lightingPipeline->Set_IBLEnabled(_previewRenderOverrideState.iblEnabled);
    }

    _previewRenderOverrideState = PreviewRenderOverrideState{};
}

void UI_GradientSkillPreviewTest::Apply_PreviewRimState()
{
    const auto applyPreviewRim = [](const Shared<Character_BattleBase>& actor)
    {
        if (actor == nullptr)
            return;

        const Shared<EffectCom> effectCom = actor->Get_Component<EffectCom>();
        if (effectCom == nullptr)
            return;

        if (actor->Is_Visible())
            effectCom->Set_PreviewRim(
                kPreviewRimColor,
                kPreviewRimIntensity,
                kPreviewRimPower,
                kPreviewHairRimIntensity);
        else
            effectCom->Clear_PreviewRim();
    };

    applyPreviewRim(_previewBattleHero);
    applyPreviewRim(_previewPotatoBag);

    const Shared<UIPreviewManager> previewManager = CLIENT->Get_PreviewManager();
    if (previewManager == nullptr)
        return;

    for (const auto& previewHero : previewManager->Get_PreviewHeroes())
    {
        if (previewHero.player == nullptr)
            continue;

        const Shared<EffectCom> effectCom = previewHero.player->Get_Component<EffectCom>();
        if (effectCom == nullptr)
            continue;

        if (previewHero.player->Is_Visible())
            effectCom->Set_PreviewRim(
                kPreviewRimColor,
                kPreviewRimIntensity,
                kPreviewRimPower,
                kPreviewHairRimIntensity);
        else
            effectCom->Clear_PreviewRim();
    }
}

void UI_GradientSkillPreviewTest::Clear_PreviewRimState()
{
    const auto clearPreviewRim = [](const Shared<Character_BattleBase>& actor)
    {
        if (actor == nullptr)
            return;

        if (const Shared<EffectCom> effectCom = actor->Get_Component<EffectCom>())
            effectCom->Clear_PreviewRim();
    };

    clearPreviewRim(_previewBattleHero);
    clearPreviewRim(_previewPotatoBag);

    const Shared<UIPreviewManager> previewManager = CLIENT->Get_PreviewManager();
    if (previewManager == nullptr)
        return;

    for (const auto& previewHero : previewManager->Get_PreviewHeroes())
    {
        if (previewHero.player == nullptr)
            continue;

        if (const Shared<EffectCom> effectCom = previewHero.player->Get_Component<EffectCom>())
            effectCom->Clear_PreviewRim();
    }
}

Unique<UI_GradientSkillPreviewTest> UI_GradientSkillPreviewTest::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_GradientSkillPreviewTest>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_GradientSkillPreviewTest");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_GradientSkillPreviewTest::Free()
{
    Close_Immediately();

    _previewTarget.reset();
    _previewImage.reset();
    _root.reset();
    _uiByName.clear();

    __super::Free();
}
