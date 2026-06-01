#include "ANS_QTE.h"

#include "BattleController.h"
#include "ClientInstance.h"

IMPLEMENT_REFLECTION(ANS_QTE)
{
	auto& info = GetStaticReflectionInfo();
	info.properties.clear();

	PROPERTY_FLOAT("Intro Duration", _introDuration);
	PROPERTY_FLOAT("Progress Duration", _progressDuration);
	PROPERTY_FLOAT("Perfect Start Ratio", _perfectStartRatio);
	PROPERTY_FLOAT("Perfect End Ratio", _perfectEndRatio);

	PROPERTY_ENUM("QTE 프리셋을 골라", _qtePreset, BattleQTEPreset);

	return true;
}

void ANS_QTE::On_Begin(const AnimNotifyContext& context)
{
	if (context.isPreview)
		return;

	auto battle = CLIENT->Get_BattleController();
	CHECK_NULL(battle);

	battle->Begin_QTE(Make_Request());
}

void ANS_QTE::On_Tick(const AnimNotifyContext& context)
{
	UNREFERENCED_PARAMETER(context);
}

void ANS_QTE::On_End(const AnimNotifyContext& context)
{
	UNREFERENCED_PARAMETER(context);

	// QTE 실패 판정은 NotifyState 길이가 아니라 UI_QTE의 progressDuration 기준으로 처리한다.
	// 여기서 Fail_QTEIfActive()를 호출하면 ANS 박스 길이에 따라 QTE가 조기 실패할 수 있어서 변경

	// TODO : 스킬의 QTE마다 디테일하게 시간 조절하는건 프로토타입 이후 진행해야할거같다.
}

BattleQTERequest ANS_QTE::Make_Request() const
{
	BattleQTERequest request{};
	request.introDuration = _introDuration;
	request.progressDuration = _progressDuration;
	request.perfectStartRatio = _perfectStartRatio;
	request.perfectEndRatio = _perfectEndRatio;

	request.placementPreset = _qtePreset;
	request.screenPosition = Resolve_ScreenPosition(_qtePreset);
	request.screenScale = Resolve_ScreenScale(_qtePreset);

	return request;
}

Vec2 ANS_QTE::Resolve_ScreenPosition(BattleQTEPreset preset) const
{
	// TEMP. TODO : 프로토타입 이후 하드코딩 형식 변경하기

	switch (preset)
	{
	case BattleQTEPreset::LumiereAssault_QTE1: return Vec2(1000.f, 450.f);
	case BattleQTEPreset::LumiereAssault_QTE2: return Vec2(800.f, 450.f);

	case BattleQTEPreset::UnleashedCharge_QTE1: Vec2(800.f, 450.f);
    case BattleQTEPreset::UnleashedCharge_QTE2: break;

    case BattleQTEPreset::MarkingShot: return Vec2(815.f, 455.f);

    case BattleQTEPreset::Immolation_QTE1: return Vec2(850.f, 500.f);
    case BattleQTEPreset::Immolation_QTE2: return Vec2(950.f, 400.f);

    case BattleQTEPreset::IceGust_QTE1: return Vec2(815.f, 455.f);

    case BattleQTEPreset::Thunderfall_QTE1: return Vec2(850.f, 500.f);
    case BattleQTEPreset::Thunderfall_QTE2: return Vec2(950.f, 400.f);

    case BattleQTEPreset::OffensiveSwitch_QTE1: return Vec2(815.f, 455.f);

    case BattleQTEPreset::Percee_QTE1: return Vec2(815.f, 455.f);

    case BattleQTEPreset::FleureFury_QTE1: return Vec2(900.f, 500.f);

	case BattleQTEPreset::Center:
	default:
		return Vec2(800.f, 450.f);
    }
}

float ANS_QTE::Resolve_ScreenScale(BattleQTEPreset preset) const
{
	switch (preset)
	{
	case BattleQTEPreset::LumiereAssault_QTE2:
		return 1.f;

	case BattleQTEPreset::LumiereAssault_QTE1:
	case BattleQTEPreset::Center:
	default:
		return 1.f;
	}
}
