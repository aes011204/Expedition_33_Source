#pragma once

#include "Client_Defines.h"
#include "Player_WorldBase.h"

NS_BEGIN(Engine)
class CurveAnimationModifier;
class CurveClip;
NS_END

NS_BEGIN(Client)

class Head_Player;

class PW_Gustave final : public Player_WorldBase
{
    GENERATED_GAMEOBJECT(PW_Gustave, ObjectType::Player)

public:
	PW_Gustave(const ComPtr<Device>& device, const ComPtr<Context>& context);
	PW_Gustave(const PW_Gustave& prototype);
	virtual ~PW_Gustave() override = default;

public:
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;
    void    Update_SkillPanelFacialMorphPreview(float timeDelta, bool enabled);

private:
    HRESULT Ready_Modifier();
    bool    Ensure_DebugFacialMorphTest(Head_Player* head);
    void    Stop_DebugFacialMorphTest(Head_Player* head);
    void    Apply_DebugFacialEyeLook(Head_Player* head, float timeDelta);
    void    Reset_DebugFacialEyeLook(Head_Player* head);

protected:
    HairChainPhysicsDesc Get_HairChainPhysicsDesc() const override;

    const wchar_t* Get_BodyModelTag() const override { return L"Model_GustaveBody"; }
    const wchar_t* Get_HeadModelTag() const override { return L"Model_GustaveHead"; }
    const wchar_t* Get_HairModelTag() const override { return L"Model_GustaveHair"; }

    // 구스타브 수염
    virtual const wchar_t* Get_BeardModelTag() const { return L"Model_GustaveBeard"; }
    virtual const wchar_t* Get_MustacheModelTag() const { return L"Model_GustaveMustache"; }

private:
    Shared<Engine::CurveAnimationModifier> _debugFacialMorphModifier{};
    Shared<Engine::CurveClip> _debugFacialCurveClip{};
    float _debugFacialTimelineTimeSec = 0.f;
    Matrix _debugFacialEyeBindLocalL{};
    Matrix _debugFacialEyeBindLocalR{};
    int32 _debugFacialEyeBoneL = -1;
    int32 _debugFacialEyeBoneR = -1;
    bool _debugFacialEyeBindCaptured = false;
    bool _debugFacialMorphEnabled = false;

public:
	static Shared<PW_Gustave> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;

};

NS_END

