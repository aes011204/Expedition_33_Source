#pragma once

#include "Base.h"
#include "UI_Animation.h"
#include "UI_Transform.h"

NS_BEGIN(Engine)

inline Vec2 Rotate_UIPoint(const Vec2& point, float rotationRadians)
{
    const float cosAngle = cosf(rotationRadians);
    const float sinAngle = sinf(rotationRadians);

    return Vec2(
        point.x * cosAngle - point.y * sinAngle,
        point.x * sinAngle + point.y * cosAngle);
}

class ENGINE_DLL UI_Base abstract : public Base
{
public:
    struct UI_DESC
    {
        UI_Transform::UI_TRANSFORM_DESC transformDesc{};
        bool isOpenOnCreate{ true };
        int32 sortOrder{ 0 };
        float alphaMultiplier{ 1.f };
        vector<UI_AnimationClip> animationClips = {};
        vector<UI_KeyframeAnimationAsset> keyframeAnimationAssets = {};

        UI_RenderGroup renderGroup { UI_RenderGroup::Foreground };
    };

public:
    UI_Base();
    UI_Base(const UI_Base& Prototype);
    ~UI_Base() override = default;

public:
    virtual HRESULT Initialize_Prototype();
    virtual HRESULT Initialize(void* Arg);

    virtual void Open();
    virtual void Close();

    virtual void Update(float TimeDelta);
    virtual HRESULT Render();
    void Notify_EffectiveOpenState(bool isEffectivelyOpen);
    bool Play_UIAnimation(const wstring& animationName, bool restart = true); // 붙어 있는 keyframe animation asset 중 이름으로 골라 재생
    void Stop_UIAnimation();                                                   // 현재 재생 중인 UI animation 정지
    void Add_UIAnimationAsset(const UI_KeyframeAnimationAsset& animationAsset);

public:
    bool Is_Open() const { return _isOpen; }
    bool Is_EffectivelyOpen() const;
    int32 Get_SortOrder() const { return _sortOrder; }

    UI_Transform* Get_Transform() const { return _transform.get(); }
    Vec2 Get_ParentAnchorReferenceSize() const;
    Vec2 Get_ResolvedLocalPosition() const;
    Vec2 Get_ResolvedLocalTopLeft() const;
    Vec2 Get_ResolvedLocalSize() const;
    Vec2 Get_ScaledLocalCenter(const Vec2& localScale) const;
    Vec2 Get_WorldPosition() const;
    Vec2 Transform_LocalPointToWorld(const Vec2& localPoint) const;
    Vec2 Get_WorldScale() const;
    float Get_WorldRotationRadians() const;
    Shared<UI_Base> Get_Parent() const { return _parent.lock(); }
    void Set_Parent(const Shared<UI_Base>& parent);
    void Clear_Parent();
    void Refresh_OpenBaseTransform();
    virtual void Set_Color(const Vec4& color);
    virtual Vec4 Get_Color() const;
    virtual void Set_PatternUvOffset(const Vec2& patternUvOffset);
    virtual void Set_PatternUvScale(const Vec2& patternUvScale);
    virtual void Set_FillRatio(float fillRatio);
    void Set_AlphaMultiplier(float alphaMultiplier);
    float Get_AlphaMultiplier() const { return _alphaMultiplier; }
    float Get_EffectiveAlphaMultiplier() const;

    UI_RenderGroup Get_RenderGroup() const { return _renderGroup; }

protected:
    Vec4 Get_EffectiveRenderColor(const Vec4& color) const;
    virtual void On_EffectiveOpenChanged(bool isEffectivelyOpen);

protected:
    Unique<UI_Transform> _transform{};
    Weak<UI_Base> _parent{};
    UI_AnimationPlayer _animationPlayer{};
    bool _isOpen{ false };
    int32 _sortOrder{ 0 };
    float _alphaMultiplier{ 1.f };
    Vec2 _openBasePosition{ 0.f, 0.f };
    Vec2 _openBaseSize{ 100.f, 100.f };
    Vec2 _openBaseScale{ 1.f, 1.f };
    float _openBaseRotationRadians{ 0.f };
    float _openBaseAlphaMultiplier{ 1.f };
    bool _hasOpenBaseTransform{ false };
    bool _lastEffectiveOpen{ false };

    UI_RenderGroup _renderGroup { UI_RenderGroup::Foreground };

public:
    virtual Shared<UI_Base> Clone(void* Arg) = 0;
    void Free() override;
};

NS_END
