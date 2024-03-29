
#include "stdafx.h"

//#include <CommClass.h>
#include <afxinet.h>
//#include <atlrx.h>
#include "..\Comm\RegExpEx.h"

#include <stdext.h>


// 全局变量

//#define _TEST_

#ifdef _TEST_
static LPCTSTR g_strServerName = _T("43.14.212.58");	// 服务器
static LPCTSTR g_check_start_page = _T("loginfp_test");	// 登陆请求文件
static LPCTSTR g_check_logged_page = _T("loginsu_test");	// 登陆请求文件
static LPCTSTR g_str_start_page = _T("/WebSQL/WebSQL.srf");	// 登陆请求文件
static LPCTSTR g_str_logged_page = _T("/WebSQL/WebSQL.srf");	// 登陆请求文件
#else
static LPCTSTR g_strServerName = _T("www.netyi.net");	// 服务器
static LPCTSTR g_str_start_page = _T("/controls/loginfp.aspx");	// 登陆请求文件
static LPCTSTR g_check_start_page = _T("loginfp");	// 登陆请求文件
static LPCTSTR g_str_logged_page = _T("/controls/loginsu.aspx");	// 登陆请求文件
static LPCTSTR g_check_logged_page = _T("loginsu");	// 登陆请求文件
//static LPCTSTR g_str_login_page = _T("/controls/loginfp.aspx");	// 登陆请求文件
//static LPCTSTR g_str_logout_page = _T("/controls/loginsu.aspx");	// 登陆请求文件
#endif

static LPCTSTR g_strHeader = _T("Content-Type: application/x-www-form-urlencoded");
// 用于匹配guid的正则表达式
static LPCSTR g_viewstate_match_exp = "(id=\\\"__VIEWSTATE\\\"\\b*value=\\\"){[^\\\"]*}\\\"";
static LPCSTR g_eventvalidation_match_exp = "(id=\\\"__EVENTVALIDATION\\\"\\b*value=\\\"){[^\\\"]*}\\\"";

typedef const unsigned char * LPCMBSTR;
//#include <log_on.h>



void GetGUID(CStringA &strBuf, LPCSTR match_exp, CStringA &strGuid)
{
	// 取得GUID
	strGuid.Empty();
	CAtlRegExp<CAtlRECharTraitsA> regexp;		// Ascii字符
	REParseError sr = regexp.Parse(match_exp);
	ASSERT(sr == REPARSE_ERROR_OK);
	CAtlREMatchContext<CAtlRECharTraitsA> mcText;
	BOOL br = regexp.Match(strBuf, &mcText);
	if (!br) return;		// 未找到，返回strGuid = _T("");
	ASSERT(mcText.m_uNumGroups > 0);
	GetMatchString(mcText, 0, strGuid);
}


void RecieveRequest(CHttpConnection * conn, CHttpFile * &lpFile, CStringA &strBuf)
{
	// 从一个打开的Http Request文件读取数据到字符串
	// 参数：
	//	[in]
	//	[out]
	ASSERT(lpFile);
	char * str = NULL;
	static const LONGLONG iFileLen = 1048576;		// 缓存大小 1M
	LONGLONG iRemain = iFileLen, iReadPos = 0, iReaded = 1;

	while (1)
	{
		DWORD dwRes;
		lpFile->QueryInfoStatusCode(dwRes);

		if ( (dwRes == HTTP_STATUS_MOVED) || (dwRes == HTTP_STATUS_REDIRECT) || (dwRes == HTTP_STATUS_REDIRECT_METHOD))
		{
			// 处理重定向
			CString new_location, new_page;
			lpFile->QueryInfo(HTTP_QUERY_RAW_HEADERS_CRLF, new_location);
			new_location.MakeLower();

			// 取得GUID
			CAtlRegExp<CAtlRECharTraits> regexp;		// Ascii字符
			REParseError sr = regexp.Parse(_T("location:\\b*{(/|\\c)*\\.(\\c)*}"));
			ASSERT(sr == REPARSE_ERROR_OK);
			CAtlREMatchContext<CAtlRECharTraits> mcText;
			BOOL br = regexp.Match(new_location, &mcText);
			if (!br) THROW_ERROR(ERR_APP, "Unknow redirection.%s", new_location);
			ASSERT(mcText.m_uNumGroups > 0);
			GetMatchString(mcText, 0, new_page);
			lpFile->Close();
			delete lpFile;

			lpFile = conn->OpenRequest(
				CHttpConnection::HTTP_VERB_GET, new_page, NULL, 1, NULL, NULL,
				INTERNET_FLAG_EXISTING_CONNECT | INTERNET_FLAG_NO_AUTO_REDIRECT);
			lpFile->SendRequest();
			continue;
		}
		else if (dwRes == HTTP_STATUS_DENIED) THROW_ERROR(ERR_APP, "Access is Denied");
		break;
	}

	// Read data
	str = strBuf.GetBufferSetLength((int)iFileLen);
	while ( (iRemain > 0) && (iReaded>0))
	{
		iReaded = lpFile->Read(str+iReadPos, (UINT)iRemain);
		iReadPos += iReaded;
		iRemain -= iReaded;
	}
	strBuf.ReleaseBuffer((int)iReadPos);
	str = NULL;
}

BOOL Logout(CHttpConnection * lpConn, CStringA & str_viewstate, CStringA & str_buf, CString & title)
{
	// 执行退出登陆操作。
	//	lpConn [in] 
	//	str_viewstate [in]	loginSU.aspx文件中取得的viewstate
	//	buf [out]			退出登陆后，取得的loginFP.aspx的内容

	// 取得GUID
	CHttpFile			*page_file = NULL;
	BOOL br = TRUE;
	try
	{
		page_file = lpConn->OpenRequest(
			CHttpConnection::HTTP_VERB_POST, g_str_logged_page, NULL, 1, NULL, NULL,
			INTERNET_FLAG_EXISTING_CONNECT | INTERNET_FLAG_NO_AUTO_REDIRECT);
		ASSERT(page_file);

		CStringA strFormData;
		strFormData.Format("btnLogout.y=10&btnLogout.x=30&__VIEWSTATE=%s", str_viewstate);
		page_file->SendRequest(
			CString(g_strHeader), (LPVOID)(LPCSTR)strFormData, strFormData.GetLength());

		RecieveRequest(lpConn, page_file, str_buf);
		
		title = page_file->GetFileURL();
		if (title.Find(g_check_logged_page) >= 0)		br = FALSE;
		/*THROW_ERROR(ERR_APP, ("log out failed."), title);*/
	}
	catch(...)
	{
		//LOG0(_T("\r\n"));
		if (page_file)
		{
			page_file->Close();
			delete page_file;
		}
		throw;
	}
	page_file->Close();	// 关闭文件
	delete page_file;
	return br;
}

void OpenLoginPage(
		CHttpConnection * lpConn, CStringA & str_viewstate, CStringA & str_eventvalidation)
{
	// 连接服务器，打开登陆页
	ASSERT(lpConn);
	CHttpFile			* page_file = NULL;
	try
	{
		int retry = 0;
		do
		{
			retry ++;
			if (retry > 3) THROW_ERROR(ERR_APP, ("failed over 3 times when open start page."));
			page_file = lpConn->OpenRequest(
				CHttpConnection::HTTP_VERB_GET, g_str_start_page, NULL, 1, NULL, NULL,
				INTERNET_FLAG_EXISTING_CONNECT | INTERNET_FLAG_NO_AUTO_REDIRECT);
			ASSERT(page_file);
			page_file->SendRequest();

			// 读取文件
			CStringA str_buf;
			RecieveRequest(lpConn, page_file, str_buf);

			// 检查打开的页面是否是 loginFP.aspx
			CString str_file_title;
			str_file_title = page_file->GetFileTitle();
			//str_file_title = page_file->GetFileName();
			//str_file_title = page_file->GetFilePath();
			//str_file_title = page_file->GetFileURL();
			str_file_title.MakeLower();

			page_file->Close();
			delete page_file;
			page_file = NULL;

			GetGUID(str_buf, g_viewstate_match_exp, str_viewstate);
			if (str_viewstate.IsEmpty() )	THROW_ERROR(ERR_APP, ("can not get viewstate in start page"));

			if (str_file_title.Find(g_check_start_page) >= 0)			break;
			else if (str_file_title.Find(g_check_logged_page) >= 0)			// 已经登陆
			{
				BOOL br = Logout(lpConn, str_viewstate, str_buf, str_file_title);
				if (!br) THROW_ERROR(ERR_APP, ("logout failed when open start page."));
				if (str_file_title.Find(g_str_start_page) < 0) continue;	// 重试

				GetGUID(str_buf, g_viewstate_match_exp, str_viewstate);
				if (str_viewstate.IsEmpty() )	THROW_ERROR(ERR_APP, ("can not get viewstate in start page"));
			}
			else
			{
				CStringA str;
				str.Format("unknow start page (titile = %S)", str_file_title);
				THROW_ERROR(ERR_APP, str);
				
			}
		}
		while (1);
	}
	catch (...)
	{
		if (page_file)
		{
			page_file->Close();
			delete page_file;
		}
		throw;
	}
}

int CheckPoint(CStringA &strBuf)
{
	// 检查是否登陆成功，如果登陆成功，检查现在有的点数
	//	返回现在有的点数，若登陆失败，则返回-1;

	CAtlRegExp<CAtlRECharTraitsMB> regexp;		// Ascii字符
	REParseError sr = regexp.Parse(reinterpret_cast<LPCMBSTR>("(id=\\\"lblScore\\\">)\\b*{\\d+}(<)"));
	ASSERT(sr == REPARSE_ERROR_OK);

	CAtlREMatchContext<CAtlRECharTraitsMB> mcText;
	BOOL br = regexp.Match(reinterpret_cast<LPCMBSTR>(strBuf.GetString()), &mcText);
	if ( (!br) || (mcText.m_uNumGroups<=0) ) return -1;
	CStringA strPoint;
	GetMatchString(mcText, 0, strPoint);
	return atoi(strPoint);
}

int Login(LPCSTR strUserName, LPCSTR strPassword)
{
	CInternetSession inet_session(_T("MSIE6"));
	CHttpConnection		*lpConn = NULL;
	CHttpFile			*lpFile = NULL;
	int point = -1;			// 如果成功，则返回取得的点数，否则返回-1

	try
	{
		CStringA str_buf;
		lpConn = inet_session.GetHttpConnection(g_strServerName);	ASSERT(lpConn);
		CStringA str_viewstate, str_eventvalidation;

		// 连接服务器，打开登陆页
		OpenLoginPage(lpConn, str_viewstate, str_eventvalidation);

		lpFile = lpConn->OpenRequest(
			CHttpConnection::HTTP_VERB_POST, g_str_start_page,
			NULL, 1, NULL, NULL,
			INTERNET_FLAG_EXISTING_CONNECT | INTERNET_FLAG_NO_AUTO_REDIRECT);
		ASSERT(lpFile);

		CStringA str_option;
		str_option.Format(
			"__EVENTTARGET=&btnLogin.y=22&btnLogin.x=22&tbMemberName=%s&tbPassword=%s&&__VIEWSTATE=%s&__EVENTARGUMENT",
			strUserName, strPassword, str_viewstate);

		lpFile->SendRequest(CString(g_strHeader), (LPVOID)(LPCSTR)str_option, str_option.GetLength());
		// 读取数据
		RecieveRequest(lpConn, lpFile, str_buf);
		//  检查文件名，判断是否登陆成功
		//CString str_title = lpFile->GetFileTitle();
		//CString str_title = lpFile->GetFileURL();
		CString str_title = lpFile->GetFileTitle();
		lpFile->Close();		// 关闭文件
		delete lpFile;
		lpFile = NULL;

		if (str_title.Find(g_check_logged_page) < 0)			// 登陆失败		
		{
			//CStringA str;
			//str.Format("login failed (title = %s)"), );
			THROW_ERROR(ERR_APP, "login failed (title = %S)", str_title.GetString() );		
		}

		// 取得点数
		point = CheckPoint(str_buf);
		if (point < 0) THROW_ERROR(ERR_APP, ("can not get point."));		// 登陆失败
		//LOG1(_T("succeeded, point = %d"), point);

		GetGUID(str_buf, g_viewstate_match_exp, str_viewstate);
		if (str_viewstate.IsEmpty() )	THROW_ERROR(ERR_APP, ("can not get viewstate for logout"));

		// 退出登陆
		Logout(lpConn, str_viewstate, str_buf, str_title);
	}
	catch (...)
	{
		if (lpFile)
		{
			lpFile->Close();
			delete lpFile;
		}

		if (lpConn)
		{
			lpConn->Close();
			delete lpConn;
		}
		inet_session.Close();		
		throw; 
	}
	if (lpFile)
	{
		lpFile->Close();
		delete lpFile;
	}

	if (lpConn)
	{
		lpConn->Close();
		delete lpConn;
	}
	inet_session.Close();


	//CLOSE_LOG();
	return point;
}

//#include <log_off.h>