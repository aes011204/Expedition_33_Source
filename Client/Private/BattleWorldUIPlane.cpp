#include "BattleWorldUIPlane.h"
#include "GameInstance.h"
#include "ShaderCom.h"
#include "VIBuffer_Rect.h"

IMPLEMENT_REFLECTION(BattleWorldUIPlane)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "BattleWorldUIPlane";
    info.category = "UI";

    return true;
}

BattleWorldUIPlane::BattleWorldUIPlane(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

BattleWorldUIPlane::BattleWorldUIPlane(const BattleWorldUIPlane& prototype)
    : GameObject{ prototype }
{
}

HRESULT BattleWorldUIPlane::Initialize_Prototype()
{
    return S_OK;
}

HRESULT BattleWorldUIPlane::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);

    BATTLEWORLDUIPLANE_DESC desc{};
    if (arg != nullptr)
        desc = *static_cast<BATTLEWORLDUIPLANE_DESC*>(arg);

    _texturePath = desc.texturePath;
    _worldPosition = desc.worldPosition;
    _worldSize = desc.worldSize;
    _tintColor = desc.tintColor;
    _billboardMode = desc.billboardMode;
    _useDepthRead = desc.useDepthRead;
    _visible = desc.visible;
    _renderReady = false;

    CHECK_FAILED(Ready_Components(), E_FAIL);
    CHECK_FAILED(Load_Texture(_texturePath), E_FAIL);

    _renderReady = true;
    Apply_WorldTransform();

    return S_OK;
}

HRESULT BattleWorldUIPlane::Render()
{
    if (!_renderReady || !_visible)
        return S_OK;

    CHECK_FAILED(Bind_ShaderResources(), E_FAIL);

    CHECK_FAILED(_shader->Begin(_useDepthRead ? 0 : 1), E_FAIL);
    CHECK_FAILED(_viBuffer->Bind_Resources(), E_FAIL);
    CHECK_FAILED(_viBuffer->Render(), E_FAIL);

    return S_OK;
}

void BattleWorldUIPlane::Set_WorldPosition(const Vec3& position)
{
    _worldPosition = position;
}

void BattleWorldUIPlane::Set_WorldSize(const Vec2& size)
{
    _worldSize = size;
}

void BattleWorldUIPlane::Set_TintColor(const Vec4& color)
{
    _tintColor = color;
}

void BattleWorldUIPlane::Set_BillboardMode(BattleWorldUIPlaneBillboardMode mode)
{
    _billboardMode = mode;
}

void BattleWorldUIPlane::Set_UseDepthRead(bool useDepthRead)
{
    _useDepthRead = useDepthRead;
}

void BattleWorldUIPlane::Set_Visible(bool visible)
{
    _visible = visible;
}

void BattleWorldUIPlane::Submit_Render()
{
    if (!_renderReady || !_visible)
        return;

    Apply_WorldTransform();

    GAME->Add_RenderGroup(RenderGroup::Blend, GetSharedPtr<GameObject>());
}

HRESULT BattleWorldUIPlane::Ready_Components()
{
    CHECK_FAILED(__super::Add_Component(
        ETOI(LevelType::Static),
        L"Shader_BattleWorldUIPlane",
        _shader), E_FAIL);

    CHECK_FAILED(__super::Add_Component(
        ETOI(LevelType::Static),
        L"VIBuffer_Rect",
        _viBuffer), E_FAIL);

    return S_OK;
}

HRESULT BattleWorldUIPlane::Load_Texture(const wstring& texturePath)
{
    _textureSRV.Reset();

    if (texturePath.empty())
        return E_FAIL;

    const fs::path resolvedPath = Resolve_TexturePath(texturePath);

    if (FAILED(CreateWICTextureFromFile(
        _device.Get(),
        resolvedPath.c_str(),
        nullptr,
        _textureSRV.GetAddressOf())))
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT BattleWorldUIPlane::Bind_ShaderResources()
{
    CHECK_NULL(_shader, E_FAIL);
    CHECK_NULL(_viBuffer, E_FAIL);
    CHECK_NULL(_textureSRV, E_FAIL);

    CHECK_FAILED(_transformCom->Bind_ShaderResource(_shader.get(), "g_WorldMatrix"), E_FAIL);
    CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix"), E_FAIL);
    CHECK_FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix"), E_FAIL);

    CHECK_FAILED(_shader->Bind_SRV("g_Texture", _textureSRV.Get()), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_TintColor", &_tintColor, sizeof(Vec4)), E_FAIL);

    return S_OK;
}

void BattleWorldUIPlane::Apply_WorldTransform()
{
    if (_transformCom == nullptr)
        return;

    _transformCom->Set_Position(_worldPosition);
    _transformCom->Set_Scale(Vec3(_worldSize.x, _worldSize.y, 1.f));

    Apply_BillboardRotation();
}

void BattleWorldUIPlane::Apply_BillboardRotation()
{
    if (_transformCom == nullptr)
        return;

    if (_billboardMode == BattleWorldUIPlaneBillboardMode::FixedRotation)
        return;

    const Vec4* cameraPosition4 = GAME->Get_CamPosition();
    if (cameraPosition4 == nullptr)
        return;

    Vec3 cameraPosition{ cameraPosition4->x, cameraPosition4->y, cameraPosition4->z };

    if (_billboardMode == BattleWorldUIPlaneBillboardMode::FixedYaw)
        cameraPosition.y = _worldPosition.y;

    _transformCom->LookAt(cameraPosition);
}

fs::path BattleWorldUIPlane::Resolve_TexturePath(const wstring& texturePath) const
{
    fs::path resolvedPath = texturePath;

    if (!resolvedPath.is_absolute())
        resolvedPath = fs::path(GAME->Get_AssetRoot()) / resolvedPath;

    return resolvedPath.lexically_normal();
}

Shared<BattleWorldUIPlane> BattleWorldUIPlane::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<BattleWorldUIPlane>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> BattleWorldUIPlane::Clone(void* arg)
{
    auto instance = make_shared<BattleWorldUIPlane>(*this);

    if (FAILED(instance->Initialize(arg)))
        return nullptr;

    return instance;
}

void BattleWorldUIPlane::Free()
{
    __super::Free();
}
