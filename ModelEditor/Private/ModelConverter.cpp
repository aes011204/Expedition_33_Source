#include "pch.h"
#include <shellapi.h>
#include <commctrl.h>
#include "ModelConverter.h"

namespace ModelEditor
{
	LRESULT CALLBACK ModelConverter::DragDropWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
	{
		ModelConverter* thisPointer = reinterpret_cast<ModelConverter*>(dwRefData);

		switch (message)
		{
		case WM_DROPFILES:
		{
			::HDROP hDrop = reinterpret_cast<::HDROP>(wParam);
			uint32 fileCount = ::DragQueryFileW(hDrop, 0xffffffff, nullptr, 0);

			for (uint32 i = 0; i < fileCount; ++i)
			{
				wchar_t filePath[MAX_PATH];
				::DragQueryFileW(hDrop, i, filePath, MAX_PATH);

				if (fs::path(filePath).extension() == ".fbx")
				{
					LOG_INFO("Drag & Drop Detected: {}", fs::path(filePath).filename().string());
					thisPointer->Convert_Model(filePath, fs::path(filePath).parent_path().wstring() + L"/");
				}
			}
			::DragFinish(hDrop);
			return 0;
		}
		case WM_NCDESTROY:
			::RemoveWindowSubclass(hWnd, DragDropWndProc, uIdSubclass);
			break;
		}
		return ::DefSubclassProc(hWnd, message, wParam, lParam);
	}

	string ModelConverter::Sanitize_FileName(string name)
	{
        std::string result = name;

        size_t pos = result.find("UE5");
        if (pos != std::string::npos)
        {
            size_t cutPos = pos + 3; 

            while (cutPos < result.length() && (result[cutPos] == '_' || result[cutPos] == ' ' ||
                result[cutPos] == '|'))
            {
                cutPos++;
            }

            result = result.substr(cutPos);
        }

        string forbidden = "\\/:*?\"<>|";
        for (char& c : result)
        {
            if (forbidden.find(c) != string::npos)
            {
                c = '_';
            }
        }

        if (result.empty())
        {
            result = "DefaultAnimName";
        }

        return result;
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
		importFlags = aiProcess_ConvertToLeftHanded |
					  aiProcess_Triangulate |
					  aiProcess_CalcTangentSpace |
					  aiProcess_GenSmoothNormals |
                      aiProcess_LimitBoneWeights;

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
		_aiScene = _importer.ReadFile(utf8FilePath, importFlags);

		if (nullptr == _aiScene || _aiScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !_aiScene->mRootNode)
		{
			LOG_ERROR("Assimp Error: {} \n", _importer.GetErrorString());
			return E_FAIL;
		}

		string modelName = Get_FileNameWithoutExtension(fbxFilePath);
        bool hasMeshBones = false;
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


		ReadMeshes(_aiScene);
		ReadMaterial(_aiScene);
		ReadAnimations(_aiScene);
		Ensure_DirectoryExists(outputDir);

		if (FAILED(Export_ModelData(_aiScene, outputDir, modelName)))
		{
			return E_FAIL;
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
                        if (vertex.blendWeight.x == 0.f) {
                            vertex.blendIndex.x = j;
                            vertex.blendWeight.x = weight;
                        }
                        else if (vertex.blendWeight.y == 0.f) {
                            vertex.blendIndex.y = j;
                            vertex.blendWeight.y = weight;
                        }
                        else if (vertex.blendWeight.z == 0.f) {
                            vertex.blendIndex.z = j;
                            vertex.blendWeight.z = weight;
                        }
                        else if (vertex.blendWeight.w == 0.f) {
                            vertex.blendIndex.w = j;
                            vertex.blendWeight.w = weight;
                        }
                    }
                }

                for (auto& vertex : vertices) {
                    float weightSum = vertex.blendWeight.x + vertex.blendWeight.y + vertex.blendWeight.z + vertex.blendWeight.w;

                    if (weightSum > 0.f)
                    {
                        vertex.blendWeight.x /= weightSum;
                        vertex.blendWeight.y /= weightSum;
                        vertex.blendWeight.z /= weightSum;
                        vertex.blendWeight.w /= weightSum;
                    }
                    else
                    {
                        vertex.blendIndex.x = 0;
                        vertex.blendWeight.x = 1.f;
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

				uint32 numKeys = max({ animNode->mNumPositionKeys, animNode->mNumRotationKeys, animNode->mNumScalingKeys });
				channelData.numKeyframes = numKeys;

				vector<KEYFRAME> frames(numKeys);
				for (uint32 k = 0; k < numKeys; ++k)
				{
					uint32 posIdx = min(k, animNode->mNumPositionKeys - 1);
					uint32 rotIdx = min(k, animNode->mNumRotationKeys - 1);
					uint32 scaleIdx = min(k, animNode->mNumScalingKeys - 1);

					frames[k].trackPosition = static_cast<float>(animNode->mPositionKeys[posIdx].mTime);

					aiVector3D pos = animNode->mPositionKeys[posIdx].mValue;
					frames[k].translation = Vec3{ pos.x, pos.y, pos.z };

					aiQuaternion rot = animNode->mRotationKeys[rotIdx].mValue;
					frames[k].rotation = Quat{ rot.x, rot.y, rot.z, rot.w };

					aiVector3D scale = animNode->mScalingKeys[scaleIdx].mValue;
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

		for (uint32 i = 0; i < _animHeaders.size(); ++i)
		{
			string animClipName = Sanitize_FileName(string(_animHeaders[i].name));
			string fullFileName = modelName + "_" + animClipName;

			wstring animPath = animDir + ToWString(fullFileName) + L".anim";
			ofstream out(animPath, ios::binary);
			if (!out.is_open())
			{
				LOG_ERROR("Failed to create anim file: {}", fullFileName);
				continue;
			}

			WriteString(out, _animHeaders[i].name);
			out.write(BYTE(&_animHeaders[i].duration), sizeof(double) * 2);
			out.write(BYTE(&_animHeaders[i].numChannels), sizeof(uint32));
			out.write(BYTE(&_animHeaders[i].rootTotalTranslation), sizeof(Vec3));
			out.write(BYTE(&_animHeaders[i].rootTotalRotation), sizeof(Vec4));

			uint32 numChannels = _animHeaders[i].numChannels;
			for (uint32 j = 0; j < numChannels; ++j)
			{
				WriteString(out, _channelHeaders[i][j].boneName);
				out.write(BYTE(&_channelHeaders[i][j].boneIndex), sizeof(int32));
				out.write(BYTE(&_channelHeaders[i][j].numKeyframes), sizeof(uint32));
				
				out.write(BYTE(_keyframes[i][j].data()), _channelHeaders[i][j].numKeyframes * sizeof(KEYFRAME));
			}

			out.close();
			LOG_INFO("Export Anim Clip: {}", fullFileName);
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
		Matrix mat;
		mat._11 = aiMat.a1; mat._12 = aiMat.b1; mat._13 = aiMat.c1; mat._14 = aiMat.d1;
		mat._21 = aiMat.a2; mat._22 = aiMat.b2; mat._23 = aiMat.c2; mat._24 = aiMat.d2;
		mat._31 = aiMat.a3; mat._32 = aiMat.b3; mat._33 = aiMat.c3; mat._34 = aiMat.d3;
		mat._41 = aiMat.a4; mat._42 = aiMat.b4; mat._43 = aiMat.c4; mat._44 = aiMat.d4;

		return mat;
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

