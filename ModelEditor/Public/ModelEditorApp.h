#pragma once

#include "Base.h"

NS_BEGIN(Engine)
class GameInstance;
NS_END

NS_BEGIN(ModelEditor)

class ModelImGui_Manager;

class ModelEditorApp final : public Base
{
private:
	ModelEditorApp() = default;

public:
	virtual ~ModelEditorApp();

public:
	HRESULT Initialize();
	void Priority_Update(float timeDelta);
	void Update(float timeDelta);
	void Late_Update(float timeDelta);
	HRESULT Render();

private:
	HRESULT Ready_Fonts();
	wstring Get_ClientResourceRoot() const;
	wstring Get_ClientResourcePath(const wchar_t* relativePath) const;
	wstring Get_RuntimeRoot() const;
	wstring Resolve_ShaderPath(const wchar_t* relativePath) const;

private:
	ComPtr<Device>  _device;
	ComPtr<Context> _context;

public:
	static Unique<ModelEditorApp> Create();
	virtual void Free() override;
};

NS_END
