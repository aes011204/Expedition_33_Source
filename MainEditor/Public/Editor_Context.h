#pragma once
#include "Base.h"

NS_BEGIN(Engine)
class GameObject;
NS_END

NS_BEGIN(MainEditor)

// 현재 선택 오브젝트를 모든 창이 공유할 수 있게
class Editor_Context : public Base
{
public:
    Editor_Context() = default;
    virtual ~Editor_Context() = default;

public:
    const EditorSelection& Get_Selection() const { return _selection; }

    void Set_SelectObject(const Shared<GameObject>& gameObject);
    void Clear_Selection();
    void Request_FocusObject(const Shared<GameObject>& gameObject);
    Shared<GameObject> Consume_FocusObjectRequest();

    uint32 Get_SelectionRevision() const { return _selectionRevision; }


private:
    EditorSelection     _selection;
    EditorFocusRequest  _focusRequest;
    uint32              _selectionRevision = 0;

public:
    static Unique<Editor_Context> Create();
    void Free() override;

};

NS_END
