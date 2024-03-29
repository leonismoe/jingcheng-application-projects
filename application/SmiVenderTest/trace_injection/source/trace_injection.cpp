// copy_compare_tester.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include <vld.h>
#include <stdext.h>
#include <jcapp.h>
#include <ata_trace.h>
#include <SmiDevice.h>
#include <vector>

#include "trace_injection.h"


LOCAL_LOGGER_ENABLE(_T("trace_inj"), LOGGER_LEVEL_NOTICE);


CApplication _app;

const TCHAR	CTraceInjApp::LOG_CONFIG_FN[] =_T("trace_inj.log.cfg");

#define _class_name_	CApplication

BEGIN_ARGU_DEF_TABLE()
	ARGU_DEF_ITEM(_T("driver"),		_T('d'), TCHAR,		m_drive_letter,		_T("test driver letter A~Z or 0~9...") )
	ARGU_DEF_ITEM(_T("interface"),	_T('i'), CJCStringT,	m_drive_type,	_T("interface of drive ATA, SCSI...") )
	ARGU_DEF_ITEM(_T("script"),		_T('s'), CJCStringT,	m_test_script,	_T("file name of test script.") )
	ARGU_DEF_ITEM(_T("pattern"),	_T('p'), CJCStringT,	m_data_pattern,	_T("file name of test data pattern.") )
	ARGU_DEF_ITEM(_T("base"),		_T('b'), FILESIZE,		m_base_lba,		_T("lba offset") )
	//ARGU_DEF_ITEM(_T("backup"),		_T('b'), bool,	m_backup,				_T("backup data before test.") )
	//ARGU_DEF_ITEM(_T("open_phy"),	_T('y'), bool,	m_open_physical,		_T("force open physical drive.") )
	//ARGU_DEF_ITEM(_T("clean"),		_T('c'), WORD,	m_clean_pattern,		_T("do clean after test, specify pattern. must <= 0xFF") )
	//ARGU_DEF_ITEM(_T("reset"),		_T('r'), UINT,	m_reset,				_T("power off/on after test. specify delay (s) between power off/on") )
END_ARGU_DEF_TABLE()


#define MAX_LINE_BUF	(256)
#define TEMP_FILE_NAME	_T("backup.bin")

///////////////////////////////////////////////////////////////////////////////
//-- defination

CTraceInjApp::CTraceInjApp(void)
: CJCAppSupport(ARGU_SUPPORT | ARGU_SUPPORT_HELP)
, m_drive_letter(0), m_dev(NULL)
, m_temp_buf(NULL), m_temp_len(0)
, m_backup(false), m_open_physical(false)
, m_clean_pattern(0x100)
, m_file_mapping(NULL)
, m_reset(0)
, m_base_lba(0)
{
	//CAtaTraceTable::Create(100, m_trace);
	////m_trace = new CAtaTraceTable;
}

void CTraceInjApp::LoadPattern(const CJCStringT & fn, CTraceInjApp::TRACE & trace)
{
	LOG_STACK_TRACE();
	CAtaTraceLoader loader;

	//if (m_data_pattern.empty())		m_data_pattern = fn + _T(".bin");
	JCSIZE line_no = 0;
	stdext::auto_array<char> _line_buf(MAX_LINE_BUF);
	char * line_buf = _line_buf;

	// open file
	FILE * src_file = NULL;
	_tfopen_s(&src_file, fn.c_str(), _T("r"));
	if (!src_file) THROW_ERROR(ERR_USER, _T("failure on openning file %s"), fn.c_str() );

	// 解析文件头
	fgets(line_buf, MAX_LINE_BUF, src_file);	// Skip header
	loader.Initialize(m_data_pattern, line_buf);
	line_no ++;

	//static const JCSIZE ALIGNE_SIZE = 4096;		// Aligne to 4KB
	//stdext::auto_array<BYTE> _buf(SECTOR_TO_BYTE(256) + 2 * ALIGNE_SIZE);
	stdext::auto_buf _buf(SECTOR_TO_BYTE(256));
	BYTE * buf = _buf;

	//BYTE * buf = (BYTE*)(VirtualAlloc(NULL, SECTOR_TO_BYTE(256), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	if (!buf) THROW_WIN32_ERROR(_T("failure on allocating buffer") );

	while (1)
	{
		line_no ++;
		CAtaTraceRow * trace_row = NULL;
		if (! fgets(line_buf, MAX_LINE_BUF, src_file) ) break;
		bool br = loader.ParseLine(line_buf, trace_row);
		if ( (!br) || (NULL == trace_row) )
		{
			LOG_ERROR(_T("trace format error at line %d"), line_no);
			continue;
		}
		InvokeTrace(buf, 256, trace_row);

		LOG_DEBUG(_T("lba:0x%08X, sec:%d"), trace_row->m_lba, trace_row->m_sectors);
		trace_row->Release();
	}

	//if (buf) VirtualFree(buf, SECTOR_TO_BYTE(256), MEM_RELEASE | MEM_DECOMMIT);
}

bool CTraceInjApp::InvokeTrace(BYTE * buf, JCSIZE buf_secs, CAtaTraceRow * trace)
{
	JCASSERT(trace);
	JCASSERT(m_dev);
	jcparam::IBinaryBuffer * data = NULL;

	FILESIZE lba = (trace->m_lba) + m_base_lba;
	JCSIZE secs = trace->m_sectors;
	if (secs > buf_secs)
	{
		LOG_ERROR(_T("[ERROR] sectors overflow: store:%d, CMD:%02X, LBA:%08I64X, SECS:%d"), trace->m_id, trace->m_cmd_code, lba, secs);
		return false;
	}
	if ( (lba + secs) >= m_dev_total_secs)
	{
		LOG_ERROR(_T("[ERROR] lba overflow: store:%d, CMD:%02X, LBA:%08I64X, SECS:%d"), trace->m_id, trace->m_cmd_code, lba, secs);
		return false;
	}



	bool br = false;
	switch (trace->m_cmd_code)
	{
	case CMD_WRITE_DMA:		{
		memset(buf, 0x55, SECTOR_TO_BYTE(secs));
		br = m_dev->SectorWrite(buf, lba, secs);
		//br = m_dev->ScsiWrite(buf, lba, secs, 100);
		if (!br)
		{
			LOG_ERROR(_T("[ERROR] write error store:%d, LBA:%08I64X, SECS:%d"), trace->m_id, lba, secs);
			break;
		}
		m_total_write += secs;
		break;				}

	case CMD_READ_DMA:	{
		//if ( (lba + secs) >= m_dev_total_secs)
		//{
		//	LOG_ERROR(_T("[ERROR] lba overflow: CMD:%02X, LBA:%08I64X, SECS:%d"), trace->m_cmd_code, lba, secs);
		//	break;
		//}
		br = m_dev->SectorRead(buf, lba, secs);
		//br = m_dev->ScsiRead(buf, lba, secs, 100);
		if (!br)
		{
			LOG_ERROR(_T("[ERROR] write error store:%d, LBA:%08I64X, SECS:%d"), trace->m_id, lba, secs);
			break;
		}
		m_total_read += secs;
		break;				}

	case CMD_FLUSH_CACHE:	{
		m_dev->FlushCache();
		break;				}

	case CMD_POWER_ON:			{	// power on
		m_dev->StartStopUnit(false);	// power on
		//Sleep(sleep);
		break;			}

	case CMD_POWER_OFF:			{	// power off
		m_dev->StartStopUnit(true);	// power off
		//DWORD sleep = (DWORD)(trace->m_busy_time);		// ms
		//Sleep(sleep);
		break;	}
	}
	DWORD sleep = (DWORD)(trace->m_busy_time);		// ms
	if (sleep != 0) Sleep(sleep);
	return true;
}

bool CTraceInjApp::RunTestPattern(IStorageDevice * dev, TRACE & trace)
{
	LOG_STACK_TRACE();
	JCASSERT(dev);

	//FILESIZE total_secs = dev->GetCapacity();
	//LOG_DEBUG(_T("total sectors = 0x08I64X"), total_secs);
	//m_total_read = 0;
	//m_total_write = 0;

	stdext::auto_array<BYTE> _buf(SECTOR_TO_BYTE(256));
	BYTE * buf = _buf;
	
	TRACE::iterator it = trace.begin();
	TRACE::iterator endit = trace.end();
	for (;it != endit; ++it)
	{
		CAtaTraceRow * row = (*it);				JCASSERT(row);
		InvokeTrace(buf, 256, row);
	}
	return true;
}

bool CTraceInjApp::OpenDrive(TCHAR drive_letter, const CJCStringT & drive_type, IStorageDevice * &dev)
{
	LOG_STACK_TRACE();
	JCASSERT(dev==NULL);

	TCHAR dev_name[32];
	if ( ( (_T('A') <= drive_letter) && (drive_letter <= _T('Z')) )
		|| ( (_T('a') <= drive_letter) && (drive_letter <= _T('z') ) ) )
	{
		if (m_open_physical)
		{
			UINT drive_number = CSmiRecognizer::GetDriveNumber(drive_letter);
			_stprintf_s(dev_name, _T("\\\\.\\PhysicalDrive%d"), drive_number);
		}
		else	_stprintf_s(dev_name, _T("\\\\.\\%c:"), drive_letter);
	}
	else if ( (_T('0') <= drive_letter) && (drive_letter <= _T('9') ) )
	{
		_stprintf_s(dev_name, _T("\\\\.\\PhysicalDrive%c"), drive_letter);
	}
	else
	{
		THROW_ERROR(ERR_USER, _T("unknow device letter %d"), drive_letter);
	}
	LOG_DEBUG(_T("open drive: %s"), dev_name);
	bool br = CSmiRecognizer::AutoStorageDevice(dev_name, drive_type, dev);
	if (!br) THROW_ERROR(ERR_APP, _T("cannot open device %s"), dev_name); 
	dev->UnmountAllLogical();
	dev->DeviceLock();

	m_dev_total_secs = dev->GetCapacity();
	LOG_DEBUG(_T("total sectors = 0x08I64X"), m_dev_total_secs);
	m_total_read = 0;
	m_total_write = 0;

	return true;
}

#define CMD_BLOCK_SIZE	16

bool CTraceInjApp::PowerCycle(IStorageDevice * dev, UINT delay)
{
	LOG_STACK_TRACE();
	JCASSERT(dev);

	// power on off instead reset
	bool br = dev->StartStopUnit(true);	// power off
	if (!br) THROW_ERROR(ERR_APP, _T("failure on power off"));
	Sleep(delay);
	br = dev->StartStopUnit(false);	// power on
	if (!br) THROW_ERROR(ERR_APP, _T("failure on power on"));
	return true;
}

int CTraceInjApp::Run(void)
{
	LOG_STACK_TRACE();
	// open driver
	bool br = false;
	br = OpenDrive(m_drive_letter, m_drive_type, m_dev);

	bool compare = true;	// compare result;

	// load pattern
	LoadPattern(m_test_script, m_trace);
	//RunTestPattern(m_dev, m_trace);

	stdext::jc_printf(_T("total write secters: 0x%08I64X\n"), m_total_write);
	stdext::jc_printf(_T("total read secters: 0x%08I64X\n"), m_total_read);
	LOG_NOTICE(_T("total write secters: 0x%08I64X\n"), m_total_write);
	LOG_NOTICE(_T("total read secters: 0x%08I64X\n"), m_total_read);
	return 0;
}

int CTraceInjApp::Initialize(void)
{
	return 1;
}

void CTraceInjApp::CleanUp(void)
{
	__super::CleanUp();

	if (m_dev) m_dev->Release(), m_dev = NULL;

	TRACE::iterator it = m_trace.begin();
	TRACE::iterator endit = m_trace.end();
	for (;it != endit; ++it)	if (*it)	(*it)->Release();

	it = m_backup_trace.begin();
	endit = m_backup_trace.end();
	for (;it != endit; ++it)	if (*it)	(*it)->Release();

	if (m_file_mapping) m_file_mapping->Release();

	delete [] m_temp_buf;

#ifdef _DEBUG
	stdext::jc_printf(_T("Press any key to continue..."));
	getc(stdin);
#endif
}

int _tmain(int argc, _TCHAR* argv[])
{
	return jcapp::local_main(argc, argv);
}

