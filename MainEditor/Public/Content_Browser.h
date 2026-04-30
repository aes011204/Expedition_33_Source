#pragma once

#include "EditorWindow.h"

NS_BEGIN(MainEditor)

class Content_Browser final : public EditorWindow
{
public:
    /* 클라이언트 리소스 폴더를 루트로 사용하는 컨텐츠 브라우저를 생성한다. */
    Content_Browser();
    virtual ~Content_Browser() = default;

public:
    /* 가벼운 리소스 디렉터리 브라우저를 그린다. */
    virtual void Render() override;

private:
    /* 무거운 에셋 파이프라인 없이 한 단계 디렉터리 목록만 그린다. */
    void Render_Directory(const filesystem::path& rootPath);

private:
    filesystem::path _resourceRoot = {};             /* 브라우저가 표시할 리소스 루트 경로. */
};

NS_END
