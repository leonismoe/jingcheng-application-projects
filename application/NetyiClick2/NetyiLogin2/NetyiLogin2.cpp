// NetyiLogin2.cpp : 定义应用程序的类行为。
//

#include "stdafx.h"
#include "NetyiLogin2.h"

//#include <CommClass.h>
#include <winsock2.h>
#include <jcdb.h>
#include <atlrx.h>

#include "..\Comm\NetyiAccRow.h"
#include "..\Comm\RegExpEx.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CDataLog g_log(_T("c:\\temp\\netyilogin.log"), 0);

// CNetyiLogin2App

BEGIN_MESSAGE_MAP(CNetyiLogin2App, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CNetyiLogin2App 构造

CNetyiLogin2App::CNetyiLogin2App()
	: m_exit_code(0)
{
	// TODO: 在此处添加构造代码，
	// 将所有重要的初始化放置在 InitInstance 中
}


// 唯一的一个 CNetyiLogin2App 对象

CNetyiLogin2App theApp;

extern int Login(LPCSTR strUserName, LPCSTR strPassword);

// CNetyiLogin2App 初始化


BOOL CNetyiLogin2App::InitInstance()
{
	// 如果一个运行在 Windows XP 上的应用程序清单指定要
	// 使用 ComCtl32.dll 版本 6 或更高版本来启用可视化方式，
	//则需要 InitCommonControlsEx()。否则，将无法创建窗口。
	//INITCOMMONCONTROLSEX InitCtrls;
	//InitCtrls.dwSize = sizeof(InitCtrls);
	// 将它设置为包括所有要在应用程序中使用的
	// 公共控件类。
	//InitCtrls.dwICC = ICC_WIN95_CLASSES;
	//InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	//AfxEnableControlContainer();

	// 标准初始化
	// 如果未使用这些功能并希望减小
	// 最终可执行文件的大小，则应移除下列
	// 不需要的特定初始化例程
	// 更改用于存储设置的注册表项
	// TODO: 应适当修改该字符串，
	// 例如修改为公司或组织名
	//SetRegistryKey(_T("应用程序向导生成的本地应用程序"));

	// 处理命令行参数
	GetCommand(m_db_user, m_db_password, m_run_mode);

	// 执行登陆
	StartLogin(NULL, FALSE);

	return FALSE;
}

static HANDLE logfile = INVALID_HANDLE_VALUE;				// LOG文件句柄
#include <log_on.h>

void CNetyiLogin2App::StartLogin(LPVOID param , bool is_timer)
{
	// 每次运行，随机的从数据库中取出一个账号，进行登陆，
	// 登陆以后，记录登陆的时间和点数，当天不再登陆账号。

	logfile = CreateFile(
		_T("NetyiLogin2.log"), GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (logfile == INVALID_HANDLE_VALUE)
		m_exit_code = EXIT_CANNOT_OPEN_LOGFILE;

	SetFilePointer(logfile, 0, 0, FILE_END);				// 移到文件尾部，追加
	LOGNOWEX;

	CMySQLClient		database;
	CNetyiAccRow		* acc = NULL;

	try
	{
		// 打开数据库
		database.Connect(_T("192.168.0.248"), 0, _T("netyiaccount"), m_db_user, m_db_password);

		COleDateTime today = COleDateTime::GetCurrentTime();
		CString condition;
		condition.Format(
			_T("`last_login` < \"%s 00:00:00\" ORDER BY RAND()"), today.Format(VAR_DATEVALUEONLY) );

		CTypedDBTable<CNetyiAccRow> result(_T("account"), &database);
		result.Open(condition);

		int acc_count = result.GetRowCount();
		if (acc_count <=0 ) THROW_ERROR(ERR_APP, ("no accout to process today."));

		acc = result.GetNextRow();	ASSERT(acc);

		CT2A	user_name(acc->m_user, CP_UTF8);
		CT2A	password(acc->m_password, CP_UTF8);
		LOG1(_T("login, \"%S\", "), user_name);
		int point = Login(user_name, password);

		if (point >= 0)
		{
			LOG1(_T("succeeded, point = %d"), point);
			acc->m_point = point;
			acc->m_last_login = COleDateTime::GetCurrentTime();
			result.SaveItem(acc);
		}
		result.Close();
	}
	catch (CException *err)
	{
#ifdef _DEBUG
		err->ReportError();
#endif
		TCHAR strMsg[256];
		strMsg[255] = 0;
		err->GetErrorMessage(strMsg, 255);
		LOG1(_T("failed : %s"), strMsg);
		err->Delete();
		m_exit_code = EIXT_MFC_ERROR;
	}
	
	catch (CBKException *err)
	{
#ifdef _DEBUG
		err->ReportError();
#endif
		if ( dynamic_cast<CTBException*>(err) ) m_exit_code = EXIT_DATABASE_ERROR;
		else									m_exit_code = EXIT_INTERNET_ERROR;
		LOG1(_T("failed : %s"), err->GetErrMessage());
		err->Delete();
	}

	LOG0(_T("\r\n"));
	CLOSE_LOG();

	if (acc) acc->Release();
	database.Disconnect();
}

void CNetyiLogin2App::GetCommand(CString & username, CString & password, CString & runmode)
{
	CAtlRegExp<> regexp;
	REParseError sr = regexp.Parse(_T("(/[uU]:{[^ \\t]+})?\\b*(/pw:{[^ \\t]+}\\b*)?"));
	ASSERT(sr == REPARSE_ERROR_OK);

	CAtlREMatchContext<> mcText;
	BOOL br = regexp.Match(m_lpCmdLine, &mcText);
	if ( (!br) || (mcText.m_uNumGroups < 2) ) return;
	GetMatchString(mcText, 0, username);
	GetMatchString(mcText, 1, password);
}

int CNetyiLogin2App::ExitInstance()
{
	// TODO: ここに特定なコードを追加するか、もしくは基本クラスを呼び出してください。
	CWinApp::ExitInstance();
	return m_exit_code;
}

#include <log_off.h>

