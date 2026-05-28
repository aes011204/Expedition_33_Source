#pragma once

#include "Engine_Define.h"

NS_BEGIN(Engine)

class ENGINE_DLL IBLBaker final
{
public:
    static bool Bake(const wstring& hdrPath, ComPtr<Device> device, ComPtr<CONTEXT> context);

    static bool BakeBRDFLUTOnly(const wstring& outputPath);
};

NS_END
