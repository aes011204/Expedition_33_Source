#pragma once

#include "Client_Defines.h"
#include "PartObject.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
class AnimComputeRuntime;
NS_END

NS_BEGIN(Client)

struct WeaponSourceHistoryAnchorDesc
{
    string name{ "Source" };
    string boneName{};
    Vec3 localPosition{ Vec3::Zero };
};

class Weapon final : public PartObject
{
    GENERATED_GAMEOBJECT(Weapon, ObjectType::Weapon)

public:
    typedef struct tagWeaponDesc final : public PartObject::PARTOBJECT_DESC
    {
        ModelType       modelType{ ModelType::NonAnim };
        const wchar_t*  modelTag{ nullptr };
        const Matrix*   socketMatrix{ nullptr };

        // 각 무기별 크기, 위치는 프리펩에서 조절하십쇼
        bool            useBodyFollow{ false };
        Vec3            bodyFollowOffset{ Vec3::Zero };
        bool            applyAttachScaleCompensation{ false }; // WP prefab 단독 보기에서는 0.1 유지, 장착 runtime에서만 x10 보정

    }WEAPON_DESC;

public:
    Weapon(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Weapon(const Weapon& prototype);
    virtual ~Weapon() = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Priority_Update(float timeDelta) override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;
    Matrix  Get_GizmoWorldMatrix() const override;
    bool    Apply_GizmoWorldMatrix(const Matrix& gizmoWorldMatrix) override;

    json    To_Json() const override;
    void    From_Json(const json& data) override;

public:
    const Matrix* Get_SocketBoneMatrixPtr(const string& boneName) const;
    Shared<ModelCom> Get_Model() const { return _model; }
    const wstring& Get_TrailEffectName() const { return _trailEffectName; }
    vector<WeaponSourceHistoryAnchorDesc>& Get_SourceHistoryAnchors() { return _sourceHistoryAnchors; }
    const vector<WeaponSourceHistoryAnchorDesc>& Get_SourceHistoryAnchors() const { return _sourceHistoryAnchors; }
    const WeaponSourceHistoryAnchorDesc* Find_SourceHistoryAnchor(const string& anchorName) const;

    // 구스타브 총돌리기
    bool    Begin_NotifyRotation();
    void    Apply_NotifyRotation(const Vec3 axisLocal, float degree);
    void    End_NotifyRotation();

private:
    HRESULT Ready_Components(const WEAPON_DESC& desc);
    HRESULT Ready_DefaultComponents(); // desc 없이 생성
    HRESULT Bind_ShaderResources();
    HRESULT Ensure_RuntimeComponentsFromJson(const json& data);

    // 프리펩에서 Scale을 1로하면 너무 커서, 0.1로 하고 런타임 도중에 보정
    void    Apply_AttachScale();

    bool    Is_LuneHoverWeapon() const;
    float   Get_LuneHoverOffsetY() const;

private:
    Shared<ShaderCom>    _shader = { nullptr };
    Shared<ModelCom>     _model = { nullptr };

private:
    const Matrix*       _socketMatrix = { nullptr };
    bool                _useBodyFollow = false;
    Vec3                _bodyFollowOffset = Vec3::Zero;
    bool                _applyAttachScaleCompensation = false;

    wstring             _trailEffectName = L""; // 무기 프리펩이 들고 있는 기본 Trail 이펙트 이름
    vector<WeaponSourceHistoryAnchorDesc> _sourceHistoryAnchors{};

    bool _notifyRotationActive = false;
    Quat _notifyRotationBaseLocal = Quat::Identity;
    float _hoverTimeSec = 0.f;

public:
    static Shared<Weapon> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* pArg) override;
    virtual void Free() override;

};

NS_END
