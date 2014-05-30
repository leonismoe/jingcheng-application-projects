// hardlinkext.cpp : DLL 导出的实现。


#include "stdafx.h"
#include "../resource.h"
#include "../hardlinkext_i.h"
#include "dllmain.h"

#include <stdext.h>

LOCAL_LOGGER_ENABLE(_T("hlchk"), LOGGER_LEVEL_DEBUGINFO);

class StartUp
{
public:
	StartUp(void)
	{
	LOGGER_SELECT_COL( 0
		| CJCLogger::COL_TIME_STAMP
		| CJCLogger::COL_FUNCTION_NAME
		| CJCLogger::COL_REAL_TIME
		);
	LOGGER_CONFIG(_T("jclog.cfg"));

	LOG_STACK_TRACE();

	MessageBox(NULL, _T("startup"), _T("debug"), MB_OK);
	}
};

static StartUp	_start_up;

// 用于确定 DLL 是否可由 OLE 卸载
STDAPI DllCanUnloadNow(void)
{
	LOG_STACK_TRACE();
    return _AtlModule.DllCanUnloadNow();
}


// 返回一个类工厂以创建所请求类型的对象
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
	LOG_STACK_TRACE();
    return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
}


// DllRegisterServer - 将项添加到系统注册表
STDAPI DllRegisterServer(void)
{
	MessageBox(NULL, _T("DllRegisterServer"), _T("debug"), MB_OK);
	LOG_STACK_TRACE();
    // 注册对象、类型库和类型库中的所有接口
    HRESULT hr = _AtlModule.DllRegisterServer();
	return hr;
}


// DllUnregisterServer - 将项从系统注册表中移除
STDAPI DllUnregisterServer(void)
{
	LOG_STACK_TRACE();
	HRESULT hr = _AtlModule.DllUnregisterServer();
	return hr;
}

// DllInstall - 按用户或者按计算机在系统注册表中添加/删除
//              项。	
STDAPI DllInstall(BOOL bInstall, LPCWSTR pszCmdLine)
{
	LOGGER_SELECT_COL( 0
		| CJCLogger::COL_TIME_STAMP
		| CJCLogger::COL_FUNCTION_NAME
		| CJCLogger::COL_REAL_TIME
		);
	LOGGER_CONFIG(_T("jclog.cfg"));

	LOG_STACK_TRACE();

	MessageBox(NULL, _T("DllInstall"), _T("debug"), MB_OK);
    HRESULT hr = E_FAIL;
    static const wchar_t szUserSwitch[] = _T("user");

    if (pszCmdLine != NULL)
    {
    	if (_wcsnicmp(pszCmdLine, szUserSwitch, _countof(szUserSwitch)) == 0)
    	{
    		AtlSetPerUserRegistration(true);
    	}
    }

    if (bInstall)
    {	
    	hr = DllRegisterServer();
    	if (FAILED(hr))
    	{	
    		DllUnregisterServer();
    	}
    }
    else
    {
    	hr = DllUnregisterServer();
    }

    return hr;
}


