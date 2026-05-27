#pragma once

// Debug build에서 DX11 debug layer를 켤지 결정한다.
// D3D11 Live Object ref Count Checker 작동 여부에 영향을 미친다.
#ifndef ENGINE_ENABLE_DX11_DEBUG_LAYER
#define ENGINE_ENABLE_DX11_DEBUG_LAYER 0
#endif

// 프로파일 창의 강도를 설정
#define ENGINE_PROFILE_LEVEL_OFF 0
#define ENGINE_PROFILE_LEVEL_BASIC 1
#define ENGINE_PROFILE_LEVEL_DETAIL 2

// Debug는 큰 단위 profile을 기본으로 켜고, Release는 성능 오염을 피하기 위해 기본으로 끈다.
#ifndef ENGINE_PROFILE_LEVEL
#ifdef _DEBUG
#define ENGINE_PROFILE_LEVEL ENGINE_PROFILE_LEVEL_BASIC
#else
#define ENGINE_PROFILE_LEVEL ENGINE_PROFILE_LEVEL_OFF
#endif
#endif
