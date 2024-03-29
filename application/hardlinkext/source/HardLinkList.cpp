// HardLinkList.cpp : CHardLinkList 的实现

#include "stdafx.h"
#include <stdext.h>

#include "HardLinkList.h"
#include "HardLinkOverlyaIcon.h"
#include "HardLinkListDlg.h"
#include "dllmain.h"

// CHardLinkList

LOCAL_LOGGER_ENABLE(_T("hlchk.list"), LOGGER_LEVEL_ERROR);

// CHardLinkList

CHardLinkList::CHardLinkList()
{
	LOG_DEBUG(_T("constructed 0x%08X"), (UINT)(this) );
}

CHardLinkList::~CHardLinkList()
{
	LOG_DEBUG(_T("destructed 0x%08X"), (UINT)(this) );
}

STDMETHODIMP CHardLinkList::Initialize(
		LPCITEMIDLIST pidlFolder, LPDATAOBJECT pDataObj, HKEY hProgID)
{
	LOG_DEBUG(_T("intialized"));
	FORMATETC fmt = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	STGMEDIUM stg = { TYMED_HGLOBAL };
	HDROP      hDrop;

	// 在数据对象内查找CF_HDROP类型数据。
	// 如果没有数据，返回一个错误（“无效参数”）给Explorer。
	if ( FAILED( pDataObj->GetData ( &fmt, &stg ) ))	return E_INVALIDARG;
	
	// 取得指向实际数据的指针。
	hDrop = (HDROP) GlobalLock ( stg.hGlobal );
	
	// 确保非NULL
    if ( NULL == hDrop )	return E_INVALIDARG;

	// 有效性检查，至少有一个文件名
	UINT uNumFiles = DragQueryFile ( hDrop, 0xFFFFFFFF, NULL, 0 );
	HRESULT hr = S_OK;

	if ( 0 == uNumFiles )
	{
		GlobalUnlock ( stg.hGlobal );
		ReleaseStgMedium ( &stg );
		return E_INVALIDARG;
	}

	// 取得第一个文件名，保存到 m_szFile
    if ( 0 == DragQueryFile ( hDrop, 0, m_file_name, MAX_PATH ) )	hr = E_INVALIDARG;
	
	GlobalUnlock ( stg.hGlobal );
    ReleaseStgMedium ( &stg );
	return hr;
}

STDMETHODIMP CHardLinkList::QueryContextMenu (HMENU hmenu, UINT menu_index, UINT id_first_cmd, UINT id_last_cmd, UINT flags)
{
	LOG_DEBUG(_T("first %d, last %d"), id_first_cmd, id_last_cmd);
	// 如果标识包含了 CMF_DEFAULTONLY，那么，我们啥都不做
	if ( flags & CMF_DEFAULTONLY ) return MAKE_HRESULT ( SEVERITY_SUCCESS, FACILITY_NULL, 0 );
	int menu_count = 0;

	if ( GetLinkCount(m_file_name, m_file_id) > 1)
	{	// it is a hardlink file
		InsertMenu(hmenu, menu_index, MF_BYPOSITION, id_first_cmd, _T("hard link list ...") );
		menu_count ++;
		InsertMenu(hmenu, menu_index+1, MF_BYPOSITION, id_first_cmd +1, _T("de-link") );
		menu_count ++;
	}

	return MAKE_HRESULT ( SEVERITY_SUCCESS, FACILITY_NULL, menu_count );
}

STDMETHODIMP CHardLinkList::GetCommandString (UINT id_cmd, UINT flag, UINT *, LPSTR name, UINT name_len)
{
	USES_CONVERSION;
	// 由于这里只有一个菜单项，所以idCmd 必须为0
    if ( 0 != id_cmd )	return E_INVALIDARG;
 
	// 如果Explorer请求提示信息，拷贝串到提供的缓冲区
    if ( flag & GCS_HELPTEXT )
    {
		LPCTSTR szText = _T("show all hard linked fies");

		// 这里，需要把 pszName 转换为 UNICODE
		if ( flag & GCS_UNICODE )	lstrcpynW ( (LPWSTR) name, T2CW(szText), name_len );
		else		lstrcpynA ( name, T2CA(szText), name_len );		// ANSI版本
		return S_OK;
	}
	return E_INVALIDARG;
}

#define BUF_SIZE 4096

STDMETHODIMP CHardLinkList::InvokeCommand (LPCMINVOKECOMMANDINFO cmd_info)
{
	// 如果 lpVerb 指向一个实际串，忽略此次调用并退出
	if ( 0 != HIWORD( cmd_info->lpVerb ) )	return E_INVALIDARG;
	// 取得命令索引，这里，唯一有效的值为0
#ifdef _DEBUG
	LOG_DEBUG(_T("mask: 0x%08X"), cmd_info->fMask);
	if ( IS_INTRESOURCE(cmd_info->lpVerb) )
	{
		LOG_DEBUG(_T("verb id: 0x%08X"), (UINT)(cmd_info->lpVerb) );
	}
	else
	{
		LOG_DEBUG(_T("verb string: '%s'"), cmd_info->lpVerb);
	}
	LOG_DEBUG(_T("parameter: '%S'"), cmd_info->lpParameters);
	LOG_DEBUG(_T("dir: '%S'"), cmd_info->lpDirectory);
	LOG_DEBUG(_T("show: %d"), cmd_info->nShow);
	LOG_DEBUG(_T("hotkey: 0x%08X"), cmd_info->dwHotKey);

#endif
	HRESULT hr = S_OK;
	switch ( LOWORD( cmd_info->lpVerb) )
	{
	case 0:	{
		hr = ShowFileList(cmd_info);
		
		//BYTE * buf = new BYTE[BUF_SIZE];
		//DWORD check_sum = 0;
		//FILE * file = NULL;
		//_tfopen_s(&file, m_file_name, _T("rb"));
		//while ( file && !feof(file) )
		//{
		//	DWORD read = fread(buf, 1, BUF_SIZE, file);
		//	for (DWORD ii = 0; ii < read; ++ii)	check_sum += buf[ii];
		//}
		//fclose(file);
		//delete [] buf;

		//TCHAR szMsg [2 * MAX_PATH];

		//wsprintf ( szMsg, _T("file_name:%s\n\ncheck_sum:0x%08X"), m_file_name, check_sum );
		//MessageBox ( cmd_info->hwnd, szMsg, _T("CheckSum"), MB_ICONINFORMATION );
		//return S_OK;
		break;	}

	case 1: {	// de-link
		hr = DeLink();

		break; 		}
 
	default:
		return E_INVALIDARG;
		break;
	}
	return hr;
}

HRESULT CHardLinkList::ShowFileList(LPCMINVOKECOMMANDINFO cmd_info)
{
	LOG_STACK_TRACE();
	LOG_DEBUG(_T("find fn: '%s'"), m_file_name);
	// calculate check sum
	CHardLinkListDlg * dlg = NULL;
	_AtlModule.CreateHllDlg(dlg);
	if (!dlg)
	{
		LOG_ERROR(_T("failure on createing dialog object"));
		return E_INVALIDARG;
	}

	dlg->Create( cmd_info->hwnd );

	TCHAR title[MAX_PATH];
	_stprintf_s(title, _T("file names of %I64X"), m_file_id);
	dlg->SetWindowTextW(title);

	TCHAR hdlink[MAX_PATH];
	DWORD len = MAX_PATH;
	HANDLE find = FindFirstFileNameW(m_file_name, 0, &len, hdlink);
	while (find)
	{
		dlg->InsertItem( hdlink );
		len = MAX_PATH;
		BOOL br = FindNextFileNameW(find, &len, hdlink);
		if (!br) break;
	}
	FindClose(find);

	dlg->CenterWindow();
	dlg->ShowWindow(SW_SHOW);
	dlg->UpdateWindow();
	return S_OK;
}

HRESULT CHardLinkList::DeLink()
{
	LOG_STACK_TRACE();
	// rename original file
	TCHAR tmp_fn[MAX_PATH];
	_tcscpy_s(tmp_fn, MAX_PATH, m_file_name);
	JCSIZE len = _tcslen(tmp_fn);
	_tcscat_s(tmp_fn + len, MAX_PATH - len, _T(".delink"));
	LOG_DEBUG(_T("temp file name: '%s'"), tmp_fn);

	BOOL br;
	br = MoveFile(m_file_name, tmp_fn);
	if (!br)	
	{
		LOG_ERROR(_T("move file '%s' to '%s' failed"), m_file_name, tmp_fn);
		return E_INVALIDARG;
	}

	// copy file
	br = CopyFile(tmp_fn, m_file_name, FALSE);
	if (!br)	
	{
		LOG_ERROR(_T("copy file '%s' to '%s' failed"), tmp_fn, m_file_name);
		return E_INVALIDARG;
	}
	// delete link
	br = DeleteFile(tmp_fn);
	if (!br)	
	{
		LOG_ERROR(_T("delete file '%s' failed"), tmp_fn);
		return E_INVALIDARG;
	}
	return S_OK;
}
