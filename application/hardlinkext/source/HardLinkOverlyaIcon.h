// HardLinkOverlyaIcon.h : CHardLinkOverlyaIcon 的声明

#pragma once
#include "../resource/resource.h"       // 主符号

#include "../hardlinkext_i.h"
#include <shlobj.h>


#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Windows CE 平台(如不提供完全 DCOM 支持的 Windows Mobile 平台)上无法正确支持单线程 COM 对象。定义 _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA 可强制 ATL 支持创建单线程 COM 对象实现并允许使用其单线程 COM 对象实现。rgs 文件中的线程模型已被设置为“Free”，原因是该模型是非 DCOM Windows CE 平台支持的唯一线程模型。"
#endif


DWORD GetLinkCount(LPCTSTR fn, UINT64 &fid);

// CHardLinkOverlyaIcon

class ATL_NO_VTABLE CHardLinkOverlyaIcon :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CHardLinkOverlyaIcon, &CLSID_HardLinkOverlyaIcon>,
	public IShellIconOverlayIdentifier
{
public:
	CHardLinkOverlyaIcon();


DECLARE_REGISTRY_RESOURCEID(IDR_HARDLINKOVERLYAICON)

DECLARE_NOT_AGGREGATABLE(CHardLinkOverlyaIcon)

BEGIN_COM_MAP(CHardLinkOverlyaIcon)
	COM_INTERFACE_ENTRY(IShellIconOverlayIdentifier)  
END_COM_MAP()



	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
	}

public:
	STDMETHODIMP GetOverlayInfo(LPWSTR pwszIconFile, 
           int cchMax,int *pIndex,DWORD* pdwFlags);
	STDMETHODIMP GetPriority(int* pPriority);
	STDMETHODIMP IsMemberOf(LPCWSTR pwszPath,DWORD dwAttrib);

public:

};

OBJECT_ENTRY_AUTO(__uuidof(HardLinkOverlyaIcon), CHardLinkOverlyaIcon)
