// DriverImageForTrace.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include <stdext.h>
//#include "driver_factory.h"
#include "DriverImage.h"
#include "../../Comm/virtual_disk.h"

#include <ntddscsi.h>
#include <WinIoCtl.h>

#include <sstream>

#define STATUS_SUCCESS		0
#define STATUS_INVALID_PARAMETER         (0xC000000DL)    // winnt

LOCAL_LOGGER_ENABLE(_T("CoreMntDriverFile"), LOGGER_LEVEL_ERROR);

///////////////////////////////////////////////////////////////////////////////
// --
CDriverImageFile::CDriverImageFile(const CJCStringT & config)
	: m_ref(1), m_file_secs(0), m_file_mapping(NULL)
	, m_store(0), m_trace_file(NULL)
{
	m_config.LoadFromFile(config);
	m_file_secs = m_config.m_total_sec;

	bool br = OpenStorageFile(m_config.m_storage_file, m_file_secs);
	if (!br) return;

	Initialize();

}

bool CDriverImageFile::OpenStorageFile(const CJCStringT fn, ULONG64 secs)
{
	//
	JCASSERT(m_file_mapping == NULL);
	ULONG64 file_len = SECTOR_TO_BYTEL(secs);
	CreateFileMappingObject(fn, m_file_mapping);
	JCASSERT(m_file_mapping);
	file_len = m_file_mapping->AligneHi(file_len);
	m_file_mapping->SetSize(file_len);

	// open trace file
	m_trace_file = CreateFile(_T("trace.txt"), FILE_ALL_ACCESS, FILE_SHARE_READ | FILE_SHARE_WRITE, 
		NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
	if(INVALID_HANDLE_VALUE == m_trace_file)
	{
		LOG_ERROR(_T("failure on opening file %s."), fn.c_str());
		m_trace_file = NULL;
		return false;
	}

	return true;
}


bool CDriverImageFile::Initialize(void)
{
	// for time stamp calculation
	m_ts_cycle = CJCLogger::Instance()->GetTimeStampCycle() / 1000;		 //ms
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	m_start_ts = now.QuadPart;

	return true;
}

CDriverImageFile::~CDriverImageFile(void)
{
	if (m_file_mapping)	m_file_mapping->Release();
	if (m_trace_file) CloseHandle(m_trace_file);
}

void CDriverImageFile::LogTrace(BYTE cmd, ULONG64 lba, ULONG32 secs)
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	UINT time_stamp = (UINT)( (now.QuadPart - m_start_ts) * m_ts_cycle);
	char str_trace[256];
	// time stamp in ms
	int str_len = sprintf_s(str_trace, ("%d,%d,0,%02X,%08I64X,%d\n"), m_store++, time_stamp, cmd, lba, secs);

	DWORD written = 0;
	WriteFile(m_trace_file, str_trace, str_len, &written, NULL);

}

ULONG32	CDriverImageFile::Read(UCHAR * buf, ULONG64 lba, ULONG32 secs)
{
	LOG_NOTICE(_T("[ATA],RD,%08I64X,%d"), lba, secs);
	JCASSERT(m_file_mapping);
	JCASSERT(m_trace_file);

	// log of the trace
	LogTrace(0xC8, lba, secs);

	// find cache

	// copyt data to buffer
	JCSIZE data_len, offset;
	UCHAR * ptr = MappingFile(lba, secs, data_len, offset);
	memcpy_s(buf, data_len, ptr + offset, data_len);
	m_file_mapping->Unmapping(ptr);

	return STATUS_SUCCESS;
}

ULONG32	CDriverImageFile::Write(const UCHAR * buf, ULONG64 lba, ULONG32 secs)
{
	LOG_NOTICE(_T("[ATA],WR,%08I64X,%d"), lba, secs);

	LogTrace(0xCA, lba, secs);

	// find cache
	JCSIZE data_len, offset;
	UCHAR * ptr = MappingFile(lba, secs, data_len, offset);
	memcpy_s(ptr + offset, data_len, buf, data_len);
	m_file_mapping->Unmapping(ptr);

	return STATUS_SUCCESS;
}

ULONG32	CDriverImageFile::FlushCache(void)
{
	LOG_NOTICE(_T("[ATA],FLUSH"));
	LogTrace(0xE7, 0, 0);

	return STATUS_SUCCESS;
}

ULONG32 CDriverImageFile::ScsiCommand(READ_WRITE rd_wr, UCHAR *buf, JCSIZE buf_len, UCHAR *cb, JCSIZE cb_length, UINT timeout)
{
	ULONG64 lba = 0;
	ULONG32 secs = 0;
	ULONG32 status = STATUS_INVALID_PARAMETER;

	switch (cb[0])
	{
	case 0x00:	//  TEST UNIT READY
		LOG_NOTICE(_T("[SCSI], TEST UNIT READY") )
		status = STATUS_SUCCESS;
		break;

	case 0x12:		{		// INQUERY
		//JCSIZE len = min(buf_len, cb[4]);	// len must < 256
		//LOG_NOTICE(_T("[SCSI],INQUERY, len=%d, buf_len=%d"), len, buf_len);
		//memcpy_s(buf, buf_len, m_buf_inquery, len);
		//LOG_DEBUG(_T("%02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, "),
		//	buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7] );
		status = STATUS_SUCCESS;
		break;		}

	case 0x25:				// get capacity
		LOG_NOTICE(_T("[SCSI],GET CAPACITY, buf len=%d, secs=0x%X"), buf_len, m_file_secs);
		memset(buf, 0, 8);
		buf[3] = m_file_secs & 0xFF;
		buf[2] = (m_file_secs>>8) & 0xFF;
		buf[1] = (m_file_secs>>16) & 0xFF;
		buf[0] = (m_file_secs>>24) & 0xFF;
		LOG_DEBUG(_T("%02X, %02X, %02X, %02X, "), buf[0], buf[1], buf[2], buf[3]);

		buf[6] = 0x02, buf[7] = 0x00;
		status = STATUS_SUCCESS;
		break;

	case 0x28:	// READ (10)
		if (cb_length < 10)
		{
			LOG_ERROR(_T("[ERR],READ(10), cb length too small: %d."), cb_length);
			return STATUS_INVALID_PARAMETER;
		}
		lba = MAKELONG(MAKEWORD(cb[5],cb[4]), MAKEWORD(cb[3],cb[2]) );
		secs = MAKEWORD(cb[8], cb[7]);
		LOG_NOTICE(_T("[SCSI],READ(10),%08I64X,%d"), lba, secs);
		if (secs * SECTOR_SIZE > buf_len)
		{
			LOG_ERROR(_T("[ERR],READ(10), buf size too small: %d."), buf_len);
			return STATUS_INVALID_PARAMETER;
		}
		LOG_DEBUG(_T("read data buf=0x%08X"), buf);

		status = Read(buf, lba, secs);
		break;
		
	case 0x2A: // WRITE (10)
		if (cb_length < 10)
		{
			LOG_ERROR(_T("[ERR],WRITE(10), cb length too small: %d."), cb_length);
			return STATUS_INVALID_PARAMETER;
		}
		lba = MAKELONG(MAKEWORD(cb[5],cb[4]), MAKEWORD(cb[3],cb[2]) );
		secs = MAKEWORD(cb[8], cb[7]);
		LOG_NOTICE(_T("[SCSI],WRITE(10),%08I64X,%d"), lba, secs);
		if (secs * SECTOR_SIZE > buf_len)
		{
			LOG_ERROR(_T("[ERR],WRITe(10), buf size too small: %d."), buf_len);
			return STATUS_INVALID_PARAMETER;
		}

		status = Write(buf, lba, secs);
		break;

	case 0x1B:	// START STOP UNIT
		LOG_NOTICE(_T("[SCSI], START STOP UNIT, %02X, %02X"), cb[4], cb[5] )
		status = STATUS_SUCCESS;
		break;

	default:	{
		WORD cmd_code = MAKEWORD(cb[1],cb[0]);
		if (cmd_code == 0xF083)
		{
			LOG_NOTICE(_T("[SCSI], Vendor:0xF083, buf_len=%d"), buf_len )
			//memcpy_s(buf, buf_len, m_buf_f083, SECTOR_SIZE);
			status = STATUS_SUCCESS;
		}
		else if (cmd_code == 0xF280)
		{
			LOG_NOTICE(_T("[SCSI], VENDOR:Initial Card, buf_len=%d, enable=%d"), buf_len, cb[2] )
			status = STATUS_SUCCESS;
		}
		else if (cmd_code == 0xF800)
		{
			LOG_NOTICE(_T("[SCSI], VENDOR:0xF800, buf_len=%d"), buf_len );
			//if (buf_len > 0)	memcpy_s(buf, buf_len, m_buf_f800, min(buf_len, SECTOR_SIZE));
			status = STATUS_SUCCESS;
		}
		else
		{
			LOG_NOTICE(_T("[SCSI] Unknow vendor command 0x%042X"), cmd_code);
			LOG_DEBUG(_T("Param: %02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X"),
				cb[2], cb[3], cb[4], cb[5], cb[6], cb[7], cb[8], cb[9], cb[10], 
				cb[11], cb[12], cb[13], cb[14], cb[15]);
		}

		//if (cb[0] == 0x1B && cb[1]== 00)
		//{

		//}
		//if (cb[0] >= 0xF0)
		//{	// vendor command
		//	WORD cmd_code = MAKEWORD(cb[1],cb[0]);
		//}
		//else 	LOG_WARNING(_T("[SCSI],unknow command 0x%02X"), cb[0]);
		break;	}
	}

	return status;
}


ULONG32 CDriverImageFile::DeviceControl(ULONG code, READ_WRITE read_write, UCHAR * buf, ULONG32 & data_size, ULONG32 buf_size)
{
	//LOG_NOTICE(_T("[ATA],DeviceControl,code=0x%08X"), code);
	ULONG32 status = STATUS_SUCCESS;
	switch (code)
	{
	case 0x0004D014: {	//IOCTL_SCSI_PASS_THROUGH_DIRECT:
		LOG_NOTICE(_T("[IOCTL] : SCSI_PASS_THROUGH_DIRECT"));
		LOG_DEBUG(_T("SCSI_PASS_THROUGH_DIR size: %d, data size:%d"), sizeof(SCSI_PASS_THROUGH_DIRECT), data_size);
		SCSI_PASS_THROUGH_DIRECT & sptd = *(reinterpret_cast<SCSI_PASS_THROUGH_DIRECT *>(buf));
		LOG_DEBUG(_T("buf = 0x%08X"), buf);
		status = ScsiCommand( (sptd.DataIn == SCSI_IOCTL_DATA_IN)?READ:WRITE,
			buf + sizeof(SCSI_PASS_THROUGH_DIRECT) + sptd.SenseInfoLength,
			sptd.DataTransferLength,
			sptd.Cdb,
			sptd.CdbLength,
			sptd.TimeOutValue);
		break; }

	case 0x0004D004: {	//IOCTL_SCSI_PASS_THROUGH_DIRECT: 
		LOG_NOTICE(_T("[IOCTL] : SCSI_PASS_THROUGH"));
		SCSI_PASS_THROUGH & sptd = *(reinterpret_cast<SCSI_PASS_THROUGH *>(buf));
		LOG_DEBUG(_T("SCSI_PASS_THROUGH, data offset=%d, data len=%d"), 
			sptd.DataBufferOffset, sptd.DataTransferLength);
		status = ScsiCommand( (sptd.DataIn == SCSI_IOCTL_DATA_IN)?READ:WRITE,
			buf + sptd.DataBufferOffset,
			sptd.DataTransferLength,
			sptd.Cdb,
			sptd.CdbLength,
			sptd.TimeOutValue);
		break;		 }

	case 0x002D1400:	{ //IOCTL_STORAGE_QUERY_PROPERTY
		STORAGE_PROPERTY_QUERY * spq = reinterpret_cast<STORAGE_PROPERTY_QUERY*>(buf);
		LOG_NOTICE(_T("[IOCTL],STORAGE_PROPERTY_QUERY,id:%d,type:%d"), spq->PropertyId, spq->QueryType)
		break; }

	default:
		LOG_ERROR(_T("[IOCTL] Unknown DeviceIoControl Code 0x%08X"), code);
		break;
	}
	return status;
}

void CDriverImageFile::SetStatus(const CJCStringT & status, jcparam::IValue * param_set)
{
	LOG_STACK_TRACE();
}

void CDriverImageFile::SendEvent(void)
{
	LOG_STACK_TRACE();
}



