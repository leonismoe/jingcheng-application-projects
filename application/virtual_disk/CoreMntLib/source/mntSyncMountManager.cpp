#include "stdafx.h"
LOCAL_LOGGER_ENABLE(_T("SyncMntManager"), LOGGER_LEVEL_DEBUGINFO);

#include "../include/mntSyncMountmanager.h"
#include "process.h"
#include <sstream>
#include <iostream>

#include "../../Comm/virtual_disk.h"


///////////////////////////////////////////////////////////////////////////////
// -- 
CSyncMountManager::CSyncMountManager(void)
: m_driver_module(NULL)
{
}

CSyncMountManager::~CSyncMountManager(void)
{
	if (m_driver_module)	FreeLibrary(m_driver_module);
}


UINT CSyncMountManager::CreateDevice(ULONG64 total_sec, const CJCStringT & symbo_link)
{
	CDriverControl * drv_ctrl = new CDriverControl(total_sec, symbo_link);	// length in sectors
	UINT dev_id = drv_ctrl->GetDeviceId();
	m_driver_map.insert(DRIVER_MAP_PAIR(dev_id, drv_ctrl) );
	return dev_id;
}

void CSyncMountManager::Connect(UINT dev_id, IImage * image)
{
	DRIVER_MAP_IT it = m_driver_map.find(dev_id);
	if (it == m_driver_map.end() ) THROW_ERROR(ERR_PARAMETER, _T("dev id %d do not exist"), dev_id);
	CDriverControl * drv_ctrl = it->second;
	drv_ctrl->Connect(image);
}

void CSyncMountManager::MountDriver(UINT dev_id, TCHAR mount_point)
{
	DRIVER_MAP_IT it = m_driver_map.find(dev_id);
	if (it == m_driver_map.end() ) THROW_ERROR(ERR_PARAMETER, _T("dev id %d do not exist"), dev_id);
	CDriverControl * drv_ctrl = it->second;
	drv_ctrl->Mount(mount_point);
}

void CSyncMountManager::Disconnect(UINT dev_id)
{
	DRIVER_MAP_IT it = m_driver_map.find(dev_id);
	if (it == m_driver_map.end() ) THROW_ERROR(ERR_PARAMETER, _T("dev id %d do not exist"), dev_id);
	CDriverControl * drv_ctrl = it->second;
	drv_ctrl->Disconnect();

	m_driver_map.erase(it->first);
	delete drv_ctrl;
}

void CSyncMountManager::UnmountImage(UINT dev_id)
{
	DRIVER_MAP_IT it = m_driver_map.find(dev_id);
	if (it == m_driver_map.end() ) THROW_ERROR(ERR_PARAMETER, _T("dev id %d do not exist"), dev_id);
	CDriverControl * drv_ctrl = it->second;
	drv_ctrl->Unmount();
}

template <> void stdext::CCloseHandle<SC_HANDLE>::DoCloseHandle(SC_HANDLE hdl)
{
	if (hdl) CloseServiceHandle(hdl);
}

void CSyncMountManager::InstallDriver(const CJCStringT & driver_fn)
{
	LOG_STACK_TRACE();
	LOG_DEBUG(_T("driver = %s"), driver_fn.c_str());

	stdext::auto_handle<SC_HANDLE> scmng(OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
	if ((SC_HANDLE)scmng == NULL) THROW_WIN32_ERROR(_T(" openning scm failed!"));

	// try to open service
	SC_HANDLE _srv = NULL;
	_srv = OpenService(scmng, _T("CoreMnt"), SERVICE_ALL_ACCESS);
	if ( (_srv == NULL) && (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) )
	{	// try to create service
		_srv = CreateService(scmng, _T("CoreMnt"), _T("CoreMnt"), SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
				SERVICE_DEMAND_START, SERVICE_ERROR_CRITICAL, driver_fn.c_str(), NULL, NULL, NULL, NULL, NULL);
	}
	if (_srv == NULL) THROW_WIN32_ERROR(_T("creating service failed!"));
	stdext::auto_handle<SC_HANDLE> srv(_srv);

/*
	SC_HANDLE srv = CreateService(scmng, _T("CoreMnt"), _T("CoreMnt"), SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
		SERVICE_DEMAND_START, SERVICE_ERROR_CRITICAL, driver_fn.c_str(), NULL, NULL, NULL, NULL, NULL);
	if (srv == NULL)
	{
		DWORD ir = GetLastError();
		LOG_DEBUG(_T("create service failed, code=%d"), ir);
		if ( (ir == ERROR_IO_PENDING) || (ir == ERROR_SERVICE_EXISTS) )
		{
			LOG_DEBUG(_T("service is existing, retry for open"));
			srv = OpenService(scmng, _T("CoreMnt"), SERVICE_ALL_ACCESS);
		}
		if (srv == NULL)	THROW_WIN32_ERROR(_T("creating service failed!"));
	}
*/
	BOOL br = StartService(srv, NULL, NULL);
	if (!br)
	{
		LOG_DEBUG(_T("start service failed"));
		DWORD ir = GetLastError();
		if (ir == ERROR_SERVICE_ALREADY_RUNNING)
		{
			LOG_DEBUG(_T("service is running."))
		}
		else THROW_WIN32_ERROR(_T("start service failed"))
	}

	//SC_HANDLE driver = OpenService(scmng, _T("CoreMnt"), 
	//ControlService(srv, SERVICE_CONTROL_STOP, NULL);

	//CloseServiceHandle(srv);
	//CloseServiceHandle(scmng);
}

void CSyncMountManager::UninstallDriver(void)
{
	LOG_STACK_TRACE();

	stdext::auto_handle<SC_HANDLE> scmng(OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
	if ((SC_HANDLE)scmng == NULL) THROW_WIN32_ERROR(_T(" openning scm failed!"));
	
	// open service
	stdext::auto_handle<SC_HANDLE> srv(OpenService(scmng, _T("CoreMnt"), SERVICE_ALL_ACCESS));
	if ( (SC_HANDLE)srv == NULL) THROW_WIN32_ERROR(_T("open service failed!"));

	// stop service
	SERVICE_STATUS status;
	BOOL br = ControlService(srv, SERVICE_CONTROL_STOP, &status);
	if (!br) LOG_DEBUG(_T("stop service failed, code=%d"), GetLastError() );
	LOG_DEBUG(_T("stop service, current status=%d"), status.dwCurrentState );
	
	// delete service
	br = DeleteService(srv);
	if (!br) LOG_DEBUG(_T("delete service failed, code=%d"), GetLastError() );
}

//bool CSyncMountManager::LoadUserModeDriver(const CJCStringT & drv_path, const CJCStringT & drv_name, jcparam::IValue * param, IImage * & img/*, HMODULE & module*/)
bool CSyncMountManager::LoadUserModeDriver(const CJCStringT & drv_path, const CJCStringT & drv_name, const CJCStringT & config, IImage * & img)
{
	JCASSERT(NULL == img);
	JCASSERT(NULL == m_driver_module);

	m_driver_module = LoadLibrary(drv_path.c_str());
	if (m_driver_module == NULL) THROW_WIN32_ERROR(_T(" failure on loading driver %s "), drv_path.c_str() );

	// load entry
	GET_DRV_FACT_PROC proc = (GET_DRV_FACT_PROC) (GetProcAddress(m_driver_module, "GetDriverFactory") );
	if (proc == NULL)	THROW_WIN32_ERROR(_T("file %s is not a virtual disk driver."), drv_path.c_str() );

	stdext::auto_interface<IDriverFactory> factory;
	BOOL br = (proc)(factory);
	if (!br) THROW_ERROR(ERR_APP, _T("failure on getting factory."));
	JCASSERT( factory.valid() );

	factory->CreateDriver(drv_name, config, img);
	JCASSERT(img);
	return true;
}

void CSyncMountManager::UnloadDriver(void)
{
	if (m_driver_module)
	{
		FreeLibrary(m_driver_module);
		m_driver_module = NULL;
	}
}
