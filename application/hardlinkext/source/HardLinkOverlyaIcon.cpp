// HardLinkOverlyaIcon.cpp : CHardLinkOverlyaIcon 的实现

#include "stdafx.h"
#include "HardLinkOverlyaIcon.h"


// CHardLinkOverlyaIcon

STDMETHODIMP CHardLinkOverlyaIcon::GetOverlayInfo(
		LPWSTR pwszIconFile, 
        int cchMax,int *pIndex,DWORD* pdwFlags)
{
	// Get our module's full path
	GetModuleFileNameW(_AtlBaseModule.GetModuleInstance(), pwszIconFile, cchMax);
	// Use first icon in the resource
	*pIndex=0; 
	*pdwFlags = ISIOI_ICONFILE | ISIOI_ICONINDEX;
	return S_OK;
}

STDMETHODIMP CHardLinkOverlyaIcon::GetPriority(
	int* pPriority)
{
	// we want highest priority 
	*pPriority=0;
	return S_OK;
}

STDMETHODIMP CHardLinkOverlyaIcon::IsMemberOf(
	LPCWSTR pwszPath, DWORD dwAttrib)
{
	wchar_t *s = _wcsdup(pwszPath);
	HRESULT r = S_FALSE;
	_wcslwr(s);

	// Criteria
	if (wcsstr(s, L".txt") != 0)	r = S_OK;
	free(s);
	return r;
}
