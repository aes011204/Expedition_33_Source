#pragma once

#include "Camera.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

class BattleTurnCamera final : public Camera
{
    GENERATED_GAMEOBJECT(BattleTurnCamera, ObjectType::BattleTurnCamera)

public:
    using BATTLE_TURN_CAMERA_DESC = Camera::CAMERA_DESC;

public:
    BattleTurnCamera(const ComPtr<Device>& device, const ComPtr<Context>& context);
    BattleTurnCamera(const BattleTurnCamera& prototype);
    ~BattleTurnCamera() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;
    void    Update(float timeDelta) override;

public:
    void Focus_Battle(const vector<BattleChar>& heroes, const vector<BattleChar>& enemies);

    void Update_FreeAimRotation(float timeDelta);

    Vec3 Get_AimOrigin() const;                // 히트스캔 시작 위치
    Vec3 Get_AimDirection() const;             // 히트스캔 방향

    bool Is_FreeAimActive() const { return _isFreeAimActive; }

private:
    void Apply_LookAt(const Vec3& cameraPos, const Vec3& lookAt);
    void Apply_FreeAimCamera();                

private:
    bool  _isFreeAimActive = false;            

public:
    static Shared<BattleTurnCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
