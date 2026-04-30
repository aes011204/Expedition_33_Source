#ifndef Engine_Struct_h__
#define Engine_Struct_h__

#include "Engine_Typedef.h"

namespace Engine
{
	typedef struct tagEngineDesc
	{
		HINSTANCE	 hInstance;
		HWND		 hWnd;
		WinMode		 eWinMode;
		unsigned int iNumLevels;
		unsigned int iViewportWidth;
		unsigned int iViewportHeight;			
	}ENGINE_DESC;

	typedef struct tagKeyFrame
	{
		Vec3			vScale;
		Quat			vRotation;
		Vec3			vTranslation;
		
		float		    fTrackPosition; /* 현재 키프레임상태는 어떤 위치에서 표현되어야하는가? */

	}KEYFRAME;

	typedef struct tagLightDesc
	{
		LightType		eType;

		Vec4		vDirection;
		Vec4		vPosition;
		float		fRange;

		Vec4		vDiffuse;
		Vec4		vAmbient;
		Vec4		vSpecular;

	}LIGHT_DESC;

	typedef struct tagAssetMeta
	{
		string		strGuid = {};			/* 에셋을 lazy-load/key lookup으로 찾기 위한 고유 식별자 */
		string		strType = {};			/* ResourceLoader가 어떤 로더를 태울지 판단하는 에셋 타입 */
		string		strModelType = {};		/* meshbin처럼 같은 확장자 안에서 static/anim 등을 구분하기 위한 보조 타입 */
		wstring		strFullPath = {};		/* 실제 파일 시스템에서 접근할 절대 경로 */
		wstring		strRelativePath = {};	/* 리소스 루트 기준 상대 경로로, json/테이블에서 사람이 확인하기 쉬운 경로 */
	}FAssetMeta;


}


#endif // Engine_Struct_h__
