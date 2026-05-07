#pragma once
#include "Client_Defines.h"

NS_BEGIN(Client)

class MapStaticMesh;

class MapLoader final
{
public:
    static HRESULT Load(const ComPtr<Device>& device, const ComPtr<Context>& context, const wstring& filePath, uint32 levelIndex, const wstring& layerTag);
};

NS_END
