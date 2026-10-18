#pragma once
#include "Level.h"
#include "EditorBlock.h"

NS_BEGIN(ModelEditor)

class Level_ModelEditor final : public Level
{
public:
	Level_ModelEditor(const ComPtr<Device>& device, const ComPtr<Context>& context);
	~Level_ModelEditor() override = default;

public:
	HRESULT Initialize() override;
	void Update(float timeDelta) override;
	virtual HRESULT Render() override;

private:
	HRESULT Ready_Layer_Camera(const wstring& layerTag);
	HRESULT Ready_Layer_Viewer(const wstring& layerTag);
	HRESULT Ready_Lights();


public:
	static Shared<Level_ModelEditor> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	void Free() override;
};

NS_END
