#pragma once

#include "Base.h"
#include "ImGui_Manager.h"

NS_BEGIN(Engine)
class GameInstance;
NS_END

NS_BEGIN(ModelEditor)

class ImGui_Manager;

class ModelEditorApp final : public Base
{
private:
	ModelEditorApp() = default;

public:
	virtual ~ModelEditorApp() = default;

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

private:
	ComPtr<Device>  _device;
	ComPtr<Context> _context;

	Unique<ImGui_Manager> _imguiManager;

public:
	static Unique<ModelEditorApp> Create();
	virtual void Free() override;
};

NS_END
