#include "MEModelManager.h"
#include "ModelCom.h"

MEModelManager::~MEModelManager()
{
    Unload();
}

Shared<ModelCom> MEModelManager::GetOrLoad(const ComPtr<Device>& device, const ComPtr<Context>& context, const wstring& modelPath)
{
    auto it = _cache.find(modelPath);
    if (it != _cache.end())
    {
        return static_pointer_cast<ModelCom>(it->second->Clone(nullptr));
    }

    string pathA = String::ToString(modelPath);
    auto prototype = ModelCom::Create(device, context, ModelType::NonAnim, pathA.c_str(), Matrix::CreateScale(0.01f));
    CHECK_NULL(prototype, nullptr);

    _cache[modelPath] = prototype;
    return static_pointer_cast<ModelCom>(prototype->Clone(nullptr));
}

void MEModelManager::Unload()
{
    _cache.clear();
}

Unique<MEModelManager> MEModelManager::Create()
{
    return make_unique<MEModelManager>();
}
