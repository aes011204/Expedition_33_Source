#pragma once

#include "Base.h"
#include "UI_Animation.h"
#include "UI_Transform.h"

NS_BEGIN(Engine)

class ENGINE_DLL UI_Base abstract : public Base
{
public:
    struct UI_DESC
    {
        UI_Transform::UI_TRANSFORM_DESC transformDesc{};
        bool isOpenOnCreate{ true };
        int32 sortOrder{ 0 };
        vector<UI_AnimationClip> animationClips = {};
        vector<UI_KeyframeAnimationAsset> keyframeAnimationAssets = {};
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

public:
    bool Is_Open() const { return _isOpen; }
    bool Is_EffectivelyOpen() const;
    int32 Get_SortOrder() const { return _sortOrder; }

    UI_Transform* Get_Transform() const { return _transform.get(); }
    Vec2 Get_WorldPosition() const;
    void Set_Parent(const Shared<UI_Base>& parent);
    void Clear_Parent();
    virtual void Set_Color(const Vec4& color);
    virtual Vec4 Get_Color() const;

protected:
    Unique<UI_Transform> _transform{};
    Weak<UI_Base> _parent{};
    UI_AnimationPlayer _animationPlayer{};
    bool _isOpen{ false };
    int32 _sortOrder{ 0 };

public:
    virtual Shared<UI_Base> Clone(void* Arg) = 0;
    void Free() override;
};

NS_END
