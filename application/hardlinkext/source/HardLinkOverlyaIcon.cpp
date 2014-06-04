// HardLinkOverlyaIcon.cpp : CHardLinkOverlyaIcon 的实现

#include "stdafx.h"
#include "HardLinkOverlyaIcon.h"
#include <stdext.h>

LOCAL_LOGGER_ENABLE(_T("hlchk.core"), LOGGER_LEVEL_ERROR);

// CHardLinkOverlyaIcon

DWORD GetLinkCount(LPCTSTR fn)
{
	LOG_STACK_TRACE();
	LOG_DEBUG(_T("file = '%s'"), fn);

	HANDLE hfile = CreateFile(fn, 
		FILE_READ_ATTRIBUTES | FILE_READ_EA,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if ( NULL == (HANDLE)hfile || INVALID_HANDLE_VALUE == (HANDLE)hfile)
	{
		LOG_WARNING(_T("failed on openning file %s"), fn);
		return S_FALSE;
	}

	BY_HANDLE_FILE_INFORMATION info;
	BOOL br = GetFileInformationByHandle(hfile, &info);
	DWORD link_count = info.nNumberOfLinks;
	LOG_DEBUG(_T("links = %d"), link_count);
	CloseHandle(hfile);
	if (!br)
	{
		LOG_ERROR(_T("failed on reading attribute %s"), fn);
		return 0;
	}
	return link_count;
}


STDMETHODIMP CHardLinkOverlyaIcon::GetOverlayInfo(
		LPWSTR pwszIconFile, 
        int cchMax,int *pIndex,DWORD* pdwFlags)
{
	LOG_STACK_TRACE();

	// Get our module's full path
	TCHAR str[MAX_PATH];
	wmemset(str, 0, MAX_PATH);
	GetModuleFileNameW(_AtlBaseModule.GetModuleInstance(), str, MAX_PATH);
	LOG_DEBUG(_T("module path = '%s'"), str);
	LPTSTR sfile = _tcsrchr(str, _T('\\') );
	if (sfile != 0) _tcscpy_s(sfile +1, (MAX_PATH - (sfile - str) -1), _T("link.ico") );
	
	_tcscpy_s(pwszIconFile, cchMax, str); 
	LOG_DEBUG(_T("icon file = '%s', buf = %d"), pwszIconFile, cchMax);
	// Use first icon in the resource
	*pIndex=0; 
	*pdwFlags = ISIOI_ICONFILE | ISIOI_ICONINDEX;
	return S_OK;
}

STDMETHODIMP CHardLinkOverlyaIcon::GetPriority(
	int* pPriority)
{
	LOG_STACK_TRACE();

	// we want highest priority 
	*pPriority=0;
	return S_OK;
}

STDMETHODIMP CHardLinkOverlyaIcon::IsMemberOf(
	LPCWSTR pwszPath, DWORD dwAttrib)
{
	LOG_STACK_TRACE();
/*
	LOG_DEBUG(_T("file = '%s'"), pwszPath);

	HANDLE hfile = CreateFile(pwszPath, 
		FILE_READ_ATTRIBUTES | FILE_READ_EA,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if ( NULL == (HANDLE)hfile || INVALID_HANDLE_VALUE == (HANDLE)hfile)
	{
		LOG_WARNING(_T("failed on openning file %s"), pwszPath);
		return S_FALSE;
	}

	BY_HANDLE_FILE_INFORMATION info;
	BOOL br = GetFileInformationByHandle(hfile, &info);
	LOG_DEBUG(_T("links = %d"), info.nNumberOfLinks);
	CloseHandle(hfile);

	if (!br)
	{
		LOG_ERROR(_T("failed on reading attribute %s"), pwszPath);
		return S_FALSE;
	}
*/

	if ( GetLinkCount(pwszPath) > 1)	return S_OK;
	else								return S_FALSE;
}
