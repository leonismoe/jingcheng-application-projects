// dllmain.h : 模块类的声明。

class ChardlinkextModule : public CAtlDllModuleT< ChardlinkextModule >
{
public :
	DECLARE_LIBID(LIBID_hardlinkextLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_HARDLINKEXT, "{90A3BE99-BD84-48D0-8F19-64A3FF7E049A}")
};

extern class ChardlinkextModule _AtlModule;
