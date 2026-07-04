#include "pch.h"
#include <shellapi.h>
#include <commctrl.h>
#include "ModelConverter.h"

namespace ModelEditor
{
	namespace
	{
		constexpr double kAnimationKeyTimeEpsilon = 1e-6;

		float Calculate_KeyRatio(double trackTime, double startTime, double endTime)
		{
			const double range = endTime - startTime;
			if (range <= kAnimationKeyTimeEpsilon)
				return 0.f;

			return static_cast<float>((trackTime - startTime) / range);
		}

		aiVector3D Sample_VectorKey(const aiVectorKey* keys, uint32 numKeys, double trackTime, const aiVector3D& fallback)
		{
			if (keys == nullptr || numKeys == 0)
				return fallback;

			if (numKeys == 1 || trackTime <= keys[0].mTime)
				return keys[0].mValue;

			const uint32 lastIndex = numKeys - 1;
			if (trackTime >= keys[lastIndex].mTime)
				return keys[lastIndex].mValue;

			for (uint32 i = 0; i + 1 < numKeys; ++i)
			{
				if (trackTime <= keys[i + 1].mTime)
				{
					const float ratio = Calculate_KeyRatio(trackTime, keys[i].mTime, keys[i + 1].mTime);
					return keys[i].mValue + (keys[i + 1].mValue - keys[i].mValue) * ratio;
				}
			}

			return keys[lastIndex].mValue;
		}

		aiQuaternion Sample_QuaternionKey(const aiQuatKey* keys, uint32 numKeys, double trackTime, const aiQuaternion& fallback)
		{
			if (keys == nullptr || numKeys == 0)
				return fallback;

			if (numKeys == 1 || trackTime <= keys[0].mTime)
				return keys[0].mValue;

			const uint32 lastIndex = numKeys - 1;
			if (trackTime >= keys[lastIndex].mTime)
				return keys[lastIndex].mValue;

			for (uint32 i = 0; i + 1 < numKeys; ++i)
			{
				if (trackTime <= keys[i + 1].mTime)
				{
					const float ratio = Calculate_KeyRatio(trackTime, keys[i].mTime, keys[i + 1].mTime);
					aiQuaternion result{};
					aiQuaternion::Interpolate(result, keys[i].mValue, keys[i + 1].mValue, ratio);
					result.Normalize();
					return result;
				}
			}

			return keys[lastIndex].mValue;
		}

		vector<double> Build_ChannelKeyTimes(const aiNodeAnim* animNode)
		{
			vector<double> keyTimes;
			keyTimes.reserve(
				static_cast<size_t>(animNode->mNumPositionKeys) +
				static_cast<size_t>(animNode->mNumRotationKeys) +
				static_cast<size_t>(animNode->mNumScalingKeys));

			for (uint32 keyIndex = 0; keyIndex < animNode->mNumPositionKeys; ++keyIndex)
				keyTimes.push_back(animNode->mPositionKeys[keyIndex].mTime);

			for (uint32 keyIndex = 0; keyIndex < animNode->mNumRotationKeys; ++keyIndex)
				keyTimes.push_back(animNode->mRotationKeys[keyIndex].mTime);

			for (uint32 keyIndex = 0; keyIndex < animNode->mNumScalingKeys; ++keyIndex)
				keyTimes.push_back(animNode->mScalingKeys[keyIndex].mTime);

			ranges::sort(keyTimes);
			auto uniqueEnd = ranges::unique(keyTimes, [](double left, double right) {
				return std::abs(left - right) <= kAnimationKeyTimeEpsilon;
			});
			keyTimes.erase(uniqueEnd.begin(), uniqueEnd.end());

			if (keyTimes.empty())
				keyTimes.push_back(0.0);

			return keyTimes;
		}

	}

	LRESULT CALLBACK ModelConverter::DragDropWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
	{
		ModelConverter* thisPointer = reinterpret_cast<ModelConverter*>(dwRefData);

		switch (message)
		{
		case WM_DROPFILES:
		{
			::HDROP hDrop = reinterpret_cast<::HDROP>(wParam);
			uint32 fileCount = ::DragQueryFileW(hDrop, 0xffffffff, nullptr, 0);
            bool handled = false;

			for (uint32 i = 0; i < fileCount; ++i)
			{
				wchar_t filePath[MAX_PATH];
				::DragQueryFileW(hDrop, i, filePath, MAX_PATH);

				if (fs::path(filePath).extension() == ".fbx")
				{
                    handled = true;
					LOG_INFO("Drag & Drop Detected: {}", fs::path(filePath).filename().string());
					thisPointer->Convert_Model(filePath, fs::path(filePath).parent_path().wstring() + L"/");
				}
			}
            if (handled)
            {
                ::DragFinish(hDrop);
			    return 0;
            }

            break;
		}
		case WM_NCDESTROY:
			::RemoveWindowSubclass(hWnd, DragDropWndProc, uIdSubclass);
			break;
		}
		return ::DefSubclassProc(hWnd, message, wParam, lParam);
	}

	string ModelConverter::Sanitize_FileName(string name)
	{
		string result = name;

		// 1. Tech Junk Tokens (Exact matches only)
		static const unordered_set<string> junkSet = {
			"ALS", "UE5", "RETAKE", "UNREAL", "TAKE", "DEFAULT", "PLACEHOLDER", "SK",
			"LF", "RF", "N", "F", "B", "L", "R", "W", "S", "E",
            "AM", "PACK", "MCIDLESPACK", "RETARGETED", "RD", "A"
		};

		// 2. Tokenize by various delimiters
		vector<string> tokens;
		string current;
		auto processToken = [&](string t) {
			if (t.empty()) return;
			string upper = t;
			for (char& c : upper) c = toupper(c);
			if (junkSet.find(upper) == junkSet.end())
				tokens.push_back(t);
		};

		for (char c : result)
		{
			// Delimiters that separate words
			if (c == '_' || c == '|' || c == ' ' || c == '-' || c == ':' || c == '*' || c == '?' || c == '\"' || c == '<' || c == '>' || c == '/')
			{
				processToken(current);
				current.clear();
			}
			else
			{
				current += c;
			}
		}
		processToken(current);

		// 3. Rejoin tokens with single underscore and basic deduplication
		string finalResult;
		for (size_t i = 0; i < tokens.size(); ++i)
		{
			// 맨 앞의 문자열이 뒤에 또 존재하면 삭제 (예: Lancleier_Lancleier_Dash_Out -> Lancleier_Dash_Out)
			if (i > 0 && tokens[i] == tokens[0])
				continue;

			// Simple consecutive duplicate prevention (e.g., Maelle_Maelle -> Maelle)
			if (i > 0 && tokens[i] == tokens[i - 1])
				continue;

			if (!finalResult.empty())
				finalResult += "_";
			finalResult += tokens[i];
		}

		if (finalResult.empty())
		{
			finalResult = "DefaultAnimName";
		}

		return finalResult;
	}

	ModelConverter::ModelConverter() : EditorBlock{}
	{
	}

	ModelConverter::~ModelConverter()
	{
		if (_hWnd)
		{
			::RemoveWindowSubclass(_hWnd, DragDropWndProc, 0);
		}
		_hWnd = nullptr;
		ZeroMemory(&_pathBuffer, sizeof(char) * 512);
		Clear_Converter();
        _cachedFbxPaths.clear();
		LOG_INFO("ModelConverter Called");
	}

	HRESULT ModelConverter::Initialize()
	{
	        Clear_Converter();
	        LOG_INFO("ModelConverter Called");
	        _importFlags = aiProcess_ConvertToLeftHanded |
	                                  aiProcess_Triangulate |
	                                  aiProcess_CalcTangentSpace |
	                                  aiProcess_GenSmoothNormals |
	                                  aiProcess_LimitBoneWeights |
	                                  aiProcess_JoinIdenticalVertices |
                                      aiProcess_ImproveCacheLocality |
                                      aiProcess_OptimizeMeshes |
                aiProcess_RemoveRedundantMaterials |
                aiProcess_FindInvalidData;

		strcpy_s(_pathBuffer, Engine::String::ToString(ResourcePath).c_str());
		_resourcePath = ResourcePath;

		if (_hWnd)
		{
			::DragAcceptFiles(_hWnd, TRUE);
			::SetWindowSubclass(_hWnd, DragDropWndProc, 0, reinterpret_cast<::DWORD_PTR>(this));
		}

		return S_OK;
	}

	void ModelConverter::Update(float timeDelta)
	{
	}

	void ModelConverter::Render()
	{
        if (!_showBlock) return;

		ImGui::Begin("Model Converter");

		ImGui::Text("Resource Path:");
		if (ImGui::InputText("##ResPath", _pathBuffer, sizeof(_pathBuffer)))
		{
			_resourcePath = String::ToWString(_pathBuffer);
		}
		ImGui::Separator();

		ImGui::Checkbox("Generate Debug JSON Logs", &_exportJsonLog);
        ImGui::Separator();
        ImGui::Checkbox("Export Model Data", &_exportModelData);
        ImGui::Separator();
        ImGui::Checkbox("Export Reversed Animation (Backward)", &_exportReversedAnimation);
        ImGui::Separator();
		ImGui::Checkbox("Apply Global Scale (Static Only)", &_applyStaticGlobalScale);
		ImGui::Separator();

		if (ImGui::Button("Scan FBX Files", ImVec2(150, 30)))
		{
			Scan_FbxFiles(_resourcePath);
		}
		ImGui::SameLine();
		if (ImGui::Button("Extract All", ImVec2(150, 30)))
		{
			Extract_All();
		}
		ImGui::Spacing();

		ImGui::Text("FBX Files");
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
		if (ImGui::BeginChild("FbxListRegion", ImVec2(0, 150), true, ::ImGuiWindowFlags_HorizontalScrollbar))
		{
			for (const auto& path : _cachedFbxPaths)
			{
				string fileName = path.filename().string();
				bool isSelected = (_selectedFbxPath == path.string());

				if (ImGui::Selectable(fileName.c_str(), isSelected, 0, ImVec2(150, 0)))
				{
					_selectedFbxPath = path.string();
				}

				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					Extract_Selected();
				}
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();

		if (!_selectedFbxPath.empty())
		{
			if (ImGui::Button("Convert Selected FBX", ImVec2(-1, 30)))
			{
				Extract_Selected();
			}
		}

		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));

		if (ImGui::BeginChild("DropInfo", ImVec2(0, 30), true, ::ImGuiWindowFlags_NoMove))
		{
			ImGui::Text("Can Drag & Drop FBX File to Window");
		}

		ImGui::EndChild();
		ImGui::PopStyleColor();
		ImGui::End();
	}

	void ModelConverter::Scan_FbxFiles(const wstring& path)
	{
		_cachedFbxPaths.clear();
		_selectedFbxPath.clear();

		if (!fs::exists(path) || !fs::is_directory(path))
		{
			LOG_ERROR("Invalid directory path: {}", String::ToString(path));
			return;
		}

		function<void(const wstring&)> findFiles = [&](const wstring& currentDir) {
			wstring searchPath = currentDir + L"\\*";
			WIN32_FIND_DATAW fd;
			HANDLE hFind = ::FindFirstFileW(searchPath.c_str(), &fd);

			if (hFind != INVALID_HANDLE_VALUE)
			{
				do {
					if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
						continue;

					wstring fullPath = (fs::path(currentDir) / fd.cFileName).lexically_normal().wstring();

					if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					{
						findFiles(fullPath); // 하위 디렉토리 재귀 탐색
					}
					else
					{
						size_t len = wcslen(fd.cFileName);
						if (len >= 4 && _wcsicmp(fd.cFileName + len - 4, L".fbx") == 0)
						{
							_cachedFbxPaths.push_back(fs::path(fullPath));
						}
					}
				} while (::FindNextFileW(hFind, &fd));
				::FindClose(hFind);
			}
		};

		findFiles(path);

		LOG_INFO(">>> Scanned {} FBX files.", _cachedFbxPaths.size());
	}

	void ModelConverter::Extract_Selected()
	{
		if (_selectedFbxPath.empty()) return;

		fs::path path(_selectedFbxPath);
		wstring fbxPath = path.wstring();
		wstring outputDir = path.parent_path().wstring() + L"/";
		LOG_INFO(">>> Start Lazy Conversion: {}", path.filename().string());

		if (SUCCEEDED(Convert_Model(fbxPath, outputDir)))
		{
			LOG_INFO(">>> Conversion Success: {}", path.filename().string());
		}
		else
		{
			LOG_ERROR(">>> Conversion Failed: {}", path.filename().string());
		}
	}

	void ModelConverter::Extract_All()
	{
		if (_cachedFbxPaths.empty()) return;

		LOG_INFO(">>> Start Batch Extraction: {} files", _cachedFbxPaths.size());

		uint32 successCount = 0;
		for (const auto& path : _cachedFbxPaths)
		{
			wstring fbxPath = path.wstring();
			wstring outputDir = path.parent_path().wstring();
			if (!outputDir.empty() && outputDir.back() != L'\\' && outputDir.back() != L'/')
				outputDir += L"/";

			if (SUCCEEDED(Convert_Model(fbxPath, outputDir)))
			{
				successCount++;
			}
		}

		LOG_INFO(">>> Batch Conversion Finished: {}/{} Success", successCount, _cachedFbxPaths.size());
	}

	HRESULT ModelConverter::Convert_Model(const wstring& fbxFilePath, const wstring& outputDir)
	{
		Clear_Converter();
		
        string utf8FilePath = Engine::String::ToString(fbxFilePath);
        
        // 정점 분할(Split) 강제 억제 설정 (Tangent 및 Normal 계산 시 미세한 각도 차이로 정점이 쪼개지는 현상 방지)
        _importer.SetPropertyFloat(AI_CONFIG_PP_CT_MAX_SMOOTHING_ANGLE, 175.0f);
        _importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 175.0f);

        const aiScene* probeScene = _importer.ReadFile(utf8FilePath,
            aiProcess_Triangulate | aiProcess_ConvertToLeftHanded);
        if (nullptr == probeScene || probeScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !probeScene->mRootNode)
        {
            LOG_ERROR("Assimp Error: {} \n", _importer.GetErrorString());
            return E_FAIL;
        }
        
        bool hasMeshBones = false;
        for (uint32 i = 0; i < probeScene->mNumMeshes; ++i)
        {
            if (probeScene->mMeshes[i]->mNumBones > 0)
            {
                hasMeshBones = true;
                break;
            }
        }
        _isSkeletal = hasMeshBones || probeScene->mNumAnimations > 0;
        
        uint32 finalFlags = _importFlags;
        if (true == _applyStaticGlobalScale && false == _isSkeletal)
        {
            finalFlags |= aiProcess_PreTransformVertices | aiProcess_GlobalScale;
        }
            
        _importer.FreeScene(); // 필수: 이전 probeScene 캐시를 날려야 새 플래그가 적용됨
        _aiScene = _importer.ReadFile(utf8FilePath, finalFlags);
        if (nullptr == _aiScene || _aiScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !_aiScene->mRootNode)
        {
            LOG_ERROR("Assimp Error: {} \n", _importer.GetErrorString());
            return E_FAIL;
        }
        string modelName = Get_FileNameWithoutExtension(fbxFilePath);

        for (uint32 i = 0; i < _aiScene->mNumMeshes; ++i)
        {
            if (_aiScene->mMeshes[i]->mNumBones > 0)
            {
                hasMeshBones = true;
                break;
            }
        }
        _isSkeletal = hasMeshBones || _aiScene->mNumAnimations > 0;

        // 1. 모든 메쉬를 순회하여 본 이름과 OffsetMatrix를 먼저 수집 (임시 저장)
        unordered_map<string, Matrix> tempOffsetMap;
        for (uint32 i = 0; i < _aiScene->mNumMeshes; ++i)
        {
            const aiMesh* aiMesh = _aiScene->mMeshes[i];
            for (uint32 j = 0; j < aiMesh->mNumBones; ++j)
            {
                string boneName = aiMesh->mBones[j]->mName.C_Str();
                if (!tempOffsetMap.contains(boneName))
                {
                    tempOffsetMap[boneName] = Convert_Matrix(aiMesh->mBones[j]->mOffsetMatrix);
                }
            }
        }

        // 2. 수집된 본 이름들을 바탕으로 계층 구조 순서대로 _bones 배열 채우기
        auto RecursiveBuild = [&](auto self, const aiNode* node, int32 parentBoneIndex) -> void {
            string nodeName = node->mName.C_Str();
            Matrix currentTransform = Convert_Matrix(node->mTransformation);

            ModelBoneData boneData{};
            boneData.name = nodeName;
            boneData.parentBoneIndex = parentBoneIndex;
            
            if (tempOffsetMap.contains(nodeName))
            {
                boneData.offsetMatrix = tempOffsetMap[nodeName];
            }
            else
            {
                boneData.offsetMatrix = Matrix::Identity;
            }
            
            boneData.localTransform = currentTransform;

            int32 currentBoneIndex = static_cast<int32>(_bones.size());
            _boneNameMap[nodeName] = currentBoneIndex;
            _bones.push_back(boneData);

            for (uint32 i = 0; i < node->mNumChildren; ++i)
                self(self, node->mChildren[i], currentBoneIndex);
        };

        RecursiveBuild(RecursiveBuild, _aiScene->mRootNode, -1);

        uint32 totalNodeCount = static_cast<uint32>(_bones.size());
        uint32 boneCount = static_cast<uint32>(tempOffsetMap.size());
        if (totalNodeCount > 0 && (totalNodeCount - boneCount) == 0)
        {
            _isSkeletal = false;
        }

        ReadMeshes(_aiScene);
        ReadMaterial(_aiScene); // 이게 빠져있어서 머티리얼 안들어왔음. 그래서 추가함
		ReadAnimations(_aiScene);
		Ensure_DirectoryExists(outputDir);

		if (_exportModelData)
		{
			if (FAILED(Export_ModelData(_aiScene, outputDir, modelName)))
			{
				return E_FAIL;
			}
		}

		if (FAILED(Export_AnimationData(_aiScene, outputDir, modelName)))
		{
			return E_FAIL;
		}

		if (_exportJsonLog)
		{
			Export_JsonLog(outputDir);
		}

		LOG_INFO("Convert Success: {}", modelName);

		return S_OK;
	}

	void ModelConverter::BuildBoneHierarchy(const aiNode* node, int32 parentBoneIndex, Matrix accumulatedTransform)
	{
		string nodeName = node->mName.C_Str();
		int32 currentBoneIndex = parentBoneIndex;
		Matrix currentTransform = Convert_Matrix(node->mTransformation);

		if (_boneNameMap.contains(nodeName))
		{
			currentBoneIndex = static_cast<int32>(_boneNameMap[nodeName]);
			_bones[currentBoneIndex].parentBoneIndex = parentBoneIndex;
			_bones[currentBoneIndex].localTransform = currentTransform * accumulatedTransform;
			accumulatedTransform = Matrix();
		}
		else
		{
			accumulatedTransform = currentTransform * accumulatedTransform;
		}

		for (uint32 i = 0; i < node->mNumChildren; ++i)
		{
			BuildBoneHierarchy(node->mChildren[i], currentBoneIndex, accumulatedTransform);
		}
	}

	void ModelConverter::ReadMeshes(const aiScene* aiScene)
	{
        for (uint32 i = 0; i < aiScene->mNumMeshes; ++i)
        {
            ModelMeshData meshData{};
            const aiMesh* aiMesh = aiScene->mMeshes[i];
            meshData.name = aiMesh->mName.C_Str();
            meshData.materialIndex = aiMesh->mMaterialIndex;
            meshData.numVertices = aiMesh->mNumVertices;
            meshData.numIndices = aiMesh->mNumFaces * 3;

            meshData.vertices = nullptr;
            meshData.indices = nullptr;
            meshData.boneIndices = nullptr;
            meshData.offsetMatrices = nullptr;

            vector<uint32> index;
            for (uint32 j = 0; j < aiMesh->mNumFaces; ++j)
            {
                for (uint32 k = 0; k < 3; ++k)
                {
                    index.push_back(aiMesh->mFaces[j].mIndices[k]);
                }
            }
            if (_isSkeletal)
            {
                vector<uint32> boneRefs;
                vector<Matrix> offsets;
                vector<VTXANIMMESH> vertices(aiMesh->mNumVertices, VTXANIMMESH{});

                for (uint32 j = 0; j < aiMesh->mNumVertices; ++j)
                {
                    vertices[j].position = Vec3{ aiMesh->mVertices[j].x, aiMesh->mVertices[j].y, aiMesh->mVertices[j].z };
                    if (aiMesh->HasNormals())
                    {
                        vertices[j].normal = Vec3{ aiMesh->mNormals[j].x, aiMesh->mNormals[j].y, aiMesh->mNormals[j].z };
                    }
                    if (aiMesh->HasTangentsAndBitangents())
                    {
                        vertices[j].tangent = Vec3{ aiMesh->mTangents[j].x, aiMesh->mTangents[j].y, aiMesh->mTangents[j].z };
                        vertices[j].binormal = Vec3{ aiMesh->mBitangents[j].x, aiMesh->mBitangents[j].y, aiMesh->mBitangents[j].z };
                    }
                    if (aiMesh->HasTextureCoords(0))
                    {
                        vertices[j].texcoord = Vec2{ aiMesh->mTextureCoords[0][j].x, aiMesh->mTextureCoords[0][j].y };
                    }
                }

				auto GetGlobalBindMatrix = [&](const aiNode* node) -> Matrix
				{
					Matrix mat = Matrix::Identity;
					const aiNode* curr = node;
					while (curr)
					{
						mat = mat * Convert_Matrix(curr->mTransformation);
						curr = curr->mParent;
					}
					return mat;
				};

				uint32 totalWeights = 0;
				for (uint32 j = 0; j < aiMesh->mNumBones; ++j)
				{
					totalWeights += aiMesh->mBones[j]->mNumWeights;
				}

				if (aiMesh->mNumBones == 0 || totalWeights == 0)
				{
					const aiNode* meshNode = nullptr;
					function<void(const aiNode*)> FindNode = [&](const aiNode* node)
					{
						if (meshNode)
						{
							return;
						}
						for (uint32 m = 0; m < node->mNumMeshes; ++m)
						{
							if (node->mMeshes[m] == i)
							{
								meshNode = node;
								return;
							}
						}
						for (uint32 c = 0; c < node->mNumChildren; ++c)
						{
							FindNode(node->mChildren[c]);
						}
					};
					FindNode(aiScene->mRootNode);

					string targetBoneName = aiScene->mRootNode->mName.C_Str();
					if (meshNode)
					{
						const aiNode* curr = meshNode;
						while (curr)
						{
							if (_boneNameMap.contains(curr->mName.C_Str()))
							{
								targetBoneName = curr->mName.C_Str();
								break;
							}
							curr = curr->mParent;
						}
					}

					Matrix meshGlobal = GetGlobalBindMatrix(meshNode);
					Matrix targetGlobalInv = GetGlobalBindMatrix(aiScene->mRootNode->FindNode(targetBoneName.c_str())).Invert();
					Matrix meshToBoneSpace = meshGlobal * targetGlobalInv;

					for (auto& vertex : vertices)
					{
						vertex.position = Vec3::Transform(vertex.position, meshToBoneSpace);
						vertex.normal = Vec3::TransformNormal(vertex.normal, meshToBoneSpace);
						if (aiMesh->HasTangentsAndBitangents())
						{
							vertex.tangent = Vec3::TransformNormal(vertex.tangent, meshToBoneSpace);
							vertex.binormal = Vec3::TransformNormal(vertex.binormal, meshToBoneSpace);
						}
						
						vertex.blendIndex.x = 0; 
						vertex.blendWeight.x = 1.f;
					}

					boneRefs.push_back(_boneNameMap[targetBoneName]);
					offsets.push_back(Matrix::Identity); 
				}
				else
				{
					for (uint32 j = 0; j < aiMesh->mNumBones; ++j)
					{
						const aiBone* aiBone = aiMesh->mBones[j];
						string boneName = aiBone->mName.C_Str();
						if (false == _boneNameMap.contains(boneName))
						{
							ModelBoneData boneData{};
							boneData.name = boneName;
							boneData.offsetMatrix = Convert_Matrix(aiBone->mOffsetMatrix);
							boneData.localTransform = Matrix();
							_boneNameMap[boneName] = static_cast<uint32>(_bones.size());
							_bones.push_back(boneData);
						}
						boneRefs.push_back(_boneNameMap[boneName]);
						offsets.push_back(Convert_Matrix(aiBone->mOffsetMatrix)); 
						for (uint32 k = 0; k < aiBone->mNumWeights; ++k)
						{
							uint32 vertexIndex = aiBone->mWeights[k].mVertexId;
							float weight = aiBone->mWeights[k].mWeight;
							auto& vertex = vertices[vertexIndex];
							if (vertex.blendWeight.x == 0.f)
							{
								vertex.blendIndex.x = j;
								vertex.blendWeight.x = weight;
							}
							else if (vertex.blendWeight.y == 0.f)
							{
								vertex.blendIndex.y = j;
								vertex.blendWeight.y = weight;
							}
							else if (vertex.blendWeight.z == 0.f)
							{
								vertex.blendIndex.z = j;
								vertex.blendWeight.z = weight;
							}
							else if (vertex.blendWeight.w == 0.f)
							{
								vertex.blendIndex.w = j;
								vertex.blendWeight.w = weight;
							}
						}

					}

                    int32 defaultBoneIndex = 0; // 기본은 Root로 설정
                    Matrix defaultOffset = Matrix::Identity;

                    if (!_bones.empty())
                    {
                        // 1. 현재 메쉬(인덱스 i)를 참조하고 있는 aiNode를 찾습니다.
                        const aiNode* meshNode = nullptr;
                        function<void(const aiNode*)> FindNode = [&](const aiNode* node) {
                            if (meshNode) return;
                            for (uint32 m = 0; m < node->mNumMeshes; ++m) {
                                if (node->mMeshes[m] == i) { meshNode = node; return; }
                            }
                            for (uint32 c = 0; c < node->mNumChildren; ++c) FindNode(node->mChildren[c]);
                            };
                        FindNode(_aiScene->mRootNode);

                        // 2. 해당 노드에서 부모로 거슬러 올라가며 가장 가까운 뼈(Bone)를 찾고, 오프셋을 계산합니다.
                        if (meshNode)
                        {
                            const aiNode* curr = meshNode;
                            Matrix accumulatedTransform = Matrix::Identity;

                            while (curr)
                            {
                                string currName = curr->mName.C_Str();
                                if (_boneNameMap.contains(currName))
                                {
                                    defaultBoneIndex = _boneNameMap[currName];
                                    break;
                                }
                                accumulatedTransform = accumulatedTransform * Convert_Matrix(curr->mTransformation);
                                curr = curr->mParent;
                            }
                            defaultOffset = accumulatedTransform;
                        }
                    }

                    // 3. 구한 가장 안전한 기본 뼈대가 현재 메쉬의 boneRefs 배열에 존재하는지 확인합니다.
                    uint32 safeRefIndex = 0;
                    bool bFoundInRefs = false;
                    for (size_t b = 0; b < boneRefs.size(); ++b)
                    {
                        if (boneRefs[b] == defaultBoneIndex)
                        {
                            safeRefIndex = static_cast<uint32>(b);
                            bFoundInRefs = true;
                            break;
                        }
                    }

                    if (!bFoundInRefs)
                    {
                        boneRefs.push_back(defaultBoneIndex);
                        offsets.push_back(defaultOffset);
                        safeRefIndex = static_cast<uint32>(boneRefs.size() - 1);
                    }
                    
                    for (auto& vertex : vertices)
                    {
                        float weightSum = vertex.blendWeight.x + vertex.blendWeight.y + vertex.blendWeight.z + vertex.blendWeight.w;

                        if (weightSum > 0.f)
                        {
                            // 정상적으로 웨이트가 있는 정점은 정규화
                            vertex.blendWeight.x /= weightSum;
                            vertex.blendWeight.y /= weightSum;
                            vertex.blendWeight.z /= weightSum;
                            vertex.blendWeight.w /= weightSum;
                        }
                        else
                        {
                            // [핵심 변경점] 웨이트가 전혀 없는 정점은 위에서 찾은 '가장 안전한 부모 뼈대'에 100% 바인딩
                            vertex.blendIndex.x = safeRefIndex;
                            vertex.blendWeight.x = 1.f;

                            // 쓰레기값 방지를 위해 나머지 요소 명시적 0 초기화
                            vertex.blendIndex.y = 0; vertex.blendWeight.y = 0.f;
                            vertex.blendIndex.z = 0; vertex.blendWeight.z = 0.f;
                            vertex.blendIndex.w = 0; vertex.blendWeight.w = 0.f;
                        }
                    }
				}

                meshData.numBoneRefs = static_cast<uint32>(boneRefs.size());
                _animVertices.push_back(move(vertices));
                _meshBoneRefs.push_back(move(boneRefs));
                _meshOffsets.push_back(move(offsets));
                _staticVertices.push_back({});
            }
            else
            {
                vector<VTXMESH> vertices(aiMesh->mNumVertices, VTXMESH{});
                for (uint32 j = 0; j < aiMesh->mNumVertices; ++j)
                {
                    vertices[j].position = Vec3{ aiMesh->mVertices[j].x, aiMesh->mVertices[j].y, aiMesh->mVertices[j].z };
                    if (aiMesh->HasNormals())
                    {
                        vertices[j].normal = Vec3{ aiMesh->mNormals[j].x, aiMesh->mNormals[j].y, aiMesh->mNormals[j].z };
                    }
                    if (aiMesh->HasTangentsAndBitangents())
                    {
                        vertices[j].tangent = Vec3{ aiMesh->mTangents[j].x, aiMesh->mTangents[j].y, aiMesh->mTangents[j].z };
                        vertices[j].binormal = Vec3{ aiMesh->mBitangents[j].x, aiMesh->mBitangents[j].y, aiMesh->mBitangents[j].z };
                    }
                    if (aiMesh->HasTextureCoords(0))
                    {
                        vertices[j].texcoord = Vec2{ aiMesh->mTextureCoords[0][j].x, aiMesh->mTextureCoords[0][j].y };
                    }
                }
                meshData.numBoneRefs = 0;
                _staticVertices.push_back(move(vertices));
                _animVertices.push_back({});
                _meshBoneRefs.push_back({});
                _meshOffsets.push_back({});
            }
            _meshHeaders.push_back(meshData);
            _indices.push_back(move(index));
        }
	}

	void ModelConverter::ReadMaterial(const aiScene* aiScene)
	{
		for (uint32 i = 0; i < aiScene->mNumMaterials; ++i)
		{
			const aiMaterial* aiMaterial = aiScene->mMaterials[i];
			ModelMaterialData materialData{};
			aiString materialName;
			aiMaterial->Get(AI_MATKEY_NAME, materialName);
			materialData.name = materialName.C_Str();

			aiColor3D aiColor{};
			if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor))
				materialData.diffuseColor = Vec4{ aiColor.r, aiColor.g, aiColor.b, 1.f };
			if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, aiColor))
				materialData.emissiveColor = Vec4{ aiColor.r, aiColor.g, aiColor.b, 1.f };

			float scalar{ 0.f };
			if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, scalar))
				materialData.roughness = scalar;
			if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_METALLIC_FACTOR, scalar))
				materialData.metalness = scalar;
			if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_SHININESS, scalar))
				materialData.shininess = scalar;
			if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_OPACITY, scalar))
				materialData.opacity = scalar;

			vector<ModelTextureSlot> slots;
			for (uint32 j = 0; j < AI_TEXTURE_TYPE_MAX; ++j)
			{
				uint32 num = aiMaterial->GetTextureCount(static_cast<aiTextureType>(j));
				for (uint32 k = 0; k < num; ++k)
				{
					aiString texPath;
					uint32 uvIndex = 0;
					ai_real blend = 1.f;
					aiTextureOp op = aiTextureOp_Multiply;
					aiTextureMapMode wm = aiTextureMapMode_Wrap;

					aiMaterial->GetTexture(static_cast<aiTextureType>(j), k, &texPath, nullptr, &uvIndex, &blend, &op, &wm);

					aiUVTransform uvTrans{};
					aiMaterial->Get(AI_MATKEY_UVTRANSFORM(static_cast<aiTextureType>(j), k), uvTrans);

					string filePath = texPath.C_Str();
					size_t lastPos = filePath.find_last_of("\\/");
					string fileName = (lastPos != string::npos) ? filePath.substr(lastPos + 1) : filePath;

					ModelTextureSlot slot{};
					slot.typeIndex = j;
					slot.textureIndex = k;
					slot.path = fileName;

					slot.uvScale = Vec2{ uvTrans.mScaling.x != 0.f ? uvTrans.mScaling.x : 1.f, uvTrans.mScaling.y != 0.f ? uvTrans.mScaling.y : 1.f };
					slot.uvOffset = Vec2{ uvTrans.mTranslation.x, uvTrans.mTranslation.y };
					slot.uvRotation = uvTrans.mRotation;
					slot.blendFactor = blend;
					slot.blendOp = static_cast<uint32>(op);
					slot.uvChannel = uvIndex;
					slot.wrapMode = static_cast<uint32>(wm);
					slots.push_back(slot);
				}
			}

			materialData.numSlots = static_cast<uint32>(slots.size());
			_matHeaders.push_back(materialData);
			_matSlots.push_back(move(slots));
		}
	}

	void ModelConverter::ReadAnimations(const aiScene* aiScene)
	{
		if (!_isSkeletal) return;

		for (uint32 i = 0; i < aiScene->mNumAnimations; ++i)
		{
			const aiAnimation* aiAnim = aiScene->mAnimations[i];
			ModelAnimationData animData{};
			animData.name = aiAnim->mName.C_Str();
			animData.duration = aiAnim->mDuration;
			animData.ticksPerSecond = aiAnim->mTicksPerSecond;
			animData.numChannels = aiAnim->mNumChannels;

			aiNodeAnim* rootChannel{ nullptr };
			int32 minParentIndex = INT32_MAX;

			for (uint32 j = 0; j < aiAnim->mNumChannels; ++j)
			{
				string channelName = aiAnim->mChannels[j]->mNodeName.C_Str();
				if (_boneNameMap.contains(channelName))
				{
					int32 boneIndex = static_cast<int32>(_boneNameMap[channelName]);
					if (_bones[boneIndex].parentBoneIndex < minParentIndex)
					{
						minParentIndex = _bones[boneIndex].parentBoneIndex;
						rootChannel = aiAnim->mChannels[j];
						if (minParentIndex == -1)
							break;
					}
				}
			}

			if (rootChannel)
			{
				aiVector3D startPosition = rootChannel->mPositionKeys[0].mValue;
				aiVector3D endPosition = rootChannel->mPositionKeys[rootChannel->mNumPositionKeys - 1].mValue;
				animData.rootTotalTranslation = Vec3{ endPosition.x - startPosition.x, endPosition.y - startPosition.y, endPosition.z - startPosition.z };

				aiQuaternion startRotation = rootChannel->mRotationKeys[0].mValue;
				aiQuaternion endRotation = rootChannel->mRotationKeys[rootChannel->mNumRotationKeys - 1].mValue;

				aiQuaternion inverseStartRotation = startRotation;
				inverseStartRotation.Conjugate();

				aiQuaternion deltaRotation = endRotation * inverseStartRotation;
				animData.rootTotalRotation = Vec4{ deltaRotation.x, deltaRotation.y, deltaRotation.z, deltaRotation.w };
			}

			vector<ModelChannelData> channels;
			vector<vector<KEYFRAME>> keyFrames;
			for (uint32 j = 0; j < aiAnim->mNumChannels; ++j)
			{
				const aiNodeAnim* animNode = aiAnim->mChannels[j];
				ModelChannelData channelData{};
				channelData.boneName = animNode->mNodeName.C_Str();

				// 본 배열 인덱스로 매핑 (노드 인덱스가 아님) 
				auto boneIt = _boneNameMap.find(channelData.boneName);
				if (boneIt == _boneNameMap.end())
					continue; 

				channelData.boneIndex = static_cast<int32>(boneIt->second);

				vector<double> keyTimes = Build_ChannelKeyTimes(animNode);
				uint32 numKeys = static_cast<uint32>(keyTimes.size());
				channelData.numKeyframes = numKeys;

				vector<KEYFRAME> frames(numKeys);
				for (uint32 k = 0; k < numKeys; ++k)
				{
					const double trackTime = keyTimes[k]; // position/rotation/scale 중 실제 존재하는 모든 key time을 보존한다.
					frames[k].trackPosition = static_cast<float>(trackTime);

					aiVector3D pos = Sample_VectorKey(animNode->mPositionKeys, animNode->mNumPositionKeys, trackTime, aiVector3D(0.f, 0.f, 0.f));
					frames[k].translation = Vec3{ pos.x, pos.y, pos.z };

					aiQuaternion rot = Sample_QuaternionKey(animNode->mRotationKeys, animNode->mNumRotationKeys, trackTime, aiQuaternion());
					frames[k].rotation = Quat{ rot.x, rot.y, rot.z, rot.w };

					aiVector3D scale = Sample_VectorKey(animNode->mScalingKeys, animNode->mNumScalingKeys, trackTime, aiVector3D(1.f, 1.f, 1.f));
					frames[k].scale = Vec3{ scale.x, scale.y, scale.z };
				}

				channels.push_back(channelData);
				keyFrames.push_back(move(frames));
			}

			/* 본이 아닌 노드 채널을 스킵했으므로 실제 수로 보정 */
			animData.numChannels = static_cast<uint32>(channels.size());

			_animHeaders.push_back(animData);
			_channelHeaders.push_back(move(channels));
			_keyframes.push_back(move(keyFrames));
		}
	}

	HRESULT ModelConverter::Export_ModelData(const aiScene* aiScene, const wstring& outputDir, const string& modelName)
	{
        wstring filePath = outputDir + Engine::String::ToWString(modelName) + L".model";
        ofstream out(filePath, ios::binary);
        if (!out.is_open()) return E_FAIL;

        auto WriteString = [](ofstream& out, const string& str) {
            uint32 len = static_cast<uint32>(str.size());
            out.write((char*)&len, sizeof(uint32));
            if (len > 0)
                out.write(str.c_str(), len);
        };

        uint32 texSize = AI_TEXTURE_TYPE_MAX;
        uint32 numMaterials = static_cast<uint32>(_matHeaders.size());
        uint32 numMeshes = static_cast<uint32>(_meshHeaders.size());
        uint32 numBones = static_cast<uint32>(_bones.size());

        out.write(BYTE(&_isSkeletal), sizeof(bool));
        out.write(BYTE(&texSize), sizeof(uint32));
        out.write(BYTE(&numMaterials), sizeof(uint32));
        out.write(BYTE(&numMeshes), sizeof(uint32));
        out.write(BYTE(&numBones), sizeof(uint32));

        for (uint32 i = 0; i < numMaterials; ++i)
        {
            WriteString(out, _matHeaders[i].name);
            out.write(BYTE(&_matHeaders[i].diffuseColor), sizeof(Vec4) * 4);
            out.write(BYTE(&_matHeaders[i].shininess), sizeof(float) * 4);
            out.write(BYTE(&_matHeaders[i].numSlots), sizeof(uint32));

            for (uint32 j = 0; j < _matHeaders[i].numSlots; ++j)
            {
                WriteString(out, _matSlots[i][j].path);
                out.write(BYTE(&_matSlots[i][j].typeIndex), sizeof(uint32) * 2);
                out.write(BYTE(&_matSlots[i][j].uvScale), sizeof(Vec2) * 2);
                out.write(BYTE(&_matSlots[i][j].uvRotation), sizeof(float) * 2);
                out.write(BYTE(&_matSlots[i][j].blendOp), sizeof(uint32) * 3);
            }
        }
        for (uint32 i = 0; i < numMeshes; ++i)
        {
            WriteString(out, _meshHeaders[i].name);
            out.write(BYTE(&_meshHeaders[i].materialIndex), sizeof(uint32) * 4);

            if (_isSkeletal)
            {
                out.write(BYTE(_animVertices[i].data()), _meshHeaders[i].numVertices * sizeof(VTXANIMMESH));
                out.write(BYTE(_indices[i].data()), _meshHeaders[i].numIndices * sizeof(uint32));

                if (_meshHeaders[i].numBoneRefs > 0)
                {
                    out.write(BYTE(_meshBoneRefs[i].data()), _meshHeaders[i].numBoneRefs * sizeof(uint32));
                    out.write(BYTE(_meshOffsets[i].data()), _meshHeaders[i].numBoneRefs * sizeof(Matrix));
                }
            }
            else
            {
                out.write(BYTE(_staticVertices[i].data()), _meshHeaders[i].numVertices * sizeof(VTXMESH));
                out.write(BYTE(_indices[i].data()), _meshHeaders[i].numIndices * sizeof(uint32));
            }
        }
        if (numBones > 0)
        {
            for (uint32 i = 0; i < numBones; ++i)
            {
                WriteString(out, _bones[i].name);
                out.write(BYTE(&_bones[i].parentBoneIndex), sizeof(int32));
                out.write(BYTE(&_bones[i].localTransform), sizeof(Matrix));
                out.write(BYTE(&_bones[i].offsetMatrix), sizeof(Matrix));
            }
        }
        out.close();
        return S_OK;
	}

	HRESULT ModelConverter::Export_AnimationData(const aiScene* aiScene, const wstring& outputDir, const string& modelName)
	{
		if (!_isSkeletal || _animHeaders.empty()) return S_OK;

		wstring animDir = outputDir + L"Animations/";
		Ensure_DirectoryExists(animDir);

        auto WriteString = [](ofstream& out, const string& str) {
            uint32 len = static_cast<uint32>(str.size());
            out.write((char*)&len, sizeof(uint32));
            if (len > 0)
                out.write(str.c_str(), len);
        };

        string cleanModelName = Sanitize_FileName(modelName);

		for (uint32 i = 0; i < _animHeaders.size(); ++i)
		{
            string finalFileName = cleanModelName;

            // 내부 애니메이션 이름 추출 및 정제 (예: "Armature|Dash_Out" -> "Dash_Out")
            string internalAnimName = _animHeaders[i].name;
            size_t pipePos = internalAnimName.find_last_of('|');
            if (pipePos != string::npos) {
                internalAnimName = internalAnimName.substr(pipePos + 1);
            }
            
            string sanitizedInternal = Sanitize_FileName(internalAnimName);

            // 애니메이션이 여러 개일 경우에 내부 이름 결합
            if (_animHeaders.size() > 1)
            {
                if (sanitizedInternal != "DefaultAnimName" && !sanitizedInternal.empty())
                {
                    // 모델 이름과 내부 이름이 중복되지 않게 처리 (예: Lancelier_Dash_Out 안에 Lancelier 가 이미 있는 경우 대비)
                    if (sanitizedInternal.find(cleanModelName) == string::npos && cleanModelName.find(sanitizedInternal) == string::npos)
                    {
                        finalFileName += "_" + sanitizedInternal;
                    }
                    else
                    {
                        finalFileName = sanitizedInternal;
                    }
                }
                else
                {
                    finalFileName += "_" + to_string(i);
                }
            }

            if (_exportReversedAnimation)
            {
                finalFileName += "_Reversed";
            }
            wstring animPath = animDir + ToWString(finalFileName) + L".anim";
            
            ofstream out(animPath, ios::binary);
            if (!out.is_open())
            {
                LOG_ERROR("Failed to create anim file: {}", finalFileName);
                continue;
            }

            // 파일 내부에 저장되는 이름 갱신 (ModelCom 에서 이름으로 찾을 수 있게)
            _animHeaders[i].name = finalFileName;

			WriteString(out, finalFileName);
			out.write(BYTE(&_animHeaders[i].duration), sizeof(double) * 2);
			out.write(BYTE(&_animHeaders[i].numChannels), sizeof(uint32));

            Vec3 rootTranslation = _animHeaders[i].rootTotalTranslation;
            Vec4 rootRotation = _animHeaders[i].rootTotalRotation;

            if (_exportReversedAnimation)
            {
                rootTranslation = Vec3{ -rootTranslation.x, -rootTranslation.y, -rootTranslation.z };
                rootRotation = Vec4{ -rootRotation.x, -rootRotation.y, -rootRotation.z, rootRotation.w };
            }

			out.write(BYTE(&rootTranslation), sizeof(Vec3));
			out.write(BYTE(&rootRotation), sizeof(Vec4));

			uint32 numChannels = _animHeaders[i].numChannels;
			for (uint32 j = 0; j < numChannels; ++j)
			{
				WriteString(out, _channelHeaders[i][j].boneName);
				out.write(BYTE(&_channelHeaders[i][j].boneIndex), sizeof(int32));
				out.write(BYTE(&_channelHeaders[i][j].numKeyframes), sizeof(uint32));
				
                if (_exportReversedAnimation) // 채널 역전 구성
                {
                    vector<KEYFRAME> reverseKeys = _keyframes[i][j];
                    float duration = static_cast<float>(_animHeaders[i].duration);

                    if (reverseKeys.size() == 1)
                    {
                        reverseKeys[0].trackPosition = 0.f;
                    }
                    else
                    {
                        ranges::reverse(reverseKeys);

                        for (auto& key : reverseKeys)
                        {
                            key.trackPosition = duration - key.trackPosition;
                        }

                        ranges::sort(reverseKeys, {}, &KEYFRAME::trackPosition);
                    }

                    out.write(BYTE(reverseKeys.data()), _channelHeaders[i][j].numKeyframes * sizeof(KEYFRAME));
                }
                else
                {
                    out.write(BYTE(_keyframes[i][j].data()), _channelHeaders[i][j].numKeyframes * sizeof(KEYFRAME));
                }
			}

			out.close();
			LOG_INFO("Export Anim Clip: {}", finalFileName);
		}

		return S_OK;
	}

	HRESULT ModelConverter::Export_JsonLog(const wstring& outputDir)
	{
		wstring logDir = outputDir + L"Log_json/";
		Ensure_DirectoryExists(logDir);

		auto Vec3J = [](const Vec3& v) { return json{ {"x", v.x}, {"y", v.y}, {"z", v.z} }; };
		auto Vec4J = [](const Vec4& v) { return json{ {"x", v.x}, {"y", v.y}, {"z", v.z}, {"w", v.w} }; };
		auto MatJ  = [](const Matrix& m) {
			json a = json::array();
			const float* f = reinterpret_cast<const float*>(&m);
			for (int i = 0; i < 16; ++i) a.push_back(f[i]);
			return a;
		};

		{
			json jStats = json::array();
			size_t totalBytes = 0;
			uint32 vertexStride = _isSkeletal ? sizeof(VTXANIMMESH) : sizeof(VTXMESH);

			for (size_t i = 0; i < _meshHeaders.size(); ++i)
			{
				uint32 vtxCount = _meshHeaders[i].numVertices;
				uint32 idxCount = _meshHeaders[i].numIndices;
				uint32 vtxBytes = vtxCount * vertexStride;
				uint32 idxBytes = idxCount * sizeof(uint32);
				uint32 meshTotal = vtxBytes + idxBytes;

				jStats.push_back({
					{"meshIndex", i},
					{"name", _meshHeaders[i].name},
					{"materialIndex", _meshHeaders[i].materialIndex},
					{"numVertices", vtxCount},
					{"numIndices", idxCount},
					{"vertexStride", vertexStride},
					{"vertexDataSize", vtxBytes},
					{"indexDataSize", idxBytes},
					{"totalSizeBytes", meshTotal}
					});
				totalBytes += meshTotal;
			}

			json jSummary = {
				{"isSkeletal", _isSkeletal},
				{"totalMeshCount", static_cast<uint32>(_meshHeaders.size())},
				{"totalBinarySize", totalBytes},
				{"meshes", jStats}
			};

			ofstream(logDir + L"MeshStats.json") << jSummary.dump(4);
		}

		{
			json jArr = json::array();
			for (const auto& b : _bones)
				jArr.push_back({ {"name", b.name}, {"parentIndex", b.parentBoneIndex}, {"offsetMatrix", MatJ(b.offsetMatrix)} });
			ofstream(logDir + L"Bone.json") << jArr.dump(4);
		}

		{
			json jArr = json::array();
			for (size_t i = 0; i < _matHeaders.size(); ++i)
			{
				json jSlots = json::array();
				for (const auto& s : _matSlots[i])
					jSlots.push_back({ {"typeIndex", s.typeIndex}, {"textureIndex", s.textureIndex}, {"path", s.path}, {"uvScale", {s.uvScale.x, s.uvScale.y}}, {"uvOffset", {s.uvOffset.x, s.uvOffset.y}}, {"uvRotation", s.uvRotation}, {"blendFactor", s.blendFactor}, {"blendOp", s.blendOp}, {"uvChannel", s.uvChannel}, {"wrapMode", s.wrapMode} });
				
				const auto& h = _matHeaders[i];
				jArr.push_back({ {"name", h.name}, {"diffuse", Vec4J(h.diffuseColor)}, {"ambient", Vec4J(h.ambientColor)}, {"specular", Vec4J(h.specularColor)}, {"emissive", Vec4J(h.emissiveColor)}, {"shininess", h.shininess}, {"opacity", h.opacity}, {"roughness", h.roughness}, {"metalness", h.metalness}, {"numSlots", h.numSlots}, {"slots", jSlots} });
			}
			ofstream(logDir + L"Material.json") << jArr.dump(4);
		}

		if (_isSkeletal)
		{
			json jAnims = json::array();
			json jChans = json::array();
			for (size_t i = 0; i < _animHeaders.size(); ++i)
			{
				const auto& ah = _animHeaders[i];
				jAnims.push_back({ {"name", ah.name}, {"duration", ah.duration}, {"ticksPerSecond", ah.ticksPerSecond}, {"numChannels", ah.numChannels}, {"rootTotalTranslation", Vec3J(ah.rootTotalTranslation)}, {"rootTotalRotation", Vec4J(ah.rootTotalRotation)} });

				json jAnimChans = json::array();
				for (size_t j = 0; j < _channelHeaders[i].size(); ++j)
				{
					const auto& ch = _channelHeaders[i][j];
					json jKeys = json::array();
					for (const auto& k : _keyframes[i][j])
						jKeys.push_back({ {"trackPosition", k.trackPosition}, {"translation", Vec3J(k.translation)}, {"rotation", Vec4J(k.rotation)}, {"scale", Vec3J(k.scale)} });
					
					jAnimChans.push_back({ {"boneName", ch.boneName}, {"boneIndex", ch.boneIndex}, {"numKeyframes", ch.numKeyframes}, {"keyframes", jKeys} });
				}
				jChans.push_back({ {"animationName", ah.name}, {"channels", jAnimChans} });
			}
			ofstream(logDir + L"Animation.json") << jAnims.dump(4);
			ofstream(logDir + L"channel.json") << jChans.dump(4);
		}

		LOG_INFO("Log: JSON data exported to Log_json/");
		return S_OK;
	}

	Matrix ModelConverter::Convert_Matrix(const aiMatrix4x4& aiMat) const
	{
        return Matrix(
            aiMat.a1, aiMat.b1, aiMat.c1, aiMat.d1,
            aiMat.a2, aiMat.b2, aiMat.c2, aiMat.d2,
            aiMat.a3, aiMat.b3, aiMat.c3, aiMat.d3,
            aiMat.a4, aiMat.b4, aiMat.c4, aiMat.d4
        );
	}

	void ModelConverter::Clear_Converter()
	{
		_importer.FreeScene();
		_aiScene = nullptr;

		_meshHeaders.clear(); _meshHeaders.shrink_to_fit();
		_staticVertices.clear(); _staticVertices.shrink_to_fit();
		_animVertices.clear(); _animVertices.shrink_to_fit();
		_indices.clear(); _indices.shrink_to_fit();
		_meshBoneRefs.clear(); _meshBoneRefs.shrink_to_fit();
		_meshOffsets.clear(); _meshOffsets.shrink_to_fit();
		_matHeaders.clear(); _matHeaders.shrink_to_fit();
		_matSlots.clear(); _matSlots.shrink_to_fit();
		_bones.clear(); _bones.shrink_to_fit();
		_boneNameMap.clear(); 
		_animHeaders.clear(); _animHeaders.shrink_to_fit();
		_channelHeaders.clear(); _channelHeaders.shrink_to_fit();
		_keyframes.clear(); _keyframes.shrink_to_fit();
	}

	string ModelConverter::Get_FileNameWithoutExtension(const wstring& filePath) const
	{
		fs::path path(filePath);
		return path.stem().string();
	}

	void ModelConverter::Ensure_DirectoryExists(const wstring& dirPath) const
	{
		if (!fs::exists(dirPath))
		{
			fs::create_directories(dirPath);
		}
	}

	Shared<ModelConverter> ModelConverter::Create(HWND hWnd)
	{
		auto prototype = make_shared<ModelConverter>();
		prototype->_hWnd = hWnd;

		if (FAILED(prototype->Initialize()))
		{
			MSG_BOX("Failed to Created : ModelConverter");
			return nullptr;
		}

		return prototype;
	}

}

