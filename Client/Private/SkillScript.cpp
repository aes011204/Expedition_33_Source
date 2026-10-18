#include "pch.h"
#include "SkillScript.h"

namespace
{
    map<string, SkillScript::SkillScriptCreateFn>& SkillScriptRegistry()
    {
        static map<string, SkillScript::SkillScriptCreateFn> registry{};
        return registry;
    }
}

SkillScript::SkillScript()
{
}

bool SkillScript::Register_SkillScriptType(const string& className, SkillScriptCreateFn createFn)
{
    if (className.empty() || !createFn)
        return false;

    auto& registry = SkillScriptRegistry();
    if (registry.contains(className))
        return false;

    registry.emplace(className, std::move(createFn));
    return true;
}

Shared<SkillScript> SkillScript::Create_SkillScriptByClass(const string& className)
{
    auto& registry = SkillScriptRegistry();

    auto iter = registry.find(className);
    if (iter == registry.end())
        return nullptr;

    return iter->second();
}

void SkillScript::Free()
{
    Base::Free();
}
