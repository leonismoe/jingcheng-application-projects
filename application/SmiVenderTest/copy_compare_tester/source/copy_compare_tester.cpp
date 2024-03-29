// copy_compare_tester.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include <vld.h>
#include <stdext.h>
#include <jcapp.h>
#include <ata_trace.h>
#include <SmiDevice.h>
#include <vector>


LOCAL_LOGGER_ENABLE(_T("qc_test"), LOGGER_LEVEL_NOTICE);

//class CQCTestApp : public jcapp::CJCAppSupport<jcapp::AppArguSupport>
class CQCTestApp : public jcapp::CJCAppicationSupport
{
public:
	CQCTestApp(void);
public:
	virtual int Initialize(void);
	virtual int Run(void);
	virtual void CleanUp(void);
	virtual LPCTSTR AppDescription(void) const
	{
		static const TCHAR desc[] = 
			_T("Copy Compare Test Tool\n")
			_T("\t Silicon Motion\n");
		return desc;
	}

protected:
	typedef std::vector<CAtaTraceRow *> TRACE;
	void LoadPattern(const CJCStringT & fn, TRACE & trace);
	bool OpenDrive(TCHAR drive_letter, const CJCStringT & drive_type, IStorageDevice * &dev);
	bool RunTestPattern(IStorageDevice * dev, TRACE & trace);
	// 如果data_pattern为NULL则使用trace所带的data，否则使用data_pattern指定的data
	bool WriteTestPattern(IStorageDevice * dev, TRACE & trace, BYTE * data_pattern, LPCTSTR verb);
	bool ReadCompareTest(IStorageDevice * dev, TRACE & trace);
	bool BackupData(IStorageDevice * dev, TRACE & src_trace, TRACE & back_trace);
	bool PowerCycle(IStorageDevice * dev, UINT delay);

//
public:
	TCHAR	m_drive_letter;
	CJCStringT	m_drive_type;
	CJCStringT	m_test_script, m_data_pattern;		// file name of configuration.
	bool	m_backup, m_open_physical;
	WORD	m_clean_pattern;
	UINT	m_reset;
	static const TCHAR	LOG_CONFIG_FN[];

protected:
	TRACE	m_trace;
	TRACE	m_backup_trace;
	IStorageDevice * m_dev;
	// 用于读取data的临时buf，考虑到性能，申请的空间将按需扩大。
	BYTE *	m_temp_buf;
	JCSIZE	m_temp_len;
	CFileMapping * m_file_mapping;	// for data backup
};

typedef jcapp::CJCApp<CQCTestApp>	CApplication;
CApplication _app;

const TCHAR	CQCTestApp::LOG_CONFIG_FN[] =_T("qctool.log.cfg");

#define _class_name_	CApplication

BEGIN_ARGU_DEF_TABLE()
	ARGU_DEF_ITEM(_T("driver"),		_T('d'), TCHAR,		m_drive_letter,		_T("test driver letter A~Z or 0~9...") )
	ARGU_DEF_ITEM(_T("interface"),	_T('i'), CJCStringT,	m_drive_type,	_T("interface of drive ATA, SCSI...") )
	ARGU_DEF_ITEM(_T("script"),		_T('s'), CJCStringT,	m_test_script,	_T("file name of test script.") )
	ARGU_DEF_ITEM(_T("pattern"),	_T('p'), CJCStringT,	m_data_pattern,	_T("file name of test data pattern.") )
	ARGU_DEF_ITEM(_T("backup"),		_T('b'), bool,	m_backup,				_T("backup data before test.") )
	ARGU_DEF_ITEM(_T("open_phy"),	_T('y'), bool,	m_open_physical,		_T("force open physical drive.") )
	ARGU_DEF_ITEM(_T("clean"),		_T('c'), WORD,	m_clean_pattern,		_T("do clean after test, specify pattern. must <= 0xFF") )
	ARGU_DEF_ITEM(_T("reset"),		_T('r'), UINT,	m_reset,				_T("power off/on after test. specify delay (s) between power off/on") )
END_ARGU_DEF_TABLE()


#define MAX_LINE_BUF	(256)
#define TEMP_FILE_NAME	_T("backup.bin")

///////////////////////////////////////////////////////////////////////////////
//-- defination

CQCTestApp::CQCTestApp(void)
: CJCAppSupport(ARGU_SUPPORT | ARGU_SUPPORT_HELP)
, m_drive_letter(0), m_dev(NULL)
, m_temp_buf(NULL), m_temp_len(0)
, m_backup(false), m_open_physical(false)
, m_clean_pattern(0x100)
, m_file_mapping(NULL)
, m_reset(0)

{
	//CAtaTraceTable::Create(100, m_trace);
	////m_trace = new CAtaTraceTable;
}

void CQCTestApp::LoadPattern(const CJCStringT & fn, CQCTestApp::TRACE & trace)
{
	LOG_STACK_TRACE();
	CAtaTraceLoader loader;

	if (m_data_pattern.empty())		m_data_pattern = fn + _T(".bin");

	//CJCStringT data_file = fn + _T(".bin");

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
		trace.push_back(trace_row);
		LOG_DEBUG(_T("lba:0x%08X, sec:%d"), trace_row->m_lba, trace_row->m_sectors);
	}
}

bool CQCTestApp::RunTestPattern(IStorageDevice * dev, TRACE & trace)
{
	LOG_STACK_TRACE();
	JCASSERT(dev);
	
	TRACE::iterator it = trace.begin();
	TRACE::iterator endit = trace.end();
	for (;it != endit; ++it)
	{
		CAtaTraceRow * row = (*it);				JCASSERT(row);
		// <TODO> backup
		// write data to device
		jcparam::IBinaryBuffer * src_buf = row->m_data;
		JCSIZE test_sec = row->m_sectors;
		JCSIZE test_len = test_sec * SECTOR_SIZE;
		JCSIZE locked = 0;
		BYTE * data = src_buf->Lock();	JCASSERT(data);
		bool br = dev->SectorWrite(data, row->m_lba, test_sec);
		if (!br) THROW_ERROR(ERR_APP, _T("failure on writing data lba:0x%08X, len:%d"), 
			row->m_lba, test_sec);
		// read data to temp mem
		if (m_temp_len < test_len)
		{
			delete [] m_temp_buf;
			m_temp_buf = new BYTE[test_len];
			m_temp_len = test_len;
		}
		dev->SectorRead(m_temp_buf, row->m_lba, test_sec);

		// compare data
		int ir = memcmp(m_temp_buf, data, test_len);
		src_buf->Unlock(data);
		if (ir == 0) continue;
		// compare failed
		// <TODO> output log
		return false;
	}
	return true;
}

bool CQCTestApp::WriteTestPattern(IStorageDevice * dev, TRACE & trace, BYTE * data_pattern, LPCTSTR verb)
{
	LOG_STACK_TRACE();
	JCASSERT(dev);
	
	TRACE::iterator it = trace.begin();
	TRACE::iterator endit = trace.end();
	for (;it != endit; ++it)
	{
		CAtaTraceRow * row = (*it);				JCASSERT(row);
		// write data to device
		jcparam::IBinaryBuffer * src_buf = row->m_data;
		JCSIZE test_sec = row->m_sectors;
		JCSIZE test_len = test_sec * SECTOR_SIZE;
		JCSIZE locked = 0;
		BYTE * data = NULL;
		if (data_pattern)	data = data_pattern;
		else
		{
			data = src_buf->Lock();	JCASSERT(data);
		}
		_tprintf_s(_T("%s from lba:0x%08X, len:%d secs..."), verb, row->m_lba, test_sec);
		bool br = dev->SectorWrite(data, row->m_lba, test_sec);
		if (!br) THROW_ERROR(ERR_APP, _T("failure on writing data lba:0x%08X, len:%d"), 
			row->m_lba, test_sec);
		if (data_pattern == NULL)	src_buf->Unlock(data);
		_tprintf_s(_T("OK\n"));
	}
	return true;
}

bool CQCTestApp::ReadCompareTest(IStorageDevice * dev, TRACE & trace)
{
	LOG_STACK_TRACE();
	JCASSERT(dev);
	
	TRACE::iterator it = trace.begin();
	TRACE::iterator endit = trace.end();
	for (;it != endit; ++it)
	{
		CAtaTraceRow * row = (*it);				JCASSERT(row);
		// write data to device
		jcparam::IBinaryBuffer * src_buf = row->m_data;
		JCSIZE test_sec = row->m_sectors;
		JCSIZE test_len = test_sec * SECTOR_SIZE;
		JCSIZE locked = 0;
		BYTE * data = src_buf->Lock();	JCASSERT(data);

		// read data to temp mem
		if (m_temp_len < test_len)
		{
			delete [] m_temp_buf;
			m_temp_buf = new BYTE[test_len];
			m_temp_len = test_len;
		}
		_tprintf_s(_T("verifying from lba:0x%08X, len:%d secs ..."), row->m_lba, test_sec);
		dev->SectorRead(m_temp_buf, row->m_lba, test_sec);

		// compare data
		int ir = memcmp(m_temp_buf, data, test_len);
		src_buf->Unlock(data);
		if (ir == 0)
		{	// compare passed
			_tprintf_s(_T("passed\n"));
		}
		else
		{	// compare failed
			_tprintf_s(_T("failed\n"));
			return false;
		}
	}
	return true;
}

bool CQCTestApp::BackupData(IStorageDevice * dev, TRACE & src_trace, TRACE & back_trace)
{
	LOG_STACK_TRACE();
	JCASSERT(dev);

	JCASSERT(m_file_mapping == NULL);
	HANDLE hback = CreateFile(TEMP_FILE_NAME, FILE_ALL_ACCESS, 
		0, NULL, CREATE_ALWAYS, 
		FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE | FILE_FLAG_RANDOM_ACCESS, NULL );
	if(INVALID_HANDLE_VALUE == hback)
	{
		THROW_WIN32_ERROR(_T("failure on opening temp file %s"), TEMP_FILE_NAME );
	}
	CreateFileMappingObject(hback, 64*1024, m_file_mapping);		JCASSERT(m_file_mapping);

	
	JCSIZE total_backed = 0;		// in sectors

	TRACE::iterator it = src_trace.begin();
	TRACE::iterator endit = src_trace.end();
	for (;it != endit; ++it)
	{
		CAtaTraceRow * row = (*it);				JCASSERT(row);
		// write data to device
		JCSIZE test_sec = row->m_sectors;
		JCSIZE test_len = test_sec * SECTOR_SIZE;

		stdext::auto_interface<jcparam::IBinaryBuffer> back_buf;
		FILESIZE new_file_size = SECTOR_TO_BYTEL(total_backed + test_sec);
		m_file_mapping->SetSize(new_file_size);
		CreateFileMappingBuf(m_file_mapping, total_backed, test_sec, back_buf);
		BYTE * data = back_buf->Lock();	JCASSERT(data);
		_tprintf_s(_T("backup lba:0x%08X, len:%d sec ..."), row->m_lba, test_sec);
		dev->SectorRead(data, row->m_lba, test_sec);
		back_buf->Unlock(data);
		_tprintf_s(_T("OK\n"));

		total_backed += test_sec;

		stdext::auto_interface<CAtaTraceRow> back_trace_row(new CAtaTraceRow);
		back_trace_row->m_id = row->m_id;
		back_trace_row->m_lba = row->m_lba;
		back_trace_row->m_cmd_code = row->m_cmd_code;
		back_trace_row->m_sectors = row->m_sectors;
		back_buf.detach(back_trace_row->m_data);

		back_trace.push_back(back_trace_row);
		back_trace_row->AddRef();
	}
	return true;
}

bool CQCTestApp::OpenDrive(TCHAR drive_letter, const CJCStringT & drive_type, IStorageDevice * &dev)
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
	return true;
}

#define CMD_BLOCK_SIZE	16

bool CQCTestApp::PowerCycle(IStorageDevice * dev, UINT delay)
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

int CQCTestApp::Run(void)
{
	LOG_STACK_TRACE();
	// open driver
	bool br = false;
	br = OpenDrive(m_drive_letter, m_drive_type, m_dev);

	bool compare = true;	// compare result;

	// load pattern
	LoadPattern(m_test_script, m_trace);
	
	// run pattern
	if (m_backup)	BackupData(m_dev, m_trace, m_backup_trace);

	WriteTestPattern(m_dev, m_trace, NULL, _T("writing"));
	compare = compare && ReadCompareTest(m_dev, m_trace);

	if (m_backup)
	{	// restore
		WriteTestPattern(m_dev, m_backup_trace, NULL, _T("restoring") );
		compare = compare && ReadCompareTest(m_dev, m_backup_trace);
	}
	else if (m_clean_pattern < 0x100)
	{	// do clean
		JCASSERT(m_temp_buf);
		memset(m_temp_buf, (BYTE)(m_clean_pattern), m_temp_len);
		WriteTestPattern(m_dev, m_trace, m_temp_buf, _T("cleaning"));
	}

	if (m_reset > 0)
	{
		_tprintf_s(_T("resetting device..."));
		PowerCycle(m_dev, m_reset * 1000);
		_tprintf_s(_T("done\n"));
	}

	if (compare)	_tprintf_s(_T("copy compare test passed.\n"));
	else			_tprintf_s(_T("copy compare test failed.\n"));
	return (compare)?(0):(1);
}

int CQCTestApp::Initialize(void)
{
	return 1;
}

void CQCTestApp::CleanUp(void)
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

