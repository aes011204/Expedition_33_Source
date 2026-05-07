#include "Inspector_Factory.h"

#include "BT_Inspector.h"
#include "Transform_Inspector.h"
#include "TransformCom.h"
#include "ModelCom.h"
#include "Model_Inspector.h"

void Inspector_Factory::Initialize()
{
    // StaticClassNameA -> const char* | StaticClassName -> const wchar* (엔진 내부 저장용)
    Register_Inspector(TransformCom::StaticClassNameA(), Transform_Inspector::Create());


    Register_Inspector("ModelCom", ModelCom_Inspector::Create());
}

void Inspector_Factory::Register_Inspector(const string& key, const Shared<Component_Inspector>& inspector)
{
    if (key.empty() || inspector == nullptr)
        return;

    if (_inspectors.contains(key))
        return;

    _inspectors.emplace(key, inspector);
}

Shared<Component_Inspector> Inspector_Factory::Get_Inspector(const string& key) const
{
    const auto iter = _inspectors.find(key);
    if (iter == _inspectors.end())
        return nullptr;

    return iter->second;
}

Unique<Inspector_Factory> Inspector_Factory::Create()
{
    auto instance = make_unique<Inspector_Factory>();
    instance->Initialize();

    return instance;
}

void Inspector_Factory::Free()
{
    _inspectors.clear();
    Base::Free();
}
