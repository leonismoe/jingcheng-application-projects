// driver_tester.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include <vld.h>
#include <stdext.h>
#include <jcapp.h>

LOCAL_LOGGER_ENABLE(_T("driver_tester"), LOGGER_LEVEL_DEBUGINFO);

#include "../CoreMntLib/include/mntSyncMountmanager.h"
#include "../CoreMntLib/include/mntImage.h"

class CDriverTestApp : public jcapp::CJCAppSupport<jcapp::AppArguSupport>
{
public:
	CDriverTestApp(void);
public:
	virtual int Initialize(void);
	virtual int Run(void);
	virtual void CleanUp(void);
	virtual LPCTSTR AppDescription(void) const {
		return _T("");
	};

protected:
	void SequentialWriteTest(IImage * img, FILESIZE start, FILESIZE end, JCSIZE data_sec, USHORT signe); 
	void SequentialReadCompareTest(IImage * img, FILESIZE start, FILESIZE end, JCSIZE data_sec, USHORT signe); 
	void RandomWriteCompareTest(IImage * img, UINT seed); 

public:
	static const TCHAR LOG_CONFIG_FN[];
	CJCStringT	m_driver;
	CJCStringT	m_config;		// file name of configuration.


protected:
	//HMODULE				m_driver_module;
};

const TCHAR CDriverTestApp::LOG_CONFIG_FN[] = _T("driver_test.cfg");

typedef jcapp::CJCApp<CDriverTestApp>	CApplication;
#define _class_name_	CApplication
CApplication _app;

BEGIN_ARGU_DEF_TABLE()
	ARGU_DEF_ITEM(_T("driver"),		_T('p'), CJCStringT,	m_driver,		_T("load user mode driver.") )
	ARGU_DEF_ITEM(_T("config"),		_T('g'), CJCStringT,	m_config,		_T("configuration file name for driver.") )
	
END_ARGU_DEF_TABLE()

CDriverTestApp::CDriverTestApp(void)
{
}


int CDriverTestApp::Initialize(void)
{
	return 1;
}


#include <time.h>


void FillData(BYTE * buf, JCSIZE buf_len, FILESIZE lba, USHORT signe)
{	// 填写测试pattern：开头8字节为LBA，接着4字节为seed。然后是根据seed产生的随机数
	USHORT * bb = (USHORT*)buf;
	memcpy_s(buf, 8, &lba, 8);	
	bb += sizeof(FILESIZE) / sizeof(USHORT);

	USHORT seed = (USHORT)(time(NULL) & 0xFFFF);
	srand(seed);

	for (JCSIZE ii=8; ii<buf_len; ii += sizeof(USHORT), ++bb )
	{
		*bb = (seed ^ signe);
		seed = (USHORT)(rand() & 0xFFFF);
	}
}

JCSIZE VerifyData(BYTE * buf, JCSIZE buf_len, FILESIZE lba, USHORT signe)
{
	USHORT * bb = (USHORT*)buf;
	if (memcmp(buf, &lba, 8) != 0)	return 0;
	bb += sizeof(FILESIZE) / sizeof(USHORT);

	USHORT seed = (*bb ^ signe);
	srand(seed);
	for (JCSIZE ii = 8; ii<buf_len; ii+= sizeof(USHORT), ++bb)
	{
		if (*bb != (seed^signe) ) return ii;
		seed = (USHORT)(rand() & 0xFFFF);
	}
	return UINT_MAX;
}

int CDriverTestApp::Run(void)
{
	CSyncMountManager	mount_manager;

	CJCStringT app_path;
	GetAppPath(app_path);

	CJCStringT drv_path;
	drv_path = app_path + _T("\\") + m_driver + _T(".dll");

	stdext::auto_interface<jcparam::CParamSet> param;
	jcparam::CParamSet::Create(param);
	jcparam::IValue * val = NULL;
	val = jcparam::CTypedValue<CJCStringT>::Create(m_config);
	param->SetSubValue(_T("CONFIG"), val);
	val->Release(); val = NULL;

	stdext::auto_interface<IImage>	img;
	_tprintf(_T("Loading user driver %s ..."), drv_path.c_str() );
	mount_manager.LoadUserModeDriver(drv_path, _T("vendor_test"), param, img);
	JCASSERT( img.valid() );

	ITestAuditPort * test_port = img.d_cast<ITestAuditPort*>();
	if (test_port)
	{
		test_port->AddRef();
		test_port->SendEvent();
		test_port->Release();
	}

	_tprintf(_T("Succeded\n"));

	// internal test for data verify
	stdext::auto_array<BYTE> _buf( SECTOR_SIZE );
	BYTE * buf = _buf;
	for (JCSIZE ss = 0; ss < 8; ss++)
	{
		FillData(buf, SECTOR_SIZE, ss, ss + 200);
		JCSIZE err = VerifyData(buf, SECTOR_SIZE, ss, ss + 200); 
		if (err < UINT_MAX) _tprintf(_T("error in verify\n"));
	}

	USHORT signe = 100;
	_tprintf(_T("pattern signature = %d\n"), signe);
	SequentialWriteTest(img, 0, 1000, 5, signe);
	img->FlushCache();
	SequentialReadCompareTest(img, 0, 1000, 10, signe);
	return 0;
}

void CDriverTestApp::SequentialWriteTest(IImage * img, FILESIZE start, FILESIZE end, JCSIZE data_sec, USHORT signe)
{
	stdext::auto_array<BYTE> _buf( SECTOR_TO_BYTE(data_sec) );
	BYTE * buf = _buf;
	
	for (; start < end; start += data_sec)
	{
		for (JCSIZE ss = 0; ss < data_sec; ss++)
			FillData(buf + SECTOR_TO_BYTE(ss), SECTOR_SIZE, start + ss, signe);

		img->Write(buf, start, data_sec);
	}
}

void CDriverTestApp::SequentialReadCompareTest(IImage * img, FILESIZE start, FILESIZE end, JCSIZE data_sec, USHORT signe)
{
	stdext::auto_array<BYTE> _buf( SECTOR_TO_BYTE(data_sec) );
	BYTE * buf = _buf;
	for (; start < end; start += data_sec)
	{
		img->Read(buf, start, data_sec);
		for (JCSIZE ss = 0; ss < data_sec; ss++)
		{
			JCSIZE err = VerifyData(buf + SECTOR_TO_BYTE(ss), SECTOR_SIZE, start + ss, signe);
			//if (err < UINT_MAX) LOG_ERROR(_T("comapre error @ lba:0x%I64X, offset: %d"), start+ss, err);
			if (err < UINT_MAX)
			{
				LOG_ERROR(_T("comapre error @ lba:0x%I64X, offset: %d"), start+ss, err);
				_tprintf(_T("comapre error @ lba:0x%I64X, offset: %d\n"), start+ss, err);
			}
		}
	}
}

void CDriverTestApp::RandomWriteCompareTest(IImage * img, UINT seed)
{
}

void CDriverTestApp::CleanUp(void)
{
	//if (m_driver_module) FreeLibrary(m_driver_module);
}


int _tmain(int argc, _TCHAR* argv[])
{
	return jcapp::local_main(argc, argv);
}