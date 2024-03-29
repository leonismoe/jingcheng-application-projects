﻿
#include "CoreMntTest.h"
#include <vld.h>

LOCAL_LOGGER_ENABLE(_T("CoreMntTest"), LOGGER_LEVEL_DEBUGINFO);



#define _STATIC_CPPLIB
#define BLOCK_LENGTH         0x1000

LOG_CLASS_SIZE(CORE_MNT_EXCHANGE_REQUEST)
LOG_CLASS_SIZE(CJCLoggerNode)

const TCHAR CCoreMntTestApp::LOG_CONFIG_FN[] = _T("jclog.cfg");
CApplication _app;

#define _class_name_	CApplication
BEGIN_ARGU_DEF_TABLE()
	ARGU_DEF_ITEM(_T("driver"),		_T('p'), CJCStringT,	m_driver,		_T("load user mode driver.") )
	ARGU_DEF_ITEM(_T("mount"),		_T('m'), TCHAR,			m_mnt_point,	_T("mount point.") )
	ARGU_DEF_ITEM(_T("deviceid"),	_T('d'), UINT,			m_dev_id,		_T("deviceid.") )
	ARGU_DEF_ITEM(_T("filesize"),	_T('s'), FILESIZE,		m_file_size,	_T("image file size in sectors.") )
	ARGU_DEF_ITEM(_T("filename"),	_T('f'), CJCStringT,	m_filename,		_T("image file name.") )
	ARGU_DEF_ITEM(_T("connect"),	_T('c'), bool,			m_connect,		_T("connect user mode driver to device.") )
	ARGU_DEF_ITEM(_T("remove"),		_T('r'), UINT,			m_remove,		_T("remove dead device.") )
	ARGU_DEF_ITEM(_T("dev_name"),	_T('n'), CJCStringT,	m_device_name,	_T("symbo link of device.") )
	ARGU_DEF_ITEM(_T("config"),		_T('g'), CJCStringT,	m_config,		_T("configuration file name for driver.") )
	ARGU_DEF_ITEM(_T("install"),	_T('i'), bool,			m_install_driver,		_T("install driver") )
	ARGU_DEF_ITEM(_T("uninstall"),	_T('u'), bool,			m_uninstall_driver,		_T("uninstall driver") )
END_ARGU_DEF_TABLE()

BOOL WINAPI HandlerRoutine(DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		LOG_DEBUG(_T("Ctrl + C has been pressed."));
		SetEvent(CApplication::Instance()->m_exit_event);
		break;
	default:
		LOG_DEBUG(_T("console event 0x%X"), dwCtrlType);
	}
	return TRUE;
}

//#define LOCAL_DEBUG

CCoreMntTestApp::CCoreMntTestApp(void) 
	: jcapp::CJCAppSupport<jcapp::AppArguSupport>(ARGU_SUPPORT_HELP | ARGU_SUPPORT_OUTFILE)
	, m_dev_id(UINT_MAX), m_file_size(0x100000LL), m_mnt_point(0)
	, m_exit_event(NULL), m_connect(false), m_remove(UINT_MAX)
	, m_install_driver(false), m_uninstall_driver(false)
	, m_test_thread(NULL), m_test_port(NULL)
{
	m_status = ST_INIT;
}

void CCoreMntTestApp::InstallDriver(CSyncMountManager & mntmng,const CJCStringT & driver)
{
	CJCStringT	wdm_path;
	SYSTEM_INFO si; 
	GetNativeSystemInfo(&si); 
	if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64)	THROW_ERROR(ERR_APP, _T("do not support ia64"))
	else if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
	{    //64 位操作系统 
		LOG_DEBUG(_T("plarform = x64"));
		wdm_path = m_app_path + _T("\\") WDM_FN _T("64.sys");
	}
	else
	{
		LOG_DEBUG(_T("plarform = x86"));
		wdm_path = m_app_path + _T("\\") WDM_FN _T(".sys");    // 32 位操作系统 
	}
	_tprintf(_T("Installing driver %s ..."), wdm_path.c_str());
	mntmng.InstallDriver(wdm_path);

	m_status = ST_DRV_INSTALLED;
	_tprintf(_T("Succeeded. \n"));
}

void CCoreMntTestApp::LoadUserModeDriver(const CJCStringT & driver, IImage * & img)
{
	CJCStringT drv_path;
	drv_path = m_app_path + _T("\\") + driver + _T(".dll");
	_tprintf(_T("Loading user driver %s ...\n"), drv_path.c_str() );

	// create parameters
	//stdext::auto_interface<jcparam::CParamSet> param;
	//CreateParameter(param);
	m_mount_manager.LoadUserModeDriver(drv_path, _T("vendor_test"), m_config, img);

	JCASSERT(img);
	m_status = ST_DRV_LOADED;
	_tprintf(_T("Succeded\n"));
}

//void CCoreMntTestApp::StartTestPort(ITestAuditPort * test_port)
//{
//	DWORD tid = 0;
//	m_test_thread = CreateThread(NULL, 0, StaticStartTestPort, (LPVOID)(test_port), 0, &tid);
//}



int CCoreMntTestApp::Run(void)
{
	LOG_STACK_TRACE();
	SetConsoleCtrlHandler(HandlerRoutine, TRUE);

	// get app dir
	__super::GetAppPath(m_app_path);

	// install / uninstall driver	
	if (m_install_driver)			InstallDriver(m_mount_manager, _T(""));
	else if (m_uninstall_driver)
	{
		_tprintf(_T("Uninstalling driver..."));
		m_mount_manager.UninstallDriver();
		_tprintf(_T("Succeded\n"));
		m_status = ST_INIT;
		return 0;
	}

	if (m_remove < UINT_MAX)
	{
		CDriverControl * driver = new CDriverControl(m_remove, true);
		driver->Disconnect();
		delete driver;
		return 0;
	}

	stdext::auto_interface<IImage>	img;
	// load driver
	if (m_driver.empty() )	return 0;

	LoadUserModeDriver(m_driver, img);
	JCASSERT( img.valid() );
	m_file_size = img->GetSize();

	m_test_port = img.d_cast<ITestAuditPort*>();
	if (m_test_port)
	{
		m_test_port->AddRef();
		DWORD tid = 0;
		m_test_thread = CreateThread(NULL, 0, StaticStartTestPort, (LPVOID)(this), 0, &tid);

		//StartTestPort(test_port);
		//test_port->SendEvent();
		//test_port->Release();
	}

#ifndef LOCAL_DEBUG

	if (m_device_name.empty() ) m_device_name = _T("disk0");
	CJCStringT symbo_link = CJCStringT(_T("")) + _T("\\DosDevices\\") + m_device_name;
	m_dev_id = m_mount_manager.CreateDevice(m_file_size, symbo_link);		// length in sectors;
	_tprintf( _T("device: %s was created.\n"), symbo_link.c_str());

	if (m_connect)
	{
		_tprintf(_T("Connecting..."));
		LOG_DEBUG(_T("connect to device %d"), m_dev_id);
		if (!img.valid())	THROW_ERROR(ERR_USER, _T("User mode driver is not loaded."));
		m_mount_manager.Connect(m_dev_id, img);
		_tprintf(_T("Succeeded\n"));
		m_status = ST_DRV_CONNECTED;
	}

	if (m_mnt_point)
	{
		_tprintf(_T("Mounting device to volumn %C: ..."), m_mnt_point);
		LOG_DEBUG(_T("mount device %d to %c:"), m_dev_id, m_mnt_point);
		m_mount_manager.MountDriver(m_dev_id, m_mnt_point);
		_tprintf(_T("Succeeded\n"));
		m_status = ST_MOUNTED;
	}
#endif

	if (m_status >= ST_DRV_CONNECTED)
	{
		printf("Press ctrl+c for disconnect...\n");
		WaitForSingleObject(m_exit_event, -1);
	}
	return 0;
}

void CCoreMntTestApp::CreateParameter(jcparam::CParamSet * &param)
{
	JCASSERT(param == NULL);
	jcparam::CParamSet::Create(param);

	jcparam::IValue * val = NULL;

	val = jcparam::CTypedValue<CJCStringT>::Create(m_filename);
	param->SetSubValue(_T("FILENAME"), val);
	val->Release(); val = NULL;

	val = jcparam::CTypedValue<ULONG64>::Create(m_file_size);
	param->SetSubValue(_T("FILESIZE"), val);
	val->Release(); val = NULL;

	val = jcparam::CTypedValue<CJCStringT>::Create(m_config);
	param->SetSubValue(_T("CONFIG"), val);
	val->Release(); val = NULL;
}


int CCoreMntTestApp::Initialize(void)
{
	m_exit_event = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (m_exit_event == NULL) THROW_WIN32_ERROR(_T("failure on creating event."));
	return 1;
}

void CCoreMntTestApp::CleanUp(void)
{
	if (m_status >= ST_MOUNTED)		
	{
		_tprintf(_T("Unmount volumn... "));
		m_mount_manager.UnmountImage(m_dev_id);
		_tprintf(_T("Succeeded\n"));
		m_status = ST_DRV_CONNECTED;
	}

	if (m_status >= ST_DRV_CONNECTED)	
	{
		_tprintf(_T("Disconnect... "));
		m_mount_manager.Disconnect(m_dev_id);
		_tprintf(_T("Succeeded\n"));
		m_status = ST_DRV_LOADED;
	}
	if (m_test_thread) CloseHandle(m_test_thread);

	if ( (m_status >= ST_DRV_LOADED) /*&& m_driver_module*/)
	{
		//FreeLibrary(m_driver_module);
		m_mount_manager.UnloadDriver();
		m_status = ST_DRV_INSTALLED;
	}

	CloseHandle(m_exit_event);
	__super::CleanUp();
}

int _tmain(int argc, _TCHAR* argv[])
{
	return jcapp::local_main(argc, argv);
}

