#ifndef Engine_Define_h__
#define Engine_Define_h__

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXCollision.h>
#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <cwctype>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <stdint.h>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <set>

#pragma warning(disable : 4251 4275)

#include <directxtk/DDSTextureLoader.h>
#include <directxtk/Effects.h>
#include <directxtk/PrimitiveBatch.h>
#include <directxtk/ScreenGrab.h>
#include <directxtk/SimpleMath.h>
#include <directxtk/SpriteBatch.h>
#include <directxtk/SpriteFont.h>
#include <directxtk/VertexTypes.h>
#include <directxtk/WICTextureLoader.h>
#include <Effects11/d3dx11effect.h>
#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
//#include <fmod.hpp>

// phiscx
#include "PxPhysicsAPI.h"

#include "Engine_Typedef.h"
#include "Engine_Enum.h"
#include "Engine_Macro.h"
#include "Engine_Struct.h"
#include "Engine_Function.h"
#include "Vertex_Types.h"
#include "Engine_Model_Struct.h"
#include "Engine_Constants.h"

//## Windows SDK 라이브러리 링커
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

//## 외부 라이브러리 링커
// FMOD: Configuration에 따라 Debug 또는 Release 라이브러리 링크
//#ifdef _DEBUG
//#pragma comment(lib, "fmodL_vc.lib")
//#else
//#pragma comment(lib, "fmod_vc.lib")
//#endif

using namespace std;
namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace Engine;
using namespace DirectX;
using namespace physx;

#endif // Engine_Define_h__
