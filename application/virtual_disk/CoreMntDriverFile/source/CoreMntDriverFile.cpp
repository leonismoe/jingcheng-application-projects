// CoreMntDriverFile.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include <stdext.h>
#include "config.h"
//#include "driver_factory.h"
#include "DriverImageFile.h"
#include "../../Comm/virtual_disk.h"


#include <ntddscsi.h>
#include <WinIoCtl.h>

#include <sstream>


#include "idtable_2244lt.h"

//#include <jcapp>


LOCAL_LOGGER_ENABLE(_T("CoreMntDriverFile"), LOGGER_LEVEL_ERROR);


///////////////////////////////////////////////////////////////////////////////
// --

CDriverImageFile::CDriverImageFile(const CJCStringT & file_name, ULONG64 secs) 
	: m_ref(1), m_file(NULL), m_file_secs(secs)
	, m_vendorcmd_st(VDS0)
{
	LOG_STACK_TRACE();

	m_file = OpenStorageFile(file_name, secs);
	Initialize();
	printf("\n");
}

CDriverImageFile::CDriverImageFile(const CJCStringT & config)
	: m_ref(1), m_file(NULL), m_file_secs(0)
	, m_vendorcmd_st(VDS0)
{
	m_config.LoadFromFile(config);
	m_file = OpenStorageFile(m_config.m_storage_file, m_config.m_total_sec);
	m_file_secs = m_config.m_total_sec;
	Initialize();
}

HANDLE CDriverImageFile::OpenStorageFile(const CJCStringT fn, ULONG64 secs)
{
	HANDLE file = INVALID_HANDLE_VALUE;
	file = ::CreateFileW(fn.c_str(), FILE_ALL_ACCESS, 0, 
		NULL, OPEN_ALWAYS, FILE_FLAG_RANDOM_ACCESS, NULL );
	if(INVALID_HANDLE_VALUE == file) THROW_WIN32_ERROR(_T("cannot open file %s"), fn.c_str() );

    LARGE_INTEGER file_size = {0,0};
    if(!::GetFileSizeEx(file, &file_size))	THROW_WIN32_ERROR(_T("failure on getting file size"));
	printf("creating img file ..");

	// 填充文件大小，比用WriteFile填写速度要快。
	ULONG64 file_len = SECTOR_TO_BYTEL(secs);
	HANDLE h_map = CreateFileMapping(file, 
		NULL, PAGE_READWRITE, HIDWORD(file_len), LODWORD(file_len), NULL);
	if (!h_map) THROW_WIN32_ERROR(_T("failure on creating file mapping."));
	CloseHandle(h_map);
	printf(" Done \n");
	return file;
}


bool CDriverImageFile::Initialize(void)
{
	// initialize vendor command
	memset(m_vendor_cmd, 0, SECTOR_SIZE);

	// card mode
	m_chunk_size = m_config.m_chunk_size;
	m_page_size = m_chunk_size * m_config.m_chunk_per_page;

	LoadBinFile(FN_VENDOR_BIN, m_buf_vendor, SECTOR_SIZE);
	LoadBinFile(FN_FID_BIN, m_buf_fid, SECTOR_SIZE);
	LoadBinFile(FN_SFR_BIN, m_buf_sfr, SECTOR_SIZE);
	LoadBinFile(FN_PAR_BIN, m_buf_par, SECTOR_SIZE);
	LoadBinFile(FN_IDTAB_BIN, m_buf_idtable, SECTOR_SIZE);
	LoadBinFile(FN_DEVINFO_BIN, m_buf_device_info, SECTOR_SIZE);
	m_isp_len = LoadBinFile(FN_ISP_BIN, m_buf_isp, m_config.m_max_isp_len * SECTOR_SIZE, false);
	LOG_DEBUG(_T("load isp bin, len=%d"), m_isp_len / SECTOR_SIZE);
	if (m_isp_len)	m_run_type = RUN_ISP;
	else			m_run_type = RUN_ROM_CODE;

	m_info_len = LoadBinFile(FN_INFO_BIN, m_buf_info, m_config.m_max_isp_len * SECTOR_SIZE, true);
	LOG_DEBUG(_T("load info bin, len=%d"), m_info_len / SECTOR_SIZE);

	m_orgbad_len = LoadBinFile(FN_ORGBAD_BIN, m_buf_orgbad,  m_config.m_max_isp_len * SECTOR_SIZE, true);
	LOG_DEBUG(_T("load org bad bin, len=%d"), m_orgbad_len / SECTOR_SIZE);

	LoadBinFile(FN_BOOTISP_BIN, m_buf_bootisp, BOOTISP_SIZE * SECTOR_SIZE, false);
	// 
	//m_run_type = RUN_ISP;
	m_vendorcmd_st = VDS0;

	// load for solo tester
	if (!m_config.m_solo_tester.empty())
	{
		LoadBinFile(FN_INQUERY, m_config.m_solo_tester, m_buf_inquery, SECTOR_SIZE, true);
		LoadBinFile(FN_CMD_F083, m_config.m_solo_tester, m_buf_f083, SECTOR_SIZE, true);
		LoadBinFile(FN_CMD_F800, m_config.m_solo_tester, m_buf_f800, SECTOR_SIZE, true);
	}
	return true;
}

CDriverImageFile::~CDriverImageFile(void)
{
	LOG_STACK_TRACE();
	CloseHandle(m_file);
}

ULONG32	CDriverImageFile::Read(UCHAR * buf, ULONG64 lba, ULONG32 secs)
{
	LOG_NOTICE(_T("[ATA],RD,%08I64X,%d"), lba, secs);

	ULONG32 byte_count = secs * SECTOR_SIZE;

    DWORD bytesWritten = 0;
    LARGE_INTEGER tmpLi;
	tmpLi.QuadPart = lba * SECTOR_SIZE;
	BOOL br = ::SetFilePointerEx(m_file, tmpLi, &tmpLi, FILE_BEGIN);
    if(!br) THROW_WIN32_ERROR(_T(" SetFilePointerEx failed. "));

	bool vs = VendorCmdStatus(READ, lba, secs, buf);
	if ( !vs )
	{	// normal command
		br = ::ReadFile(m_file, buf, byte_count, &bytesWritten, 0);
		if (!br) THROW_WIN32_ERROR(_T(" ReadFile failed. "));
	}

	LOG_DEBUG(_T("data read: %02X, %02X, %02X, %02X"), buf[0], buf[1], buf[2], buf[3]);
	return STATUS_SUCCESS;
}

ULONG32	CDriverImageFile::Write(const UCHAR * buf, ULONG64 lba, ULONG32 secs)
{
	LOG_NOTICE(_T("[ATA],WR,%08I64X,%d"), lba, secs);

	// for test only: dummy write
	if ( (lba >= m_config.m_dummy_write_start) && 
		(lba < (m_config.m_dummy_write_start + m_config.m_dummy_write_size) ) )
		return STATUS_SUCCESS;

	ULONG32 byte_count = secs * SECTOR_SIZE;

    DWORD bytesWritten = 0;
    LARGE_INTEGER tmpLi;tmpLi.QuadPart = lba * SECTOR_SIZE;
	BOOL br = ::SetFilePointerEx(m_file, tmpLi, &tmpLi, FILE_BEGIN);
    if (!br) THROW_WIN32_ERROR(_T(" SetFilePointerEx failed. "));

	bool vs = VendorCmdStatus(WRITE, lba, secs, const_cast<UCHAR*>(buf) );
	if (!vs)
	{
		LOG_DEBUG(_T("actual write"));
		br = ::WriteFile(m_file, buf, byte_count, &bytesWritten, 0);
		if (!br) 	THROW_WIN32_ERROR(_T(" WriteFile failed. "));
	}
	return STATUS_SUCCESS;
}

ULONG32	CDriverImageFile::FlushCache(void)
{
	LOG_NOTICE(_T("[ATA],FLUSH"));
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
		JCSIZE len = min(buf_len, cb[4]);	// len must < 256
		LOG_NOTICE(_T("[SCSI],INQUERY, len=%d, buf_len=%d"), len, buf_len);
		memcpy_s(buf, buf_len, m_buf_inquery, len);
		LOG_DEBUG(_T("%02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X, "),
			buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7] );
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

		//buf[0] = HIWORD(HIBYTE(m_file_secs) );
		//buf[1] = HIWORD(LOBYTE(m_file_secs) );
		//buf[2] = LOWORD(HIBYTE(m_file_secs) );
		//buf[3] = LOWORD(LOBYTE(m_file_secs) );

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
			memcpy_s(buf, buf_len, m_buf_f083, SECTOR_SIZE);
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
			if (buf_len > 0)	memcpy_s(buf, buf_len, m_buf_f800, min(buf_len, SECTOR_SIZE));
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


bool CDriverImageFile::VendorCmdStatus(READ_WRITE rdwr, ULONG64 lba, ULONG32 secs, UCHAR * buf)
{
	LOG_STACK_TRACE();
	bool vendor_cmd = false;
	switch (m_vendorcmd_st)
	{
	case VDS0:	// waiting 0x00AA
		if ( (rdwr == READ) && (secs == 1) && (lba == 0x00AA) )
		{
			LOG_DEBUG(_T("vendor status 1"));
			m_vendorcmd_st=VDS1;
		}
		else	m_vendorcmd_st = VDS0;
		break;
	case VDS1: // waiting 0xAA00
		if ( (rdwr == READ) && (secs == 1) && (lba == 0xAA00) )
		{
			LOG_DEBUG(_T("vendor status 2"));
			m_vendorcmd_st=VDS2;
		}
		else	m_vendorcmd_st = VDS0;
		break;
	case VDS2: // waiting 0x0055
		if ( (rdwr == READ) && (secs == 1) && (lba == 0x0055) )
		{
			LOG_DEBUG(_T("vendor status 3"));
			m_vendorcmd_st=VDS3;
		}
		else	m_vendorcmd_st = VDS0;
		break;
	case VDS3: // waiting 0x5500
		if ( (rdwr == READ) && (secs == 1) && (lba == 0x5500) )
		{
			LOG_DEBUG(_T("vendor status 4"));
			m_vendorcmd_st=VDS4;
		}
		else	m_vendorcmd_st = VDS0;
		break;
	case VDS4: // waiting 0x55AA
		if ( (rdwr == READ) && (secs == 1) && (lba == 0x55AA) )
		{
			memcpy_s(buf, SECTOR_SIZE, m_buf_vendor, SECTOR_SIZE);
			LOG_DEBUG(_T("vendor: %02X, %02X, %02X, %02X"), buf[0], buf[1], buf[2], buf[3]);
			LOG_DEBUG(_T("vendor status cmd"));
			m_vendorcmd_st=VDS_CMD;
			vendor_cmd = true;
		}
		else	m_vendorcmd_st = VDS0;
		break;
	case VDS_CMD:		{// send vender command
		if ( (rdwr == WRITE) && (secs == 1) && (lba == 0x55AA) )
		{
			// save vendor command
			memcpy_s(m_vendor_cmd, SECTOR_SIZE, buf, SECTOR_SIZE);
			LOG_DEBUG(_T("vendor status data"));
			m_vendorcmd_st=VDS_DATA;
			vendor_cmd = true;
		}
		else m_vendorcmd_st = VDS0;
		break;	}

	case VDS_DATA: // read/write data
		// copy data
		if ( (lba == 0x55AA) )
		{
			ProcessVendorCommand(m_vendor_cmd, SECTOR_SIZE, rdwr, buf, secs);
			LOG_DEBUG(_T("vendor data: %02X, %02X, %02X, %02X"), buf[0], buf[1], buf[2], buf[3]);
			vendor_cmd = true;
		}
		m_vendorcmd_st = VDS0;
		break;

	default:
		JCASSERT(0);
		break;
	}
	return vendor_cmd;
}

#define LOG_VENDOR_CMD	{						\
		CJCStringT str;		TCHAR _tmp[16];		\
		for (int ii =2; ii<16; ++ii)	{		\
			_stprintf_s(_tmp, _T("%02X,"), vcmd[ii]);	str += _tmp;	}	\
		LOG_DEBUG(_T("[VENDOR] content: %s"), str.c_str() );	\
}

JCSIZE CDriverImageFile::ProcessVendorCommand(const UCHAR * vcmd, JCSIZE vcmd_len,  READ_WRITE rdwr, UCHAR * data, JCSIZE data_sec)
{
	LOG_STACK_TRACE();
	WORD cmd_id = MAKEWORD(vcmd[1], vcmd[0]);
	BYTE vnd_data_len = vcmd[11];

	WORD block = MAKEWORD(vcmd[3], vcmd[2]);
	WORD page = MAKEWORD(vcmd[5], vcmd[4]);

	JCSIZE processed_len = 0;		// 已经处理的长度
	switch (cmd_id)
	{
	case 0xF004:	// READ PAR
		LOG_NOTICE(_T("[VENDOR] PAR (F004)"))
		memcpy_s(data, data_sec * SECTOR_SIZE, m_buf_par, SECTOR_SIZE);
		processed_len = SECTOR_SIZE;
		break;

	case 0xF00A:	// READ FLASH DATA
	case 0xF10A:	{
		BYTE chunk = vcmd[6];
		BYTE mode = vcmd[8];
		LOG_NOTICE(_T("[VENDOR] READ FLASH DATA (%04X): b=%04X, p=%04X,k=%02X,m=%02X,len=%d")
			, cmd_id, block, page, chunk, mode, vnd_data_len);
		ReadFlashData(block, page, chunk, mode, vnd_data_len, data, data_sec);
		break;			}

	case 0xF10B: {	// WRITE_FLASH
		//BYTE chunk = vcmd[6];
		//BYTE mode = vcmd[8];
		LOG_NOTICE(_T("[VENDOR] WRITE BLOCK (F10B), block:%04X, page:%04X"), block, page);
		LOG_VENDOR_CMD
		WriteFlashData(block, page, vnd_data_len, data, data_sec);
		break;}

	case 0xF020:	// READ FLASH ID
		LOG_NOTICE(_T("[VENDOR] READ FLASH ID (F020)"))
		memcpy_s(data, data_sec * SECTOR_SIZE, m_buf_fid, SECTOR_SIZE);
		data[0x0A] = (UCHAR)(m_run_type);
		processed_len = SECTOR_SIZE;
		break;

	case 0xF026:	// READ SFR
		LOG_NOTICE(_T("[VENDOR] READ SFR (F026)"))
		memcpy_s(data, data_sec * SECTOR_SIZE, m_buf_sfr, SECTOR_SIZE);
		processed_len = SECTOR_SIZE;
		break;

	case 0xF02C:	// RESET CPU
		LOG_NOTICE(_T("[VENDOR] RESET CPU (F02C)"));
		m_run_type = RUN_ISP;
		break;

	case 0xF03F:	// READ IDENTIFY DEVICE
	case 0xF13F:
		LOG_NOTICE(_T("[VENDOR] READ IDTABLE (%04X)"), cmd_id)
		processed_len = ReadIDTable(data, data_sec * SECTOR_SIZE);
		break;

	case 0xF047:	// DEVICE INFO
		LOG_NOTICE(_T("[VENDOR] DEVIDE INFO (F047), len:%d"), vnd_data_len)
		memcpy_s(data, data_sec * SECTOR_SIZE, m_buf_device_info, SECTOR_SIZE);
		processed_len = SECTOR_SIZE;
		break;

	case 0xF127:	// DOWNLOAD MPISP
		LOG_NOTICE(_T("[VENDOR] DOWNLOAD MPISP (F127), %02X, len:%d"), vcmd[4], data_sec);
		// save mpisp to file
		if (vnd_data_len > data_sec)	LOG_WARNING(_T("vnd_data_len (%d) > data (%d)"),
			vnd_data_len, data_sec);
		if (vcmd[4] == 0x40)		SaveBinFile(FN_MPISP_LOG, data, data_sec * SECTOR_SIZE);
		else if (vcmd[4] == 0x80)	SaveBinFile(FN_PRETEST_LOG, data, data_sec * SECTOR_SIZE);
		m_run_type = RUN_MPISP;
		break;

	case 0xF139: {	// DOWNLOAD BOOTISP		
		LOG_NOTICE(_T("[VENDOR] DOWNLOAD BOOTISP (F139), len:%d"),  data_sec);
		memcpy_s(m_buf_bootisp, BOOTISP_SIZE * SECTOR_SIZE, 
			data, min(data_sec, BOOTISP_SIZE) * SECTOR_SIZE);
		SaveBinFile(FN_BOOTISP_BIN, m_buf_bootisp, BOOTISP_SIZE * SECTOR_SIZE);
		//SaveBinFile(FN_BOOTISP_LOG, m_buf_bootisp, BOOTISP_SIZE * SECTOR_SIZE);
		break;		 }

	case 0xF029: {	// READ BOOTISP
		LOG_NOTICE(_T("[VENDOR] READ BOOTISP (F029), len:%d"),  data_sec);
		memcpy_s(data, data_sec * SECTOR_SIZE, m_buf_bootisp, BOOTISP_SIZE * SECTOR_SIZE);
		break;	 }

	case 0xF00C:	// ERASE BLOCK
		LOG_NOTICE(_T("[VENDOR] ERASE BLOCK (F00C), block:%04X"), block);
		break;

	default:		{
		LOG_NOTICE(_T("[VENDOR] UNKNOWN VENDOR CMD (%04X)"), cmd_id);
		LOG_VENDOR_CMD
		break;		}
	}
	LOG_DEBUG(_T("[VENDOR] data: %02X, %02X, %02X, %02X,..."), data[0], data[1], data[2], data[3]);
	return processed_len;
}

JCSIZE CDriverImageFile::ReadIDTable(UCHAR * buf, JCSIZE buf_size)
{
	LOG_STACK_TRACE();
	memcpy_s(buf, buf_size, m_buf_idtable, SECTOR_SIZE);
	// set capacity
	UCHAR * temp_buf = m_buf_isp + CHS_START_ADDR_2244LT;
	UINT capacity = MAKELONG(
		MAKEWORD(temp_buf[2], temp_buf[3]), MAKEWORD(temp_buf[0], temp_buf[1]) );
	LOG_DEBUG(_T("capacity = 0x%08X"), capacity);

	buf[0x7B] = HIBYTE(HIWORD(capacity));
	buf[0x7A] = LOBYTE(HIWORD(capacity));
	buf[0x79] = HIBYTE(LOWORD(capacity));
	buf[0x78] = LOBYTE(LOWORD(capacity));

	// set vendor specific
	memcpy_s(buf + 0x102, VENDOR_LENGTH, m_buf_isp + VENDOR_START_ADDR_2244LT, VENDOR_LENGTH);
	// set model name
	memcpy_s(buf + 0x36, MODEL_LENGTH, m_buf_isp + MODEL_START_ADDR_2244LT, MODEL_LENGTH);
	// set sn
	memcpy_s(buf + 0x14, SN_LENGTH, m_buf_isp + SN_START_ADDR_2244LT, SN_LENGTH);

	// set sata speed:	check IF Setting word76 bit1~3
	UINT speed = 0;
	buf[152] &= 0xF1;
	if (m_buf_isp[IF_ADDR_2244LT] & 0x10)			buf[152] |= 0x02;		// force gen1
	else											buf[152] |= 0x06;
	//buf[152] |= 0x06;
	LOG_DEBUG(_T("sata speed: isp:0x%02X, idtab:0x%02X"), m_buf_isp[IF_ADDR_2244LT], buf[152]);

	// set trim: check TRIM word169 bit1
	buf[338] &= 0xFE;
	if (m_buf_isp[TRIM_ADDR_2244LT] & 0x08)		buf[338] |= 0x01;
	LOG_DEBUG(_T("trim: isp:0x%02X, idtab:0x%02X"), m_buf_isp[TRIM_ADDR_2244LT], buf[338])

	// set device sleep : check device sleep word78 bit8
	buf[157] &= 0xFE;
	if (m_buf_isp[DEVSLP_ADDR_2244LT] & 0x10) buf[157] |= 0x01;
	LOG_DEBUG(_T("devslp: isp:0x%02X, idtab:0x%02X"), m_buf_isp[DEVSLP_ADDR_2244LT], buf[157])

	// set f/w version
	memcpy_s(buf + FWVER_OFFSET, FWVER_LENGTH, m_buf_isp + 0xA2E, FWVER_LENGTH);

	return SECTOR_SIZE;
}


void CDriverImageFile::WriteFlashData(WORD block, WORD page, BYTE read_secs, UCHAR * buf, JCSIZE secs)
{
	LOG_STACK_TRACE();
	JCSIZE offset = (page * m_page_size) * SECTOR_SIZE;
	if (block == 3 || block ==4)
	{	// isp
		if (offset >= m_config.m_max_isp_len * SECTOR_SIZE) 
		{
			LOG_ERROR(_T("write isp overflow, offset=%X, page=%d"), offset, page);
			return;
		}
		memcpy_s(m_buf_isp + offset, m_config.m_max_isp_len * SECTOR_SIZE - offset, buf, secs * SECTOR_SIZE);
	}
}

void CDriverImageFile::ReadFlashData(WORD block, WORD page, BYTE chunk, BYTE mode, BYTE read_secs, UCHAR * buf, JCSIZE secs)
{
	LOG_STACK_TRACE();
	LOG_DEBUG(_T("read len=%d, buf len=%d"), read_secs, secs);
	memset(buf, 0, secs * SECTOR_SIZE);

	// 计算offset
	JCSIZE offset = (page * m_page_size + chunk * m_chunk_size) * SECTOR_SIZE;
	if (block == 3 || block ==4)
	{	// isp block
		if (offset >= m_isp_len)
		{
			memset(buf, 0xFF, (m_chunk_size+1) * SECTOR_SIZE);
			return;
		}
		memcpy_s(buf, m_chunk_size * SECTOR_SIZE, m_buf_isp + offset, m_chunk_size* SECTOR_SIZE);
		memset(buf + m_chunk_size * SECTOR_SIZE, 0xE4, 9);
		buf[m_chunk_size * SECTOR_SIZE +6] = 0x16;
		buf[m_chunk_size * SECTOR_SIZE +7] = 0xB4;
	}
	else if (block == 1 || block == 2)
	{	// info
		LOG_DEBUG(_T("read info: info_len =%d, offset = %d"), m_info_len, offset);
		if (offset >= m_info_len)
		{
			memset(buf, 0xFF, min((m_chunk_size+1), secs) * SECTOR_SIZE);
			return;
		}
		memcpy_s(buf, secs * SECTOR_SIZE, m_buf_info + offset, min(m_chunk_size,secs)* SECTOR_SIZE);
		if (secs > m_chunk_size)
		{
			memset(buf + m_chunk_size* SECTOR_SIZE, 0xE1, 4);
			memset(buf + m_chunk_size* SECTOR_SIZE + 6 , 0x55, 4);
		}
	}
	else if (block == 6)
	{	// orgbad
		if (offset >= m_orgbad_len)
		{
			memset(buf, 0xFF, min(m_chunk_size+1, secs)*SECTOR_SIZE);
			return;
		}
		memcpy_s(buf, secs * SECTOR_SIZE, m_buf_orgbad + offset, min(m_chunk_size,secs)* SECTOR_SIZE);
		if (secs > m_chunk_size)
		{
			memset(buf + m_chunk_size* SECTOR_SIZE, 0xEA, 4);
			memset(buf + m_chunk_size* SECTOR_SIZE + 6 , 0x55, 4);
		}
		
	}
	else
	{	// other blocks
		char str[128];
		sprintf_s(str, "BLOCK:%04X------PAGE:%04X-------CHUNK:%02X", block, page, chunk);
		memcpy_s(buf, m_chunk_size*SECTOR_SIZE, str, strlen(str));
	}
}

JCSIZE CDriverImageFile::LoadBinFile(const CJCStringT & fn, UCHAR * buf, 
									 JCSIZE buf_len, bool mandatory)
{
	return LoadBinFile(fn, m_config.m_data_folder, buf, buf_len, mandatory);
}

JCSIZE CDriverImageFile::LoadBinFile(const CJCStringT & fn, const CJCStringT & folder,
									 UCHAR * buf, JCSIZE buf_len, bool mandatory)
{
	CJCStringT path;
	path = folder + fn;

	FILE * file = NULL;
	JCSIZE read = 0;
	stdext::jc_fopen(&file, path.c_str(), _T("rb"));
	if (file)
	{
		read = fread(buf, 1, buf_len, file);
		fclose(file);
	}
	else 
	{
		if (mandatory) THROW_ERROR(ERR_PARAMETER, _T("failure on openning file %s"), path.c_str())
		else LOG_ERROR(_T("failure on openning file %s"), path.c_str());
	}	
	return read;
}

void CDriverImageFile::SaveBinFile(const CJCStringT & fn, UCHAR * buf, JCSIZE buf_len)
{
	FILE * file = NULL;
	stdext::jc_fopen(&file, fn.c_str(), _T("w+b"));
	if (file == NULL) THROW_ERROR(ERR_PARAMETER, _T("failure on openning file %s"), fn.c_str());
	JCSIZE written = fwrite(buf, 1, buf_len, file);
	fclose(file);
	//return read;
}

void SetStatus(const CJCStringT & status, jcparam::IValue * param_set)
{
	LOG_STACK_TRACE();
}

void SendEvent(void)
{
	LOG_STACK_TRACE();
}

//class jcparam::CArguDefList jcapp::AppArguSupport::m_cmd_parser;


