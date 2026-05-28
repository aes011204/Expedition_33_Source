#include "pch.h"
#include "ScreenFx_View.h"
#include "GameInstance.h"
#include "ScreenFx_Manager.h"
#include "ScreenFx_Instance.h"

NS_BEGIN(MainEditor)

ScreenFx_View::ScreenFx_View() : Editor_Window(L"ScreenFx Viewer")
{
}

HRESULT ScreenFx_View::Initialize()
{
    return S_OK;
}

void ScreenFx_View::Update(float timeDelta)
{
    if (_previewInstance && _isPlaying)
    {
        auto delta = ImGui::GetIO().DeltaTime * _playbackSpeed;
        _previewInstance->Set_ElapsedTime(_previewInstance->Get_ElapsedTime() + delta);
        
        if (_previewInstance->Get_ElapsedTime() >= _previewInstance->Get_Duration())
        {
            if (_autoLoop)
                _previewInstance->Set_ElapsedTime(0.f);
            else
            {
                _previewInstance->Set_ElapsedTime(_previewInstance->Get_Duration());
                _isPlaying = false;
            }
        }
        _previewInstance->Evaluate_At(_previewInstance->Get_ElapsedTime());
    }
}

void ScreenFx_View::Pre_Render()
{
}

void ScreenFx_View::Render()
{
    if (!_isOpen) return;
    if (!ImGui::Begin("ScreenFx Editor", &_isOpen)) { ImGui::End(); return; }

    const char* currentSelection = _selectedEffectName.empty() ? "Select Effect..." : _selectedEffectName.c_str();

    // 1. 이펙트 타입 콤보박스 (TypeRegistry에서 등록된 목록 가져오기)
    std::vector<std::string> effectNames;
    GAME->Visit_ScreenFxTypes([&](const Engine::TypeRegistry::ScreenFxTypeInfo& info) {
        effectNames.push_back(info.className);
    });

    if (ImGui::BeginCombo("Effect Type", currentSelection))
    {
        for (const auto& name : effectNames)
        {
            bool isSelected = (_selectedEffectName == name);
            if (ImGui::Selectable(name.c_str(), isSelected))
            {
                _selectedEffectName = name;
                
                // GameInstance를 통해 TypeRegistry에서 생성
                _previewInstance = GAME->Create_ScreenFxInstance(name);

                if (_previewInstance)
                {
                    _previewInstance->Initialize(nullptr);
                    GAME->Get_ScreenFxManager()->Set_PreviewEffect(_previewInstance);
                }
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        _selectedEffectName = "";
        _previewInstance = nullptr;
        GAME->Get_ScreenFxManager()->Clear_PreviewEffect();
    }

    // 2. 리플렉션 렌더링 (프로퍼티 에디터)
    if (_previewInstance)
    {
        ImGui::Separator();
        ImGui::Text("Properties");

        auto& info = _previewInstance->Get_ReflectionInfo();
        char* baseAddress = reinterpret_cast<char*>(_previewInstance.get());
        bool propertyChanged = false;

        for (auto& prop : info.properties)
        {
            void* valuePtr = baseAddress + prop.offset;
            ImGui::PushID(prop.name.c_str());

            int propType = static_cast<int>(prop.type);
            
            switch (propType)
            {
            case static_cast<int>(Engine::PropertyType::Float):
                if (ImGui::DragFloat(prop.displayName.c_str(), reinterpret_cast<float*>(valuePtr), prop.dragSpeed, prop.minVal, prop.maxVal))
                    propertyChanged = true;
                break;
            case static_cast<int>(Engine::PropertyType::Int):
                if (ImGui::DragInt(prop.displayName.c_str(), reinterpret_cast<int*>(valuePtr), prop.dragSpeed, static_cast<int>(prop.minVal), static_cast<int>(prop.maxVal)))
                    propertyChanged = true;
                break;
            case static_cast<int>(Engine::PropertyType::Vec2):
                if (ImGui::DragFloat2(prop.displayName.c_str(), reinterpret_cast<float*>(valuePtr), prop.dragSpeed, prop.minVal, prop.maxVal))
                    propertyChanged = true;
                break;
            case static_cast<int>(Engine::PropertyType::Vec3):
                if (ImGui::DragFloat3(prop.displayName.c_str(), reinterpret_cast<float*>(valuePtr), prop.dragSpeed, prop.minVal, prop.maxVal))
                    propertyChanged = true;
                break;
            case static_cast<int>(Engine::PropertyType::Vec4):
                if (ImGui::DragFloat4(prop.displayName.c_str(), reinterpret_cast<float*>(valuePtr), prop.dragSpeed, prop.minVal, prop.maxVal))
                    propertyChanged = true;
                break;
            case static_cast<int>(Engine::PropertyType::Color):
                if (ImGui::ColorEdit4(prop.displayName.c_str(), reinterpret_cast<float*>(valuePtr)))
                    propertyChanged = true;
                break;
            }
            ImGui::PopID();
        }

        // 프로퍼티가 수정되면 즉시 Evaluate_At 호출하여 갱신
        if (propertyChanged)
        {
            _previewInstance->Evaluate_At(_previewInstance->Get_ElapsedTime());
        }

        // 3. 타임라인
        ImGui::Separator();
        ImGui::Text("Timeline");

        float elapsedTime = _previewInstance->Get_ElapsedTime();
        if (ImGui::SliderFloat("Elapsed", &elapsedTime, 0.f, _previewInstance->Get_Duration(), "%.3f s"))
        {
            _previewInstance->Set_ElapsedTime(elapsedTime);
            _previewInstance->Evaluate_At(_previewInstance->Get_ElapsedTime());
        }

        if (ImGui::Button(_isPlaying ? "Pause" : "Play"))
        {
            _isPlaying = !_isPlaying;
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
        {
            _isPlaying = false;
            _previewInstance->Set_ElapsedTime(0.f);
            _previewInstance->Evaluate_At(0.f);
        }
        
        ImGui::SameLine();
        ImGui::Checkbox("Loop", &_autoLoop);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.f);
        ImGui::SliderFloat("Speed", &_playbackSpeed, 0.1f, 3.0f, "%.1fx");
    }

    ImGui::End();
}

Shared<ScreenFx_View> ScreenFx_View::Create()
{
    return make_shared<ScreenFx_View>();
}

NS_END
