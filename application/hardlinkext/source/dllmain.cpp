// dllmain.cpp : DllMain 的实现。

#include "stdafx.h"

#ifdef _DEBUG
#include <vld.h>
#include <vldapi.h>
#endif

#include "../resource/resource.h"
#include "../hardlinkext_i.h"
#include "dllmain.h"
#include "HardLinkListDlg.h"

#include <stdext.h>
LOCAL_LOGGER_ENABLE(_T("hlchk"), (LOGGER_LEVEL_ERROR) );

ChardlinkextModule _AtlModule;

// DLL 入口点
extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:	{
		LOG_RELEASE(_T("starting..."));
		LOGGER_SELECT_COL(	0
			| CJCLogger::COL_THREAD_ID 
			| CJCLogger::COL_SIGNATURE 
			| CJCLogger::COL_FUNCTION_NAME);
		TCHAR str[MAX_PATH];
		wmemset(str, 0, MAX_PATH);
		GetModuleFileNameW(_AtlBaseModule.GetModuleInstance(), str, MAX_PATH);
		LPTSTR sfile = _tcsrchr(str, _T('\\') );
		if (sfile != 0) _tcscpy_s(sfile +1, (MAX_PATH - (sfile - str) -1), _T("jclog.cfg") );
		LOG_DEBUG( _T("loading config %s"), str )
		LOGGER_CONFIG(str);
		break;	}
	case DLL_THREAD_ATTACH:	{
		LOG_DEBUG_(1, _T("thread attached %d"), GetCurrentThreadId() );
		break;				}
	case DLL_THREAD_DETACH:	{
		_AtlModule.CleanDialogPool();
		LOG_DEBUG_(1, _T("thread detached %d"), GetCurrentThreadId() );
		break;				}
	case DLL_PROCESS_DETACH:	{
		LOG_RELEASE(_T("unloading..."));
		break;					}
	}
	hInstance;
	return _AtlModule.DllMain(dwReason, lpReserved); 
}

void ChardlinkextModule::CleanDialogPool(void)
{
	LOG_STACK_TRACE();
	if ( m_dialog_pool.empty() ) return;

	DIALOG_POOL::iterator it = m_dialog_pool.begin();
	DIALOG_POOL::iterator endit = m_dialog_pool.end();
	//bool empty = m_dialog_pool.empty();
	for (;it != endit; )
	{
		CHardLinkListDlg * dlg = (*it);
		if (dlg && (dlg->m_hWnd == NULL) )
		{
			LOG_DEBUG(_T("delete dialog 0x%08X"), (UINT)(dlg) );
			delete dlg;
			DIALOG_POOL::iterator oldit = it;
			++it;
			m_dialog_pool.erase(oldit);
		}
		else ++it;
	}
	LONG lock = 0;
	// from not empty to empty
	//if ( m_dialog_pool.empty() ) lock = Unlock();
	//LOG_DEBUG(_T("--lock = %d"), lock);
}

void ChardlinkextModule::CreateHllDlg(CHardLinkListDlg * &dlg)
{
	LONG lock = 0;
	//if (m_dialog_pool.empty() ) lock = Lock();
	//LOG_DEBUG(_T("++lock = %d"), lock);

	JCASSERT(NULL == dlg);
	dlg = new CHardLinkListDlg;
	m_dialog_pool.push_back(dlg);
}

