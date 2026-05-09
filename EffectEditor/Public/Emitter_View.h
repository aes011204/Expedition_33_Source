#pragma once

#include "Editor_Window.h"
#include "EffectAuthoring_Types.h"

NS_BEGIN(EffectEditor)

class Emitter_View final : public Editor_Window
{
public:
    Emitter_View();
    ~Emitter_View() override = default;

public:
    HRESULT Initialize() override;
    void Render() override;

public: //## Accessor::Authoring
    AuthoringEmitter* Find_Emitter(uint32 emitterId);
    AuthoringModule* Find_Module(uint32 emitterId, uint32 moduleId);

private:
    // 선택 emitter 공통 강조색.
    static constexpr float kEmitterAccentOrange[4]{ 1.0f, 0.42f, 0.10f, 1.0f };
    static constexpr float kEmitterColumnWidth{ 270.f };
    static constexpr float kEmitterHeaderHeight{ 90.f };
    static constexpr float kEmitterTypeDataReserveHeight{ 22.f };
    static constexpr float kEmitterDividerWidth{ 1.f };
    static constexpr float kEmitterColumnBackgroundColor[4]{ 0.06f, 0.065f, 0.08f, 1.0f };
    static constexpr float kEmitterTypeDataReserveColor[4]{ 0.035f, 0.040f, 0.055f, 1.0f };
    static constexpr float kEmitterDividerColor[4]{ 0.015f, 0.018f, 0.025f, 1.0f };

private: //## Data
    // 현재 편집 중인 effect의 emitter 목록이다.
    vector<AuthoringEmitter> _emitters{};
    // 현재 선택된 emitter index다. 값이 없으면 emitter 선택이 해제된 상태다.
    optional<size_t> _selectedEmitterIndex{};
    // 현재 선택된 emitter 내부의 module index다. 값이 없으면 emitter header만 선택된 상태다.
    optional<size_t> _selectedModuleIndex{};
    // 저장용 id가 아니라 editor 세션 안에서 emitter/module을 구분하기 위한 임시 id 발급값이다.
    uint32 _nextAuthoringId{ 1 };

private: //## Helpers
    // 새 emitter/module에 부여할 editor-local id를 발급한다.
    uint32 Issue_AuthoringId();
    // Cascade의 기본 더미 emitter 하나를 만든다.
    AuthoringEmitter Make_DefaultSpriteEmitter();
    AuthoringModule Make_RequiredModule();
    AuthoringModule Make_SpawnModule();
    AuthoringModule Make_LifetimeModule();
    AuthoringModule Make_InitialSizeModule();
    AuthoringModule Make_InitialVelocityModule();
    AuthoringModule Make_ColorOverLifeModule();
    // 현재 선택 index가 emitter 목록 범위를 벗어나지 않도록 보정한다.
    // TODO : 삭제/이동 단계에서 호출 예정
    void Clamp_Selection();

    void Draw_EmitterBoard();
    void Draw_EmitterColumn(size_t emitterIndex, const AuthoringEmitter& emitter, float columnHeight);
    void Draw_ModuleRow(size_t emitterIndex, size_t moduleIndex, const AuthoringModule& module);

public:
    static Shared<Emitter_View> Create();
};

NS_END
