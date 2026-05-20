#pragma once
#include "Base.h"

class CameraDirector final : public Base
{
public:
    CameraDirector();
    virtual ~CameraDirector();

public:
    void RequestGamePlay();
    void RequestPreview();
    void RequestBattleTurn();
    void RequestCinematic();

private:

public:
  //  static<>

};

