#include "BattleWorldUI.h"
#include "GameInstance.h"
#include "ShaderCom.h"
#include "VIBuffer_Rect.h"

IMPLEMENT_REFLECTION(BattleWorldUI)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "BattleWorldUI";
    info.category = "UI";

    return true;
}

BattleWorldUI::BattleWorldUI(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

BattleWorldUI::BattleWorldUI(const BattleWorldUI& prototype)
    : GameObject{ prototype }
{
}

HRESULT BattleWorldUI::Initialize_Prototype()
{
    return S_OK;
}

HRESULT BattleWorldUI::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);

    BATTLEWORLDUI_DESC desc{};
    if (arg != nullptr)
        desc = *static_cast<BATTLEWORLDUI_DESC*>(arg);

    _worldPosition = desc.worldPosition;
    _worldSize = desc.worldSize;
    _worldRotation = desc.worldRotation;
    _tintColor = desc.tintColor;
    _uvRect = desc.uvRect;
    _billboardMode = desc.billboardMode;
    _useDepthRead = desc.useDepthRead;
    _visible = desc.visible;
    _renderReady = false;

    CHECK_FAILED(Ready_Components(), E_FAIL);

    _renderReady = true;
    Apply_WorldTransform();

    return S_OK;
}

HRESULT BattleWorldUI::Render()
{
    if (!_renderReady || !_visible || _textureSRV == nullptr)
        return S_OK;

    CHECK_FAILED(Bind_ShaderResources(), E_FAIL);

    CHECK_FAILED(_shader->Begin(_useDepthRead ? 0 : 1), E_FAIL);
    CHECK_FAILED(_viBuffer->Bind_Resources(), E_FAIL);
    CHECK_FAILED(_viBuffer->Render(), E_FAIL);

    return S_OK;
}

void BattleWorldUI::Set_TextureSRV(ID3D11ShaderResourceView* srv)
{
    _textureSRV = srv;
}

void BattleWorldUI::Set_UVRect(const Vec4& uvRect)
{
    _uvRect = uvRect;
}

void BattleWorldUI::Set_WorldPosition(const Vec3& position)
{
    _worldPosition = position;
}

void BattleWorldUI::Set_WorldSize(const Vec2& size)
{
    _worldSize = size;
}

void BattleWorldUI::Set_WorldRotation(const Vec3& rotation)
{
    _worldRotation = rotation;
}

void BattleWorldUI::Set_TintColor(const Vec4& color)
{
    _tintColor = color;
}

void BattleWorldUI::Set_BillboardMode(BattleWorldUIBillboardMode mode)
{
    _billboardMode = mode;
}

void BattleWorldUI::Set_UseDepthRead(bool useDepthRead)
{
    _useDepthRead = useDepthRead;
}

void BattleWorldUI::Set_Visible(bool visible)
{
    _visible = visible;
}

void BattleWorldUI::Submit_Render()
{
    if (!_renderReady || !_visible || _textureSRV == nullptr)
        return;

    Apply_WorldTransform();

    GAME->Add_RenderGroup(RenderGroup::Blend, GetSharedPtr<GameObject>());
}

HRESULT BattleWorldUI::Ready_Components()
{
    CHECK_FAILED(__super::Add_Component(
        ETOI(LevelType::Static),
        L"Shader_BattleWorldUI",
        _shader), E_FAIL);

    CHECK_FAILED(__super::Add_Component(
        ETOI(LevelType::Static),
        L"VIBuffer_Rect",
        _viBuffer), E_FAIL);

    return S_OK;
}

HRESULT BattleWorldUI::Bind_ShaderResources()
{
    CHECK_NULL(_shader, E_FAIL);
    CHECK_NULL(_viBuffer, E_FAIL);
    CHECK_NULL(_textureSRV, E_FAIL);

    CHECK_FAILED(_transformCom->Bind_ShaderResource(_shader.get(), "g_WorldMatrix"), E_FAIL);
    CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix"), E_FAIL);
    CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix"), E_FAIL);

    CHECK_FAILED(_shader->Bind_SRV("g_Texture", _textureSRV.Get()), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_TintColor", &_tintColor, sizeof(Vec4)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_UVRect", &_uvRect, sizeof(Vec4)), E_FAIL);

    return S_OK;
}

void BattleWorldUI::Apply_WorldTransform()
{
    if (_transformCom == nullptr)
        return;

    _transformCom->Set_Position(_worldPosition);
    _transformCom->Set_Scale(Vec3(_worldSize.x, _worldSize.y, 1.f));

    if (_billboardMode == BattleWorldUIBillboardMode::FixedRotation)
        _transformCom->Set_RotationEuler(_worldRotation);
    else
    {
        Apply_BillboardRotation();
        _transformCom->Add_LocalRotationEuler(_worldRotation);
    }
}

void BattleWorldUI::Apply_BillboardRotation()
{
    if (_transformCom == nullptr)
        return;

    if (_billboardMode == BattleWorldUIBillboardMode::FixedRotation)
        return;

    const Vec4* cameraPosition4 = GAME->Get_CamPosition();
    if (cameraPosition4 == nullptr)
        return;

    Vec3 cameraPosition{ cameraPosition4->x, cameraPosition4->y, cameraPosition4->z };

    if (_billboardMode == BattleWorldUIBillboardMode::FixedYaw)
        cameraPosition.y = _worldPosition.y;

    _transformCom->LookAt(cameraPosition);
}

Shared<BattleWorldUI> BattleWorldUI::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<BattleWorldUI>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> BattleWorldUI::Clone(void* arg)
{
    auto instance = make_shared<BattleWorldUI>(*this);

    if (FAILED(instance->Initialize(arg)))
        return nullptr;

    return instance;
}

void BattleWorldUI::Free()
{
    __super::Free();
}
