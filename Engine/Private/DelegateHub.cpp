#include "DelegateHub.h"

void DelegateHub::Free()
{
    OnEditorStopRuntime.Clear();
    OnEditorPlayRuntime.Clear();
    OnEditorPauseRuntime.Clear();
    OnEditorResumeRuntime.Clear();

    Base::Free();
}
