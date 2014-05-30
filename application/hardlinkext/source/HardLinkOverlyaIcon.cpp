// HardLinkOverlyaIcon.cpp : CHardLinkOverlyaIcon 的实现

#include "stdafx.h"
#include "HardLinkOverlyaIcon.h"
#include <stdext.h>

LOCAL_LOGGER_ENABLE(_T("hlchk"), LOGGER_LEVEL_DEBUGINFO);

// CHardLinkOverlyaIcon

STDMETHODIMP CHardLinkOverlyaIcon::GetOverlayInfo(
		LPWSTR pwszIconFile, 
        int cchMax,int *pIndex,DWORD* pdwFlags)
{
	LOG_STACK_TRACE();

	// Get our module's full path
	GetModuleFileNameW(_AtlBaseModule.GetModuleInstance(), pwszIconFile, cchMax);
	LOG_DEBUG(_T("icon file = '%s'"), pwszIconFile);
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
	LOG_DEBUG(_T("file = '%s'"), pwszPath);

	wchar_t *s = _wcsdup(pwszPath);
	HRESULT r = S_FALSE;
	_wcslwr(s);

	// Criteria
	if (wcsstr(s, L".txt") != 0)	r = S_OK;
	free(s);
	return r;
}
