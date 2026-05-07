#pragma once

namespace UIEditor
{
    class UI_Editor_Window
    {
    public:
        explicit UI_Editor_Window(const wstring& windowName);
        virtual ~UI_Editor_Window() = default;

    public:
        virtual HRESULT Initialize();
        virtual void Render();

    public:
        const wstring& Get_WindowName() const { return _windowName; }

        bool Is_Open() const { return _isOpen; }
        void Set_Open(bool isOpen) { _isOpen = isOpen; }

        bool Is_Focused() const { return _isFocused; }
        bool Is_Hovered() const { return _isHovered; }

    protected:
        wstring _windowName = {};
        bool _isOpen = true;
        bool _isFocused = false;
        bool _isHovered = false;
    };
}
