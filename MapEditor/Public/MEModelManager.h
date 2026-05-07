#pragma once
#include "MapEditorDefines.h"

NS_BEGIN(Engine)
class ModelCom;
NS_END

NS_BEGIN(MapEditor)

class MEModelManager
{
public:
    MEModelManager() = default;
    ~MEModelManager();

public:
    Shared<ModelCom> GetOrLoad(const ComPtr<Device>& device, const ComPtr<Context>& context, const wstring& modelPath);

private:
    map<wstring, Shared<ModelCom>> _cache;

public:
    void Unload();

public:
    static Unique<MEModelManager> Create();
};

NS_END
