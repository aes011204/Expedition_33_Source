#pragma once
#include"Editor_Window.h"

NS_BEGIN(MainEditor)
class BT_Tool : public Editor_Window
{
public:
    BT_Tool();
    virtual ~BT_Tool() = default;


public:
    HRESULT Initialize() override;
    void    Update(float timeDelta) override;
  //  void    Pre_Render() override;
    void    Render() override;

private:
   
private:
   
public:
    static Shared<BT_Tool> Create();
    virtual void Free();
};



NS_END

