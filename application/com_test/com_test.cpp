// com_test.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <vld.h>
#include <stdext.h>
#include <jcparam.h>
#include <jcapp.h>

LOCAL_LOGGER_ENABLE(_T("com_test"), LOGGER_LEVEL_DEBUGINFO);


class CComTestApp : public jcapp::CJCAppBase<jcapp::AppArguSupport>
{
public:
	CComTestApp(void) 
		: jcapp::CJCAppBase<jcapp::AppArguSupport>(ARGU_SUPPORT_HELP | ARGU_SUPPORT_OUTFILE) 
		, m_port(0) {};
public:
	bool Initialize(void);
	virtual int Run(void);
	void CleanUp(void);

public:
	UINT m_port;

};

typedef jcapp::CJCApp<CComTestApp>	CApplication;
static CApplication the_app;
#define _class_name_	CApplication

BEGIN_ARGU_DEF_TABLE()
	ARGU_DEF_ITEM(_T("port"),	_T('p'), UINT,	m_port, _T("com port id.") )
END_ARGU_DEF_TABLE()

bool CComTestApp::Initialize(void)
{
	return true;
}

int CComTestApp::Run(void)
{
	TCHAR port_name[MAX_PATH];
	stdext::jc_sprintf(port_name, _T("\\\\.\\COM%d"), m_port);

	HANDLE com_file = CreateFile(port_name, 
				GENERIC_READ|GENERIC_WRITE, 
				FILE_SHARE_READ | FILE_SHARE_WRITE, 
				NULL, 
				OPEN_ALWAYS, 
				FILE_ATTRIBUTE_NORMAL,					//如果使用NO_BUFFERING选项，文件操作必须sector对齐。
				NULL );
	if (com_file == INVALID_HANDLE_VALUE) THROW_WIN32_ERROR(_T("failur on opening com port"));

	BYTE buf[256];
	DWORD readed = 0;
	while (1)
	{
		readed = 0;
		BOOL br = ReadFile(com_file, buf, 1, &readed, NULL);
		if (!br) THROW_WIN32_ERROR(_T("failure on reading com port"));
		stdext::jc_printf(_T("in: "));
		for (DWORD ii = 0; ii < readed; ++ ii)
		{
			stdext::jc_printf(_T("0x%02X "), buf[ii]);
		}
		stdext::jc_printf(_T("\n"));
	}
}

void CComTestApp::CleanUp(void)
{
	__super::CleanUp();
}

int _tmain(int argc, _TCHAR* argv[])
{
	int ret_code = 0;
	try
	{
		the_app.Initialize();
		ret_code = the_app.Run();
	}
	catch (stdext::CJCException & err)
	{
		stdext::jc_fprintf(stderr, _T("error: %s\n"), err.WhatT() );
		ret_code = err.GetErrorID();
	}
	the_app.CleanUp();

	stdext::jc_printf(_T("Press any key to continue..."));
	getc(stdin);
	return ret_code;
}
