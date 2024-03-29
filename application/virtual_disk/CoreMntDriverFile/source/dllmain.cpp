// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "stdafx.h"
#include "config.h"
#include "DriverImageFile.h"

LOCAL_LOGGER_ENABLE(_T("CoreMntDriver"), LOGGER_LEVEL_DEBUGINFO);

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

extern "C" BOOL GetDriverFactory(IDriverFactory * & factory)
{
	LOG_STACK_TRACE();
	JCASSERT(factory == NULL);
	factory = &g_factory;
	factory->AddRef();
	return TRUE;
}