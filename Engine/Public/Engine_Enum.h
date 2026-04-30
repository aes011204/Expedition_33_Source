#pragma once

namespace Engine
{
	enum class WinMode { FULL, WIN };

	enum class Prototype { GAMEOBJECT, COMPONENT };

	enum class RenderGroup { PRIORITY, NONBLEND, BLEND, UI, END };
	
	enum class State { RIGHT, UP, LOOK, POSITION, END };
	
	enum class D3DTS { VIEW, PROJ, END };

	enum class LightType { DIRECTIONAL, POINT, END };

	enum class ModelType { NONANIM, ANIM, END };

}
