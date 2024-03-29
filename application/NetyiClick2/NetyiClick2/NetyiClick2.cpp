// NetyiClick2.cpp : 定义应用程序的类行为。
//

#include "stdafx.h"
#include "NetyiClick2.h"
#include "NetyiClick2Dlg.h"

#include "NetyiAccRow.h"
#include <float.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CNetyiClick2App

BEGIN_MESSAGE_MAP(CNetyiClick2App, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


VOID WINAPI StartClick(PVOID param, BOOL is_timer);

// CNetyiClick2App 构造

CNetyiClick2App::CNetyiClick2App()
{
	// TODO: 在此处添加构造代码，
	// 将所有重要的初始化放置在 InitInstance 中
}


// 唯一的一个 CNetyiClick2App 对象

CNetyiClick2App theApp;


// CNetyiClick2App 初始化

#include <log_on.h>
static HANDLE logfile = INVALID_HANDLE_VALUE;

BOOL CNetyiClick2App::InitInstance()
{
	// 如果一个运行在 Windows XP 上的应用程序清单指定要
	// 使用 ComCtl32.dll 版本 6 或更高版本来启用可视化方式，
	//则需要 InitCommonControlsEx()。否则，将无法创建窗口。
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// 将它设置为包括所有要在应用程序中使用的
	// 公共控件类。
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	AfxEnableControlContainer();

	// 标准初始化
	// 如果未使用这些功能并希望减小
	// 最终可执行文件的大小，则应移除下列
	// 不需要的特定初始化例程
	// 更改用于存储设置的注册表项
	// TODO: 应适当修改该字符串，
	// 例如修改为公司或组织名
	SetRegistryKey(_T("应用程序向导生成的本地应用程序"));

	logfile = CreateFile(
		_T("NetyiClick.log"), GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (logfile == INVALID_HANDLE_VALUE)	return 2;		// 打开文件失败
	SetFilePointer(logfile, 0, 0, FILE_END);				// 移到文件尾部，追加


	//CNetyiClick2Dlg dlg;
	//m_pMainWnd = &dlg;
	//INT_PTR nResponse = dlg.DoModal();
	//if (nResponse == IDOK)
	//{
	//	// TODO: 在此处放置处理何时用“确定”来关闭
	//	//  对话框的代码
	//}
	//else if (nResponse == IDCANCEL)
	//{
	//	// TODO: 在此放置处理何时用“取消”来关闭
	//	//  对话框的代码
	//}

	// 由于对话框已关闭，所以将返回 FALSE 以便退出应用程序，
	//  而不是启动应用程序的消息泵。

	StartClick(NULL, TRUE);

	CLOSE_LOG();

	return FALSE;
}


VOID WINAPI StartClick(PVOID param, BOOL is_timer)
{
	srand( (unsigned)time( NULL ) );

	CMySQLClient		database;
	CNetyiAccRow		* min_acc = NULL;

	try
	{
		// 打开数据库
		database.Connect(NULL, 0, _T("netyiaccount"), _T("root"), _T("aaddggjj"));

		CTypedDBTable<CNetyiAccRow> result(_T("account"), &database);
		result.Open();

		float		min_sort_key = FLT_MAX;
		while (1) 
		{
			// 获取表格中的行
			CNetyiAccRow * acc = result.GetNextRow();
			if (!acc) break;

			// 将clicked字段加上一随计数，并且取得排序最小的用户
			float fsort = (float)rand() / (float)(RAND_MAX) * 5 + (float)acc->m_today_clicked;
			if (min_sort_key > fsort)
			{
				if (min_acc) min_acc->Release();
				min_sort_key = fsort, min_acc = acc;
			}
			else							acc->Release();
		}
		ASSERT(min_acc);

		// 模拟访问排序最小的用户
		//AD_Access(min_acc->m_user);

		// 用户的当天访问次数加1，保存之数据库
		min_acc->m_today_clicked ++;
		min_acc->m_clicked ++;

		LOGNOWEX;
		LOG2(_T(" Click \"%s\" today = %d\r\n"), min_acc->m_user, min_acc->m_today_clicked);

		result.SaveItem(min_acc);
	}
	catch (CBKException *err)
	{
		LOGNOWEX;
		LOG_ERROR(err);
		err->Delete();
		//exit_code = 1;
	}

	if (min_acc)	min_acc->Release();
	database.Disconnect();
}

#include <log_off.h>