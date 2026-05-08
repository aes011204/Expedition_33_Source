#include "pch.h"
#include "UI_Editor_Instance.h"

#include "UI_Editor_ImGuiManager.h"
#include "UI_Editor_Manager.h"

NS_BEGIN(UIEditor)

IMPLEMENT_SINGLETON(UI_Editor_Instance)

HRESULT UI_Editor_Instance::Initialize_Editor(const UI_EDITOR_DESC& editorDesc, const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    _editorDesc = editorDesc;
    _device = device;
    _context = context;

    if (nullptr == _editorDesc.hWnd || nullptr == _device || nullptr == _context)
        return E_FAIL;

    _imguiManager = UI_Editor_ImGuiManager::Create(_editorDesc.hWnd, _device, _context);
    CHECK_NULL(_imguiManager, E_FAIL);

    _editorManager = UI_Editor_Manager::Create(_device, _context, _editorDesc.assetManagerReady);
    CHECK_NULL(_editorManager, E_FAIL);

    return S_OK;
}

void UI_Editor_Instance::Update_Editor(float timeDelta)
{
    if (nullptr != _imguiManager)
        _imguiManager->Update(timeDelta);
}

void UI_Editor_Instance::Render_Editor()
{
    if (nullptr != _editorManager)
        _editorManager->Render();

    if (nullptr != _imguiManager)
        _imguiManager->Render();
}

void UI_Editor_Instance::Render_FontPreviews()
{
    if (nullptr != _editorManager)
        _editorManager->Render_FontPreviews();
}

bool UI_Editor_Instance::Request_Close()
{
    if (nullptr == _editorManager)
        return true;

    return _editorManager->Request_Close();
}

void UI_Editor_Instance::Release_Editor()
{
    if (_editorManager)
    {
        _editorManager.reset();
    }

    if (_imguiManager)
    {
        _imguiManager->Free();
        _imguiManager.reset();
    }

    _context.Reset();
    _device.Reset();
    _editorDesc = {};
}

void UI_Editor_Instance::Free()
{
    Release_Editor();
    __super::Free();
}

NS_END
