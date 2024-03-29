// NetyiLogin2.h : PROJECT_NAME 应用程序的主头文件
//

#pragma once

#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含“stdafx.h”以生成 PCH 文件"
#endif

#include "resource.h"		// 主符号


// CNetyiLogin2App:
// 有关此类的实现，请参阅 NetyiLogin2.cpp
//

class CNetyiLogin2App : public CWinApp
{
public:
	CNetyiLogin2App();

// 重写
	public:
	virtual BOOL InitInstance();

	CString m_db_user, m_db_password, m_run_mode;		// 通过命令行传递的数据库用户名和密码

	enum _EXIT_CODE {
		EXIT_OK=0, EXIT_CANNOT_OPEN_LOGFILE, EXIT_DATABASE_ERROR, EXIT_INTERNET_ERROR,
		EIXT_MFC_ERROR,
	};
	int m_exit_code;

// 实现

	DECLARE_MESSAGE_MAP()
	void StartLogin(LPVOID param , bool is_timer);
	void GetCommand(CString & username, CString & password, CString & runmode);
	virtual int ExitInstance();
};

extern CNetyiLogin2App theApp;