#include "Level_Loading.h"
#include "Loader.h"

#include "Client_Constants.h"
#include "ClientInstance.h"
#include "GameInstance.h"
#include "UI_RuntimeLoader.h"

#include "Level_MainTitle.h"
#include "Level_GameEntry.h"
#include "Level_GamePlay.h"
#include "Level_MapTest.h"
#include "Level_Stage01.h"

Level_Loading::Level_Loading(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Level{ device, context }
{
}

HRESULT Level_Loading::Initialize(LevelType eNextLevelID)
{
    _nextLevel = eNextLevelID;
    _loadingStartTime = chrono::steady_clock::now();
    _printedLoadCompleteLog = false;
    _hasRenderedFirstFrame = false;
    _postFirstRenderFrameCount = 0;
    _requestedNextLevelTransition = false;

    if (FAILED(Ready_LoadingUIRenderResources()))
        return E_FAIL;

	if (FAILED(Ready_Layer_BackGround(TEXT("Layer_BackGround"))))
		return E_FAIL;

    if (FAILED(Ready_Layer_UI(TEXT("Layer_UI"))))
        return E_FAIL;

    _loader = Loader::Create(_device.Get(), _context.Get(), eNextLevelID);

    if (nullptr == _loader)
        return E_FAIL;

    return S_OK;
}

void Level_Loading::Update(float fTimeDelta)
{
    CHECK_NULL(_loader);

    if (!_hasRenderedFirstFrame)
        return;

    const size_t maxMainThreadCreatePerFrame =
        _postFirstRenderFrameCount < 5 ? 1 : 2;

    const chrono::milliseconds maxMainThreadCreateTime =
        _postFirstRenderFrameCount < 5 ? chrono::milliseconds(1) : chrono::milliseconds(3);

    CHECK_FAILED(_loader->Process_PendingLoadJobs(8, maxMainThreadCreatePerFrame, maxMainThreadCreateTime));

    ++_postFirstRenderFrameCount;

#ifdef _DEBUG
    if (_loader->isFinished() && !_printedLoadCompleteLog)
    {
        const auto elapsed = chrono::duration_cast<chrono::milliseconds>(
            chrono::steady_clock::now() - _loadingStartTime);

        LOG_DEBUG(
            "[Level_Loading::Summary] nextLevel={}, totalLoadingTime={:.1f} seconds",
            string(magic_enum::enum_name(_nextLevel)),
            static_cast<float>(elapsed.count()) / 1000.f);

        _printedLoadCompleteLog = true;
    }
#endif

    if (_loader->isFinished())
    {
        if (_requestedNextLevelTransition)
            return;

        if (CLIENT->Is_FadeTransitionPlaying())
            return;

        Shared<Level> pNextLevel = nullptr;

        switch (_nextLevel)
        {
        case LevelType::MainTitle:
            pNextLevel = Level_MainTitle::Create(_device.Get(), _context.Get());
            break;
        case LevelType::GameEntry:
            pNextLevel = Level_GameEntry::Create(_device.Get(), _context.Get());
            break;
        case LevelType::GamePlay:
            pNextLevel = Level_GamePlay::Create(_device.Get(), _context.Get());
            break;
        case LevelType::MapTest:
            pNextLevel = Level_MapTest::Create(_device.Get(), _context.Get());
            break;
        case LevelType::Stage01:
            pNextLevel = Level_Stage01::Create(_device.Get(), _context.Get());
            break;
        }

        if (nullptr == pNextLevel)
        {
            MSG_BOX("Failed to Created : NextLevel");
            return;
        }

        _requestedNextLevelTransition = true;

        if (!CLIENT->Request_LoadedLevelTransition(_nextLevel, pNextLevel))
        {
            if (FAILED(GAME->Change_Level(ETOI(_nextLevel), pNextLevel)))
            {
                MSG_BOX("Failed to Changed : NextLevel");
                _requestedNextLevelTransition = false;
                return;
            }
        }

        return;
    }
}

HRESULT Level_Loading::Render()
{
    if (!_hasRenderedFirstFrame)
        _hasRenderedFirstFrame = true;

#ifdef _DEBUG
    _loader->Print_LoadingText();
#endif

    return S_OK;
}

HRESULT Level_Loading::Ready_LoadingUIRenderResources()
{
    Shared<ResourceLoader> resourceLoader = ResourceLoader::Create(_device, _context);
    CHECK_NULL(resourceLoader, E_FAIL);

    vector<FLoadJob> loadJobs;
    CHECK_FAILED(resourceLoader->Build_LoadingUIRenderJobs(loadJobs), E_FAIL);

    for (const FLoadJob& loadJob : loadJobs)
    {
        CHECK_FAILED(resourceLoader->Register_LoadJob(loadJob), E_FAIL);
    }

    return S_OK;
}

HRESULT Level_Loading::Ready_Layer_BackGround(const wchar_t* pLayerTag)
{
    // 여기에 나중에 이팩트 추가?

	return S_OK;
}

HRESULT Level_Loading::Ready_Layer_UI(const wchar_t* pLayerTag)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = ETOI(LevelType::Loading);
    desc.jsonPath = Constants::UI::Loading_UI_Json_Path;
    desc.animationPath = Constants::UI_Anim::UI_LoadingAnim_Path;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    return S_OK;
}

Shared<Level_Loading> Level_Loading::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, LevelType eNextLevelID)
{
    auto pInstance = make_shared<Level_Loading>(device, context);

    if (FAILED(pInstance->Initialize(eNextLevelID)))
    {
        MSG_BOX("Failed to Created : Level_Loading");
        return nullptr;
    }
    return pInstance;
}

void Level_Loading::Free()
{
    if (_loader)
    {
        while (!_loader->Is_WorkerDone())
        {
            this_thread::sleep_for(chrono::milliseconds(1));
        }
    }

    __super::Free();
}
