﻿// dllmain.h : 模块类的声明。

#include <vector>
#include <list>

class CHardLinkListDlg;

class ChardlinkextModule : public CAtlDllModuleT< ChardlinkextModule >
{
public :
	DECLARE_LIBID(LIBID_hardlinkextLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_HARDLINKEXT, "{90A3BE99-BD84-48D0-8F19-64A3FF7E049A}")

public:
	void CreateHllDlg(CHardLinkListDlg * &dlg);
	void CleanDialogPool(void);

protected:
	typedef std::list<CHardLinkListDlg *>	DIALOG_POOL;
	DIALOG_POOL m_dialog_pool;
};

extern class ChardlinkextModule _AtlModule;
