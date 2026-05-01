//#pragma once
//#include "FreeCamera.h"
//
//NS_BEGIN(Editor)
//class EditorCamera : public Camera
//{
//public:
//    typedef struct tagEditorCamera final : public Camera::CAMERA_DESC
//    {
//    }EDITORCAMERA_DESC;
//
//public:
//    EditorCamera(const ComPtr<Device>& device, const ComPtr<Context>& context);
//    EditorCamera(const FreeCamera& prototype);
//    virtual ~EditorCamera() { Free();}
//
//public:
//    virtual HRESULT Initialize_Prototype() override;
//    virtual HRESULT Initialize(void* pArg) override;
//    virtual void    Priority_Update(float fTimeDelta) override;
//    virtual void    Update(float fTimeDelta) override;
//    virtual void    Late_Update(float fTimeDelta) override;
//    virtual HRESULT Render() override;
//
//private:
//    float       _mouseSensor = {};
//public:
//    static Shared<EditorCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
//    virtual Shared<GameObject> Clone(void* pArg) override;
//    virtual void Free() override;
//};
//
//NS_END
