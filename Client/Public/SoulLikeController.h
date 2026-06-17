#pragma once

#include "Battle_Event.h"
#include "GameObject.h"

NS_BEGIN(Client)

class SL_Maelle;
class BattleTurnCamera;

class SoulLikeController final : public GameObject
{
    GENERATED_GAMEOBJECT(SoulLikeController, ObjectType::SoulLikeController)

public:
    enum class BattleState
    {
        None,
        EnterFx,
        Playing,
        Finished,
    };

public:
    SoulLikeController(const ComPtr<Device>& device, const ComPtr<Context>& context);
    SoulLikeController(const SoulLikeController& prototype);
    ~SoulLikeController() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;
    void    Update(float timeDelta) override;

public:
    bool Begin_Battle();
    void End_Battle();

    bool Is_BattleActive() const
    {
        return _state == BattleState::EnterFx || _state == BattleState::Playing;
    }

    void Handle_BattleShotNotify(
        const AnimNotifyContext& context,
        BattleCameraMode cameraMode,
        const string& presetId,
        const string& clipId,
        bool useClip,
        bool restartClip,
        bool keepCurrentClip);

private:
    HRESULT Ready_BattleTurnCamera();
    void    Request_TargetCamera();
    void    Request_BattleShotCamera(const EvtBattlePresentation& event);
    void    Restore_TargetCamera();

private:
    bool Spawn_Player();
    void Update_EnterFx(float timeDelta);
    void Update_Player(float timeDelta);
    void Check_BattleEnd();

private:
    string _playerPrefab = "SL_Maelle";
    Vec3   _playerSpawnPosition = Vec3(0.f, 0.f, 0.f);
    Vec3   _playerSpawnRotation = Vec3(0.f, 180.f, 0.f);

    float  _enterFxDelay = 0.6f;
    float  _playerMaxHp = 100.f;
    bool   _enableDebugKeyStart = true;

private:
    Shared<BattleTurnCamera> _battleTurnCamera;

    BattleState _state = BattleState::None;
    Weak<SL_Maelle> _player{};

    float _enterFxTimer = 0.f;
    bool  _playerSpawned = false;

public:
    static Shared<SoulLikeController> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
