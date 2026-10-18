#pragma once

#include "Base.h"
#include <filesystem>

#include "Material_Define.h"
#include "Material_Types.h"

NS_BEGIN(Engine)

class ENGINE_DLL Material final : public Base
{
public:
	struct TextureSlot  
	{
		MaterialTextureSlot slot = MaterialTextureSlot::BaseColor;
		uint32 textureIndex{};

        // 인스펙터에서 실제 어떤 파일이 연결됐는지 보기 위한 경로
        wstring resolvedPath{};

        Vec2    uvScale{ 1.f, 1.f };
        Vec2    uvOffset{ 0.f, 0.f };
        float   uvRotation{ 0.f };
        float   blendFactor{ 1.f };
        uint32  uvChannel{};
        uint32  wrapMode{};

        bool    isOverride{};

		ComPtr<ID3D11ShaderResourceView> shaderResourceView{ nullptr };
	};

public:
	Material(const ComPtr<Device>& device, const ComPtr<Context>& context);
	~Material() override = default;

public:
	HRESULT Initialize(const char* modelPath, const ModelMaterialData& materialData, const vector<ModelTextureSlot>& slots, uint32 maxTextureTypeSize);
	HRESULT Bind_Material(class ShaderCom* shader, const char* constantName, MaterialTextureSlot slot, uint32 textureIndex = 0);
	HRESULT Override_TextureSlot(MaterialTextureSlot slot, uint32 slotIndex, const wstring& texturePath);
    Shared<Material> Clone() const;

    void    Update_MaterialState();
    const string& Get_MaterialName() const { return _materialName; }
    uint32 Get_MaxTextureTypeSize() const { return _maxTextureTypeSize; }
    const vector<vector<TextureSlot>>& Get_TextureSlots() const { return _materials; }

    uint32 Get_TextureCount(MaterialTextureSlot slot) const;
    const TextureSlot* Find_TextureSlot(MaterialTextureSlot slot, uint32 textureIndex = 0) const;

public:
    MaterialShaderTemplate      Get_TemplateId() const { return _templateId; }
    const MaterialRenderState&  Get_RenderState() const { return _renderState; }
    const MaterialCB&           Get_CB() const  {
        return _cb;
    }
    MaterialCB&                 Get_CB() {
        return _cb;
    }
    uint32                      Get_PassIndex();
    int32                       Get_ForcedPassIndex() const { return _forcedPassIndex; }
    
    void Set_TemplateId(MaterialShaderTemplate templateId) { _templateId = templateId; }
    void Set_RenderState(const MaterialRenderState& renderState);
    void Set_CB(const MaterialCB& cb);
    void Set_ShaderType(ShaderType shaderType);
    void Set_ForcedPassIndex(int32 forcedPassIndex);

private:
	std::wstring    Find_TextureFile(const fs::path& searchRoot, const string& fileName);
    fs::path        Resolve_TextureLoadPath(const fs::path& texturePath) const;

	HRESULT         Create_TextureSRV(const fs::path& texturePath, ID3D11ShaderResourceView** outSrv);
    void            Refresh_PassIndex();

private:
    string                          _materialName;

	uint32							_maxTextureTypeSize{};
	ComPtr<Device>					_device = { nullptr };
	ComPtr<Context>					_context = { nullptr };
	vector<vector<TextureSlot>>		_materials;

    MaterialShaderTemplate          _templateId = MaterialShaderTemplate::OpaquePBR;
    MaterialRenderState             _renderState{};
    MaterialCB                      _cb{};

    ShaderType                      _shaderType = ShaderType::VtxTex;
    uint32                          _passIndex = 0;
    bool                            _passDirty = true;

    // -1이면 자동, 0 이상이면 해당 인덱스로 강제한다.
    int32                           _forcedPassIndex = -1;

public:
	static Shared<Material> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const char* modelPath, const ModelMaterialData& materialData, const vector<ModelTextureSlot>& slots, uint32 maxTextureTypeSize);
    static size_t Get_TextureSRVCacheSize();
    static void Clear_TextureSRVCache();
	void Free() override;
};

NS_END
