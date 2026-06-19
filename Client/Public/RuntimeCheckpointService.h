#pragma once

#include "RuntimeCheckpointIO.h"

NS_BEGIN(Engine)
class GameObject;
NS_END

NS_BEGIN(Client)

class RuntimeCheckpointService final
{
public:
    RuntimeCheckpointService() = delete;

    static bool Capture_Current(RuntimeCheckpointIO::Checkpoint& outCheckpoint);
    static bool Apply_ToCurrentLevel(const RuntimeCheckpointIO::Checkpoint& checkpoint);

private:
    static bool Should_CaptureWorldObject(const wstring& layerTag, const Shared<GameObject>& gameObject);
    static void Sync_RestoredMovement(const Shared<GameObject>& gameObject);
};

NS_END
