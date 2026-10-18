#include "Level.h"

#include "GameInstance.h"

Level::Level(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: _device { device }
	, _context { context }
{
}

HRESULT Level::Initialize()
{
	return S_OK;
}

void Level::BeginPlay()
{
}

void Level::Update(float timeDelta)
{
}

HRESULT Level::Render()
{
	return S_OK;
}

HRESULT Level::Try_BeginPlay()
{
    if (_hasBegunPlay)
        return S_OK;

    BeginPlay();
    _hasBegunPlay = true;

    return S_OK;
}

void Level::Free()
{
	__super::Free();
}




