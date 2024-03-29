#include "stdafx.h"

LOCAL_LOGGER_ENABLE(_T("SyncMntManager"), LOGGER_LEVEL_DEBUGINFO);

#include "shlobj.h"

#include "../include/mntDriverControl.h"
#include "../../Comm/virtual_disk.h"

#define IRP_MJ_READ                     0x03
#define IRP_MJ_WRITE                    0x04
#define IRP_MJ_QUERY_INFORMATION        0x05
#define IRP_MJ_DEVICE_CONTROL           0x0e
#define IRP_MJ_FLUSH_BUFFERS            0x09

#define STATUS_INVALID_DEVICE_REQUEST    ((NTSTATUS)0xC0000010L)


#include <setupapi.h>

static const GUID COREMNT_GUID = COREMNT_CLASS_GUID;

void SearchForDevice(CJCStringT & symbo)
{
	LOG_STACK_TRACE();
	const GUID * guid = &COREMNT_GUID;
	HDEVINFO info = NULL;
	PSP_INTERFACE_DEVICE_DETAIL_DATA ifdetail = NULL;

	try
	{
		info = SetupDiGetClassDevs(guid, NULL, NULL, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
		if (info == INVALID_HANDLE_VALUE) THROW_WIN32_ERROR(_T("failure on get class device"));

		SP_INTERFACE_DEVICE_DATA ifdata;
		ifdata.cbSize = sizeof (ifdata);
		int ii=0;
		BOOL br = FALSE;
		for (ii=0; ii < 10; ++ii)
		{
			br = SetupDiEnumDeviceInterfaces(info, NULL, guid, ii, &ifdata);
			if (br) break;
		}
		if (ii >= 10) THROW_WIN32_ERROR(_T("failure on enum device if"));

		LOG_DEBUG(_T("got device %d"), ii);
		DWORD reg_len;
		br = SetupDiGetDeviceInterfaceDetail(info, &ifdata, NULL, 0, &reg_len, NULL);
		ifdetail = (PSP_INTERFACE_DEVICE_DETAIL_DATA) (new char[reg_len]);
		JCASSERT(ifdetail);

		ifdetail->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);
		br = SetupDiGetDeviceInterfaceDetail(info, &ifdata, ifdetail, reg_len, NULL, NULL);
		if (!br) THROW_WIN32_ERROR(_T("failure on getting detail"));

		symbo = ifdetail->DevicePath;
		LOG_DEBUG(_T("symbo link = %s"), symbo.c_str());
	}
	catch (stdext::CJCException & err)
	{
		throw;
	}
	delete [] (char*)(ifdetail);
	SetupDiDestroyDeviceInfoList(info);
}



///////////////////////////////////////////////////////////////////////////////
// -- 
CDriverControl::CDriverControl(ULONG64 total_sec, const CJCStringT & disk_symbo_link)		// length in sectors
	: m_ctrl(NULL), m_image(NULL), m_dev_id(UINT_MAX)
	, m_thd(NULL), m_mount_point(0), m_thd_event(NULL)
{
	LOG_STACK_TRACE();

	CJCStringT mnt_symbo_link;
#ifdef COREMNT_PNP_SUPPORT
	SearchForDevice(mnt_symbo_link);
#else
	mnt_symbo_link = COREMNT_USER_NAME;
#endif
	//LOG_DEBUG(_T("core mnt symbol link=%s"), mnt_symbo_link.c_str());
	m_ctrl = CreateFile(mnt_symbo_link.c_str(), GENERIC_READ | GENERIC_WRITE, 
            FILE_SHARE_READ | FILE_SHARE_WRITE,    NULL, OPEN_EXISTING, 0, NULL);

	LOG_DEBUG(_T("open device: %s, handle: 0x%08X"), mnt_symbo_link.c_str(), m_ctrl);
	if (m_ctrl == INVALID_HANDLE_VALUE) THROW_WIN32_ERROR(_T("failure on open device %s"), mnt_symbo_link.c_str());

	// create device in driver
	JCSIZE size = sizeof(CORE_MNT_MOUNT_REQUEST) + disk_symbo_link.length() * sizeof (TCHAR);
	LOG_DEBUG(_T("symbolink len = %d, request len = %d, total len = %d"), 
		sizeof(CORE_MNT_MOUNT_REQUEST), disk_symbo_link.length(), size);
	stdext::auto_array<UCHAR> _req(size);
	memset(_req, 0, size);
	CORE_MNT_MOUNT_REQUEST * request = reinterpret_cast<CORE_MNT_MOUNT_REQUEST *>((void*)(_req));
	request->total_sec = total_sec;				// length in sectors
	request->dev_id = -1;
	_tcscpy_s(request->symbo_link, disk_symbo_link.length() + 1, disk_symbo_link.c_str() );
	DWORD written = 0;

	BOOL br = DeviceIoControl(m_ctrl, CORE_MNT_MOUNT_IOCTL, request, size,
		request, sizeof(CORE_MNT_MOUNT_REQUEST), &written, NULL);
	if (!br) THROW_WIN32_ERROR(_T("failure on create device"));

	m_dev_id = request->dev_id;
	JCASSERT(m_dev_id < MAX_MOUNTED_DISK);
	m_thd_event = CreateEvent(NULL, FALSE, FALSE, NULL);
}

CDriverControl::CDriverControl(UINT dev_id, bool dummy)
	: m_ctrl(NULL), m_image(NULL), m_dev_id(dev_id)
	, m_thd(NULL), m_mount_point(0), m_thd_event(NULL)
{
	LOG_STACK_TRACE();
	CJCStringT mnt_symbo_link;
#ifdef COREMNT_PNP_SUPPORT
	SearchForDevice(mnt_symbo_link);
#else
	mnt_symbo_link = COREMNT_USER_NAME;
#endif
	//LOG_DEBUG(_T("core mnt symbol link=%s"), mnt_symbo_link.c_str());
	m_ctrl = CreateFile(mnt_symbo_link.c_str(), GENERIC_READ | GENERIC_WRITE, 
            FILE_SHARE_READ | FILE_SHARE_WRITE,    NULL, OPEN_EXISTING, 0, NULL);
	LOG_DEBUG(_T("open device: %s, handle: 0x%08X"), mnt_symbo_link.c_str(), m_ctrl);
	if (m_ctrl == INVALID_HANDLE_VALUE) THROW_WIN32_ERROR(_T("failure on open device %s"), mnt_symbo_link.c_str());
}


CDriverControl::~CDriverControl(void)
{
	LOG_STACK_TRACE();

	if (m_ctrl)
	{	// unmount for driver
		CORE_MNT_COMM request;
		request.dev_id = m_dev_id;
		DWORD written = 0;
		BOOL br = DeviceIoControl(m_ctrl, CORE_MNT_UNMOUNT_IOCTL, 
			&request, sizeof(request), NULL, 0, &written, NULL);
		if (!br) LOG_ERROR(_T("failure on unmounting disk device"));
	}

	CloseHandle(m_ctrl);
	if (m_image) m_image->Release();
	if (m_thd) CloseHandle(m_thd);
	if (m_thd_event)	CloseHandle(m_thd_event);
}

void CDriverControl::Connect(IImage * image)
{
	LOG_STACK_TRACE();
	JCASSERT(m_image == NULL);
	JCASSERT(m_ctrl);

	m_image = image;
	JCASSERT(m_image);
	m_image->AddRef();

	m_thd = CreateThread(NULL, 0, StaticRun, (LPVOID)(this), 0, NULL); 
	if (!m_thd) THROW_WIN32_ERROR(_T("failure on creating exchange thread."));
	DWORD ir = WaitForSingleObject(m_thd_event, 10000);
	if (ir == WAIT_TIMEOUT)	{ THROW_ERROR(ERR_APP, _T("creating thread time out."));}
}

void CDriverControl::Mount(TCHAR mnt_point)
{
	LOG_STACK_TRACE();

	JCASSERT(m_image);
	m_mount_point = mnt_point;

	// define logical drive
	CJCStringT	volume_name = CJCStringT(_T("")) + mnt_point + _T(":");
	LOG_DEBUG(_T("volume name = %s"), volume_name.c_str());

	CJCStringT device_name = DIRECT_DISK_PREFIX;
	device_name += (_T('0') + m_dev_id);
	LOG_DEBUG(_T("device_name = %s"), device_name.c_str() );

	BOOL br = DefineDosDevice(DDD_RAW_TARGET_PATH, volume_name.c_str(), device_name.c_str() );
	if (!br) THROW_WIN32_ERROR(_T(" failure on define dos device, vol:%s, dev:%s "),
		volume_name, device_name);

	volume_name += _T("\\");
	LOG_DEBUG(_T("mount root: %s"), volume_name.c_str());

	SHChangeNotify(SHCNE_DRIVEADD, SHCNF_PATH, volume_name.c_str(), NULL);
}

void CDriverControl::Disconnect(void)
{
	LOG_STACK_TRACE();
	JCASSERT(m_ctrl);

	CORE_MNT_COMM	request;
	request.dev_id = m_dev_id;
	DWORD written = 0;
	BOOL br = DeviceIoControl(m_ctrl, CORE_MNT_DISCONNECT_IOCTL, 
		&request, sizeof(request), NULL, 0, &written, NULL);
	if (!br) THROW_WIN32_ERROR(_T(" failure on disconnect "));

	if (m_thd)
	{
		DWORD ir = WaitForSingleObject(m_thd, 60000);	// 1 min
		if (ir == WAIT_TIMEOUT)	{LOG_ERROR(_T("waiting disconnect time out."));}
		CloseHandle(m_thd);
		m_thd = NULL;
	}
}

void CDriverControl::Unmount(void)
{
	LOG_STACK_TRACE();
	if (m_mount_point == 0) return;

	CJCStringT volume = CJCStringT( _T("\\\\.\\") ) + m_mount_point + _T(":");
	LOG_DEBUG(_T("volume = %s"), volume.c_str());
	DWORD written = 0;

	HANDLE hvol = NULL;
	BOOL br = FALSE;
	try
	{
		//LOG_DEBUG(_T("opening logic device..."));
		//hvol = CreateFile(volume.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
		//		NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
		//if (hvol == INVALID_HANDLE_VALUE)	THROW_WIN32_ERROR(_T("Unable to open logical drive: %s"), volume.c_str() );
		//LOG_DEBUG(_T("dis-mounting logic device..."));
		//BOOL br = DeviceIoControl(hvol, FSCTL_DISMOUNT_VOLUME, NULL,0,NULL,0,&written,NULL);
		//if (!br) THROW_WIN32_ERROR(_T("failure on dismount logical driver"));
		
		volume = CJCStringT( _T("") ) + m_mount_point + _T(":");
		br = DefineDosDevice(DDD_REMOVE_DEFINITION, volume.c_str() ,NULL);
		if (!br) THROW_WIN32_ERROR(_T("Unable to undefine logical drive"));

		volume += _T("\\");
		SHChangeNotify(SHCNE_DRIVEREMOVED, SHCNF_PATH, volume.c_str(), NULL);
	}
	catch (stdext::CJCException &err)
	{
		LOG_DEBUG(_T("failed in unmount."));
	}
	CloseHandle(hvol);
}



DWORD WINAPI CDriverControl::StaticRun(LPVOID param)
{
	JCASSERT(param);
	CDriverControl * drv_ctrl = (CDriverControl*)param;
	return drv_ctrl->Run();
}

DWORD CDriverControl::Run(void)
{
	LOG_STACK_TRACE();
	DWORD ir=0;
	UCHAR * buf = NULL;

	HANDLE exchange_dev = NULL;
	try
	{
		CJCStringT mnt_symbo_link;
#ifdef COREMNT_PNP_SUPPORT
		SearchForDevice(mnt_symbo_link);
#else
		mnt_symbo_link = COREMNT_USER_NAME;
#endif
		LOG_DEBUG(_T("core mnt symbol link=%s"), mnt_symbo_link.c_str());

		exchange_dev = CreateFile(mnt_symbo_link.c_str(), GENERIC_READ | GENERIC_WRITE, 
            FILE_SHARE_READ | FILE_SHARE_WRITE,    NULL, OPEN_EXISTING, 0, NULL);
		LOG_DEBUG(_T("open core mnt dev: name %s, handle: 0x%08X"), mnt_symbo_link.c_str(), exchange_dev);
		if (exchange_dev == INVALID_HANDLE_VALUE) THROW_WIN32_ERROR(_T("failure on opening core mnt (%s)"), mnt_symbo_link.c_str());
		
		//buf = new UCHAR[EXCHANGE_BUFFER_SIZE];
		buf = (UCHAR*)(VirtualAlloc(
			NULL, EXCHANGE_BUFFER_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
		if (!buf) THROW_WIN32_ERROR(_T("failure on allocating buffer."));

		CORE_MNT_EXCHANGE_REQUEST request;

		request.dev_id = m_dev_id;
		request.m_major_func = IRP_MJ_NOP;
		request.m_minor_code = 0;
		request.m_read_write = 0;

		request.lastStatus = 0;
		request.proc_size = 0;
		request.m_buf = (ULONG64)(buf);
		request.buf_size = EXCHANGE_BUFFER_SIZE;
		LOG_DEBUG(_T("user mode buffer size: %dKB"), EXCHANGE_BUFFER_SIZE / 1024);

		SetEvent(m_thd_event);
		while (true)
		{
			CORE_MNT_EXCHANGE_RESPONSE response;
			response.m_major_func = IRP_MJ_NOP;
			response.m_minor_code = 0;
			response.m_read_write = 0;

			response.size = 0;
			response.offset = 0;

			DWORD written = 0;
			BOOL br = DeviceIoControl(exchange_dev, CORE_MNT_EXCHANGE_IOCTL,
						&request, sizeof(request), &response, sizeof(response), 
						&written, NULL);
			if (!br) THROW_WIN32_ERROR(_T("send exchange request failed."));

			if ( response.m_major_func == IRP_MJ_DISCONNECT)
			{
				LOG_DEBUG(_T("disconnect reflected."));
				break;
			}

			ULONG64 offset = response.offset / SECTOR_SIZE;		// byte to sector
			ULONG32 size = response.size / SECTOR_SIZE;			// byte to sector

			ULONG32 status = STATUS_INVALID_DEVICE_REQUEST;
			switch (response.m_major_func)
			{
			case IRP_MJ_READ:
				status = m_image->Read(buf, offset, size);
				request.proc_size = response.size;
				break;

			case IRP_MJ_WRITE:
				status = m_image->Write(buf, offset, size);
				request.proc_size = response.size;
				break;

			case IRP_MJ_FLUSH_BUFFERS:
				break;

			case IRP_MJ_DEVICE_CONTROL:	{
				ULONG32 data_size = response.size;
				status = m_image->DeviceControl(response.m_minor_code, (READ_WRITE)(response.m_read_write), buf, data_size, EXCHANGE_BUFFER_SIZE);
				request.proc_size = data_size;
				break;						}

			case IRP_MJ_QUERY_INFORMATION:
				break;

			default:
				LOG_DEBUG(_T("unknow major func: %d"), response.m_major_func);
				break;
			}

			request.m_major_func = response.m_major_func;
			request.m_minor_code = response.m_minor_code;
			request.m_read_write = response.m_read_write;
			request.lastStatus = status;
		}
	}
	catch (stdext::CJCException & err)
	{
		stdext::jc_fprintf(stderr, _T("error on exchanging: %s\n"), err.WhatT() );
		ir = err.GetErrorID();
	}

	if (buf)
	{
		VirtualFree(buf, EXCHANGE_BUFFER_SIZE, MEM_DECOMMIT | MEM_RELEASE);
	}
	//delete [] buf;
	CloseHandle(exchange_dev);
	return ir;
}





