#include "GamePlayUI_SL.h"

#include "ClientInstance.h"
#include "SL_Base.h"
#include "SoulLikeController.h"
#include "UI_BossHpBar.h"

HRESULT GamePlayUI_SL::Initialize(uint32 levelIndex)
{
	 _levelIndex = levelIndex;
    return S_OK;


}

void GamePlayUI_SL::Update(float timeDelta)
{
	Update_BossHpBar(timeDelta);
}

void GamePlayUI_SL::Handle_EditorStopRuntime()
{
	if (_bossHpBar)
        _bossHpBar->Clear_Boss();
}

vector<RuntimeUIEntry> GamePlayUI_SL::Get_RuntimeUIEntries() const
{
	vector<RuntimeUIEntry> entries{};

    if (_bossHpBar)
        _bossHpBar->Append_RuntimeUIEntries(entries, "SoulLike", "BossHpBar");

    return entries;
}

bool GamePlayUI_SL::Ensure_BossHpBar()
{
	if (_bossHpBar)
        return true;

    _bossHpBar = UI_BossHpBar::Create(_levelIndex);
    return _bossHpBar != nullptr;
}

void GamePlayUI_SL::Update_BossHpBar(float timeDelta)
{
	if (!CLIENT->Is_SoulLikeBattleActive())
    {
        if (_bossHpBar)
            _bossHpBar->Clear_Boss();
        return;
    }

    const Shared<SoulLikeController> controller = CLIENT->Get_SoulLikeController();
    const Shared<SL_Base> monster = controller ? controller->Get_Monster() : nullptr;

    if (monster == nullptr)
    {
        if (_bossHpBar)
            _bossHpBar->Clear_Boss();
        return;
    }

    if (!Ensure_BossHpBar())
        return;

    BattleChar boss{};
    boss.statCom = monster->Get_Stat();

    _bossHpBar->Show_Boss(boss);
    _bossHpBar->Update(timeDelta);
}

Unique<GamePlayUI_SL> GamePlayUI_SL::Create(uint32 levelIndex)
{
	auto instance = make_unique<GamePlayUI_SL>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : GamePlayUI_SL");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void GamePlayUI_SL::Free()
{
    if (_bossHpBar)
    {
        _bossHpBar->Free();
        _bossHpBar.reset();
    }

    Base::Free();
}
