#ifndef Engine_Define_h__
#define Engine_Define_h__

#include <d3d11.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <DirectXCollision.h>

#include "DirectXTK/DDSTextureLoader.h"
#include "DirectXTK/WICTextureLoader.h"
#include "DirectXTK/SpriteBatch.h"
#include "DirectXTK/SpriteFont.h"
#include "DirectXTK/ScreenGrab.h"

#include "Fx11/d3dx11effect.h"

#include "Assimp/scene.h"
#include "Assimp/Importer.hpp"
#include "Assimp/postprocess.h"


using namespace DirectX;

#include <vector>
#include <list>
#include <map>
#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <cwctype>

#include <nlohmann/json.hpp>

using namespace std;
namespace fs = std::filesystem;
using json = nlohmann::json;

#include "Engine_Enum.h"
#include "Engine_Macro.h"
#include "Engine_Struct.h"
#include "Engine_Typedef.h"
#include "Engine_Function.h"
#include "Vertex_Types.h"

#pragma warning(disable : 4251)

#ifdef _DEBUG

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#ifndef DBG_NEW 

#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ ) 
#define new DBG_NEW 

#endif
#endif


using namespace Engine;

#endif // Engine_Define_h__
