// HardLinkList.h : CHardLinkList 的声明

#pragma once

#include <Shobjidl.h>

#include "../resource/resource.h"       // 主符号
#include "../hardlinkext_i.h"



#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Windows CE 平台(如不提供完全 DCOM 支持的 Windows Mobile 平台)上无法正确支持单线程 COM 对象。定义 _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA 可强制 ATL 支持创建单线程 COM 对象实现并允许使用其单线程 COM 对象实现。rgs 文件中的线程模型已被设置为“Free”，原因是该模型是非 DCOM Windows CE 平台支持的唯一线程模型。"
#endif



// CHardLinkList

class ATL_NO_VTABLE CHardLinkList :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CHardLinkList, &CLSID_HardLinkList>,
	public IShellExtInit,
	public IContextMenu
{
public:
	CHardLinkList();
	~CHardLinkList();


DECLARE_REGISTRY_RESOURCEID(IDR_HARDLINKLIST)

DECLARE_NOT_AGGREGATABLE(CHardLinkList)

BEGIN_COM_MAP(CHardLinkList)
	COM_INTERFACE_ENTRY(IShellExtInit)
	COM_INTERFACE_ENTRY(IContextMenu)
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
	// IShellExtInit
	STDMETHODIMP Initialize(LPCITEMIDLIST, LPDATAOBJECT, HKEY);

	// IContextMenu
	STDMETHODIMP GetCommandString (UINT, UINT, UINT*, LPSTR, UINT);
	STDMETHODIMP InvokeCommand (LPCMINVOKECOMMANDINFO);
	STDMETHODIMP QueryContextMenu (HMENU, UINT, UINT, UINT, UINT);

protected:
	HRESULT ShowFileList(LPCMINVOKECOMMANDINFO cmd_info);
	HRESULT DeLink();

protected:
	TCHAR m_file_name[MAX_PATH];
	UINT64 m_file_id;
};

OBJECT_ENTRY_AUTO(__uuidof(HardLinkList), CHardLinkList)
