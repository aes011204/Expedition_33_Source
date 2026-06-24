#include "UI_GradientSkillPreviewTest.h"

#include "Camera.h"
#include "ClientInstance.h"
#include "GameInstance.h"
#include "PreviewCamera.h"
#include "PreviewPlayer.h"
#include "RenderTarget.h"
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
    Clear_RenderTargetImage();

    UI_RuntimePanelBase::Close();

    for (const auto& [name, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(name);

        if (ui)
            ui->Close();
    }

    _elapsedSec = 0.f;
    _previewResourcesPrepared = false;
    _previewAnimationStarted = false;

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

    Shared<PreviewPlayer> previewPlayer{};
    HeroId previewHeroId = HeroId::END;
    for (const UIPreviewManager::PreviewHeroEntry& previewHero : previewManager->Get_PreviewHeroes())
    {
        if (previewHero.player == nullptr)
            continue;

        previewPlayer = previewHero.player;
        previewHeroId = previewHero.heroId;
        break;
    }

    const Shared<PreviewCamera> previewCamera = previewManager->Get_PreviewCamera();
    if (previewPlayer == nullptr || previewCamera == nullptr || previewPlayer->Get_Transform() == nullptr)
        return false;

    if (!_previewAnimationStarted)
    {
        previewPlayer->Play_PreviewAnimation(PreviewAnimState::Skill_Intro, PreviewAnimState::Skill_Idle);
        _previewAnimationStarted = true;
    }

    const bool wasVisible = previewPlayer->Is_Visible();
    previewPlayer->Set_Visible(true);
    previewPlayer->Update_Preview(timeDelta, false);

    const PreviewShotPreset preset = CLIENT->Get_Preset(previewHeroId, PreviewShotPanel::ChangeSkill);
    previewCamera->Set_Preset(preset);
    previewCamera->Set_TargetTransform(previewPlayer->Get_Transform());
    previewCamera->Update(timeDelta);

    ScopedRenderTargetState restoreRenderState{ GAME->Get_Context() };
    ScopedActiveCamera restoreCamera{ static_pointer_cast<Camera>(previewCamera) };

    ID3D11ShaderResourceView* nullSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
    GAME->Get_Context()->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);
    GAME->Get_Context()->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);

    if (FAILED(_previewTarget->Begin(true, true)))
    {
        previewPlayer->Set_Visible(wasVisible);
        return false;
    }

    const HRESULT renderResult = previewPlayer->Render_PreviewParts();

    GAME->Get_Context()->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);
    GAME->Get_Context()->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRVs);

    previewPlayer->Set_Visible(wasVisible);
    return SUCCEEDED(renderResult);
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
