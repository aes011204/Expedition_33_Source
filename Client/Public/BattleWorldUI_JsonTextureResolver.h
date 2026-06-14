#pragma once

#include "Base.h"
#include "Texture.h"

NS_BEGIN(Client)

namespace BattleWorldUI_JsonTextureResolver
{
    Shared<Texture> Load_TextureFromUITextureRef(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const string& textureGuid,
        const string& texturePath,
        const string& debugName);

    Shared<Texture> Load_TextureFromUIImage(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        const wstring& uiJsonPath,
        const string& imageName,
        const string& fallbackTextureGuid);
}

NS_END
