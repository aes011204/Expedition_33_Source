#pragma once
#include "Editor_Window.h"



NS_BEGIN(MainEditor)

class AnimMapperAdapter final : Editor_Window
{
public:
    AnimMapperAdapter();
    ~AnimMapperAdapter() override = default;

public:
    void Initialize(const std::wstring& targetModelPath);
    void Draw_Mapper();

private:
    void Draw_StateList();
    void Draw_AnimList();
    void Draw_SaveLoad();

    void Save_Mapping(const std::string& fileName);
    void Load_Mapping(const std::string& fileName);
    void Scan_Animations();

private:
    std::wstring _targetModelPath;
    std::wstring _targetModelDir;

    std::map<string, std::string> _animationMapping;
    std::vector<std::string> _availableAnimations;

    // 검색용 필터 및 저장/로드 버퍼
    char _saveLoadBuffer[MAX_PATH] = "";
};

NS_END
