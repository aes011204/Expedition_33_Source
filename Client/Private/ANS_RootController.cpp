#include "ANS_RootController.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "ModelCom.h"
#include "MotionScaleAnimationModifier.h"

namespace
{
constexpr wchar_t MotionScaleModifierTag[] = L"ANS_RootController";

string Trim_Copy(string value)
{
    value.erase(
        value.begin(),
        find_if(value.begin(), value.end(), [](unsigned char ch)
        {
            return !std::isspace(ch);
        }));

    value.erase(
        find_if(value.rbegin(), value.rend(), [](unsigned char ch)
        {
            return !std::isspace(ch);
        }).base(),
        value.end());

    return value;
}

vector<MotionScaleAnimationModifier::BoneScaleDesc> Build_BoneScales(
    const string& boneNames,
    const Vec3& translationScale)
{
    vector<MotionScaleAnimationModifier::BoneScaleDesc> result{};

    stringstream stream(boneNames);
    string boneName{};

    while (getline(stream, boneName, ','))
    {
        boneName = Trim_Copy(boneName);
        if (boneName.empty())
            continue;

        MotionScaleAnimationModifier::BoneScaleDesc desc{};
        desc.boneName = boneName;
        desc.translationScale = translationScale;
        result.push_back(desc);
    }

    return result;
}
}

IMPLEMENT_REFLECTION(ANS_RootController)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "ANS_RootController";
    info.category = "SoulLike";

    PROPERTY_STRING("Bone Names", _boneNames);
    PROPERTY_VEC3("Translation Scale", _translationScale, 0.05f);
    PROPERTY_FLOAT("Blend In Sec", _blendInSec, 0.f, 1.f, 0.01f);
    PROPERTY_FLOAT("Blend Out Sec", _blendOutSec, 0.f, 1.f, 0.01f);

    return true;
}

void ANS_RootController::On_Begin(const AnimNotifyContext& context)
{
    Apply_Control(context);
}

void ANS_RootController::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
}

void ANS_RootController::On_End(const AnimNotifyContext& context)
{
    if (context.model == nullptr)
        return;

    auto modifier = static_cast<MotionScaleAnimationModifier*>(
        context.model->Find_AnimModifierByTag(MotionScaleModifierTag));

    if (modifier != nullptr)
        modifier->End_Control();
}

void ANS_RootController::Apply_Control(const AnimNotifyContext& context) const
{
    if (context.model == nullptr)
        return;

    auto modifier = static_cast<MotionScaleAnimationModifier*>(
        context.model->Find_AnimModifierByTag(MotionScaleModifierTag));

    if (modifier == nullptr)
    {
        MotionScaleAnimationModifier::MotionScaleDesc desc{};
        desc.modifierTag = MotionScaleModifierTag;
        desc.strengthAlpha = 1.f;

        Shared<MotionScaleAnimationModifier> created =
            MotionScaleAnimationModifier::Create(&desc);

        if (created == nullptr)
            return;

        context.model->Add_AnimModifier(created);
        modifier = created.get();
    }

    modifier->Begin_Control(
        context.model,
        Build_BoneScales(_boneNames, _translationScale),
        _blendInSec,
        _blendOutSec);
}
