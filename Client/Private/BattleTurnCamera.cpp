#include "BattleTurnCamera.h"
#include "Character.h"
#include "GameInstance.h"

IMPLEMENT_REFLECTION(BattleTurnCamera)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "BattleTurnCamera";
    info.category = "Camera";

    return true;
}

BattleTurnCamera::BattleTurnCamera(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Camera(device, context)
{
}

BattleTurnCamera::BattleTurnCamera(const BattleTurnCamera& prototype)
    : Camera(prototype)
{
}

HRESULT BattleTurnCamera::Initialize_Prototype()
{
    return S_OK;
}

HRESULT BattleTurnCamera::Initialize(void* arg)
{
    CHECK_FAILED(Camera::Initialize(arg), E_FAIL);

    Set_Name(L"Battle Turn Camera");
    return S_OK;
}

void BattleTurnCamera::BeginPlay()
{
    Camera::BeginPlay();

    GAME->Add_Camera(GetSharedPtr<Camera>());
}

void BattleTurnCamera::Update(float timeDelta)
{
    if (!GAME->Is_ActiveCamera(GetSharedPtr<Camera>()))
        return;

    Update_TransformMatrices();
}

void BattleTurnCamera::Focus_Battle(const vector<BattleChar>& heroes, const vector<BattleChar>& enemies)
{
    Vec3 center = Vec3::Zero;
    uint32 count = 0;

    for (const BattleChar& hero : heroes)
    {
        if (hero.actor && hero.actor->Get_Transform())
        {
            center += hero.actor->Get_Transform()->Get_WorldPosition();
            ++count;
        }
    }

    for (const BattleChar& enemy : enemies)
    {
        if (enemy.actor && enemy.actor->Get_Transform())
        {
            center += enemy.actor->Get_Transform()->Get_WorldPosition();
            ++count;
        }
    }

    if (count == 0)
        return;

    center /= static_cast<float>(count);

    // 3대3 전체 구도
    Apply_LookAt(center + Vec3(0.f, 7.f, -13.f), center + Vec3(0.f, 1.2f, 0.f));
}

void BattleTurnCamera::Apply_LookAt(const Vec3& cameraPos, const Vec3& lookAt)
{
    _transformCom->Set_WorldPosition(cameraPos);
    _transformCom->LookAt(lookAt);

    Update_TransformMatrices();
}

Shared<BattleTurnCamera> BattleTurnCamera::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<BattleTurnCamera>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> BattleTurnCamera::Clone(void* arg)
{
    auto instance = make_shared<BattleTurnCamera>(*this);

    if (FAILED(instance->Initialize(arg)))
        return nullptr;

    return instance;
}

void BattleTurnCamera::Free()
{
    Camera::Free();
}
