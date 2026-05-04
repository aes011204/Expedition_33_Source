#ifndef PCH_H
#define PCH_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <memory>
#include <filesystem>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <ImGuizmo.h>
#include <implot.h>

#include <Assimp/Importer.hpp>
#include <Assimp/scene.h>
#include <Assimp/postprocess.h>

#ifdef _DEBUG
#pragma comment(lib, "assimp-vc143-mtd.lib")
#else
#pragma comment(lib, "assimp-vc143-mt.lib")
#endif

#include "Engine_Define.h"


#define BYTE(_DATA_)    reinterpret_cast<const char*>(_DATA_)

#include "ModelEditor_Const.h"

namespace ModelEditor
{
    using namespace std;
    using namespace Engine;
    using namespace String;
}
using namespace ModelEditor;


#endif //PCH_H
