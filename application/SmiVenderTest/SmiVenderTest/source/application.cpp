﻿#include "stdafx.h"
// CSvtApplication 同时继承 Application和jcscript::IPlugin两个Interface。
// 此源文件主要实现Application接口部分，包括
//  1) 命令解析
//  2) Plugin管理

#include <stdio.h>
#include <jcparam.h>

LOCAL_LOGGER_ENABLE(_T("CSvtApp"), LOGGER_LEVEL_DEBUGINFO);

#include "application.h"

///////////////////////////////////////////////////////////////////////////////
//-- application
#ifdef _DEBUG
	// 输出编译的中间结果
const TCHAR CSvtApplication::INDENTATION[16] = _T("\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"); 
#endif


CSvtApplication * CSvtApplication::m_app = NULL;

CSvtApplication::CSvtApplication(void)
	: m_plugin_manager(NULL)
	, m_cur_script(NULL)
{
	JCASSERT(m_app == NULL);
	m_app = this;

	m_plugin_manager = new CPluginManager;
}


CSvtApplication::~CSvtApplication(void)
{
	Cleanup();
}

static void PassArgument(LPCTSTR name, jcparam::CArguSet & from, jcparam::CArguSet & to)
{
	jcparam::IValue * val = NULL;
	from.GetSubValue(name, val);
	if ( val ) 
	{
		to.AddValue(name, val);
		val->Release();
	}
}


int CSvtApplication::Initialize(LPCTSTR cmd)
{
	ProcessCommandLine(cmd);
	bool has_help = false;
	m_arg_set.GetValT(_T("help"), has_help);
	if (has_help)
	{
		ShowHelpMessage();
		throw jcscript::CExitException();
	}

	jcparam::CArguSet	argu_scan;

	PassArgument(_T("list"), m_arg_set, argu_scan);
	PassArgument(_T("driver"), m_arg_set, argu_scan);
	PassArgument(_T("controller"), m_arg_set, argu_scan);


	CSmiRecognizer::CreateDummyDevice(m_dev);

	jcparam::IValue * outvar = NULL;
	ScanDevice(argu_scan, NULL, outvar);
	if (outvar) outvar->Release();
#ifdef _DEBUG
	// 输出编译的中间结果
	m_compile_log = NULL;
	_tfopen_s(&m_compile_log, _T("compile.log"), _T("w+"));
	if (NULL == m_compile_log) 
		THROW_ERROR(ERR_APP, _T("Open file compile.log failed"));
#endif
	return 1;
}

int CSvtApplication::Cleanup(void)
{
	if (m_dev)
	{
		ISmiDevice * tmp = m_dev;
		m_dev = NULL;
		tmp->Release();
	}	
	if (m_cur_script)
	{
		m_cur_script->Release();
		m_cur_script = NULL;
	}
	if (m_plugin_manager) 
	{
		m_plugin_manager->Release();
		m_plugin_manager = NULL;
	}
#ifdef _DEBUG
	// 输出编译的中间结果
	fclose(m_compile_log);
#endif
	return 0;
}

void CSvtApplication::SetDevice(const CJCStringT & name, ISmiDevice * dev)
{
	m_device_name = name;
	if (m_dev)	m_dev->Release();
	// TODO: Inform all plugin to change device
	m_dev = dev;
	m_dev->AddRef();
}

class CLOSE_C_FILE
{
public:
	static inline void DoCloseHandle(FILE * file)
	{ if (file) fclose(file); }
};

typedef stdext::auto_handle<FILE *, CLOSE_C_FILE>	CAutoFile;


bool CSvtApplication::RunScript(LPCTSTR file_name)
{
	LOG_STACK_TRACE();
	CAutoFile	script_file(NULL);	
	try
	{
		// Run command in "script"
		JCASSERT(file_name)

		LOG_DEBUG(_T("Processing script %s"), file_name);
		stdext::jc_printf(_T("Running script %s ...\n"), file_name);
		
		//FILE * script_file = NULL;
		if ( _tfopen_s(&script_file, file_name, _T("r") ) != 0)
			THROW_ERROR(ERR_PARAMETER, _T("failure on openning script %s"), file_name);

		jcscript::CSyntaxParser	syntax_parser(
			static_cast<jcscript::IPluginContainer*>(m_plugin_manager),
			static_cast<jcscript::LSyntaxErrorHandler*>(this) );
		syntax_parser.Source(script_file);

		if (m_cur_script)	m_cur_script->Release(), m_cur_script= NULL;

		syntax_parser.MatchScript(m_cur_script);
#ifdef _DEBUG
		// 输出编译的中间结果
		stdext::jc_fprintf(m_compile_log, _T("compiling script file %s.\n"), file_name);
		m_cur_script->DebugOutput(INDENTATION + 15, m_compile_log);
		stdext::jc_fprintf(m_compile_log, _T("\n"));
		fflush(m_compile_log);
#endif	
		if ( !syntax_parser.GetError() )
		{
			LOG_DEBUG(_T("Start invoking script") );
			m_cur_script->Invoke();
			LOG_DEBUG(_T("Finished invoking script") );
		}

	}
	catch (std::exception & err)
	{
		printf("%s\r\n", err.what());
		return false;
	}
	return true;
}

bool CSvtApplication::ParseCommand(LPCTSTR first, LPCTSTR last)
{
	LOG_STACK_TRACE();
	try
	{
		jcscript::CSyntaxParser	syntax_parser(
			static_cast<jcscript::IPluginContainer*>(m_plugin_manager),
			static_cast<jcscript::LSyntaxErrorHandler*>(this) );
		syntax_parser.Parse(first, last);

		if (m_cur_script)	m_cur_script->Release(),	m_cur_script= NULL;
		syntax_parser.MatchScript(m_cur_script);
#ifdef _DEBUG
		// 输出编译的中间结果
		stdext::jc_fprintf(m_compile_log, _T("compiling command \"%s\""), first);
		m_cur_script->DebugOutput(INDENTATION + 15, m_compile_log);
		stdext::jc_fprintf(m_compile_log, _T("\n"));
		fflush(m_compile_log);
#endif
		if ( !syntax_parser.GetError() )
		{
			LOG_DEBUG(_T("Start invoking command") );
			m_cur_script->Invoke();
			LOG_DEBUG(_T("Finished invoking command") );
		}
	}
	catch (std::exception & err)
	{
		printf("%s\r\n", err.what());
	}
	return true;
}

int CSvtApplication::Run(void)
{
	LOG_STACK_TRACE();

	try
	{
		// processing script in command line
		stdext::auto_array<TCHAR> line_buf(MAX_LINE_BUF);
		CJCStringT script_file_name = DEFAULT_SCRIPT;
		m_arg_set.GetValT(_T("script"), script_file_name);
		if ( !script_file_name.empty() )	RunScript(script_file_name.c_str() );

		// Running
		while (1)
		{
			_tprintf(_T(">"));
			// one line command parse
			_getts_s(line_buf, MAX_LINE_BUF-1);
			JCSIZE len = _tcslen(line_buf);
			JCASSERT(len < MAX_LINE_BUF -1);
			line_buf[len++] = _T('\n');
			line_buf[len] = 0;

			LPCTSTR first = line_buf, last = line_buf + len;
			ParseCommand(first, last);
		}
	}
	catch(jcscript::CExitException)
	{
		// Just use exception to exit loop. Do nothing
		return 0;
	}
	return 0;
}




/*
int CSvtApplication::Run(void)
{
	LOG_STACK_TRACE();
	static const JCSIZE MAX_LINE_BUF=1024;
	try
	{
		bool keep_life = false;
		m_arg_set.GetValT(_T("keep_life"), keep_life);

		// Run command in "invoke" option
		CJCStringT	invoke_cmd;
		m_arg_set.GetValT(_T("invoke"), invoke_cmd);
		if ( !invoke_cmd.empty() )
		{
			LOG_DEBUG(_T("Processing in line command %s"), invoke_cmd.c_str());
			LPCTSTR first = invoke_cmd.c_str(), last = first + _tcslen(first);
			ParseCommand(first, last);
			if ( !keep_life ) return 0;
		}

		// Run command in "script"
		stdext::auto_array<TCHAR> line_buf(MAX_LINE_BUF);
		CJCStringT script_file_name;
		m_arg_set.GetValT(_T("script"), script_file_name);
		if ( !script_file_name.empty() )
		{
			LOG_DEBUG(_T("Processing script %s"), script_file_name.c_str());
			_tprintf(_T("Running script %s ...\n"), script_file_name.c_str());
			FILE * script_file = NULL;
			if ( _tfopen_s(&script_file, script_file_name.c_str(), _T("r") ) == 0)
			{
				while (1)
				{
					if ( _fgetts(line_buf, MAX_LINE_BUF, script_file) == NULL) break;
					_tprintf(line_buf);
					LPCTSTR first = line_buf, last = line_buf + _tcslen(line_buf);
					ParseCommand(first, last);
					_tprintf(_T("\n"));
				}
				fclose(script_file);

				if ( !keep_life ) 
				{
					_tprintf(_T("Finished running script. Press any key to exit..."));
					getchar();
					return 0;
				}
			}
			else
			{
				_tprintf(_T("Cannot open script file %s"), script_file_name.c_str());
			}
		}

		// Running

		while (1)
		{
			_tprintf(_T(">"));
			_getts_s(line_buf, MAX_LINE_BUF-1);
			JCSIZE len = _tcslen(line_buf);
			JCASSERT(len < MAX_LINE_BUF -1);
			line_buf[len++] = _T('\n');
			line_buf[len] = 0;

			LPCTSTR first = line_buf, last = line_buf + len;
			ParseCommand(first, last);
		}
	}
	catch(jcscript::CExitException)
	{
		// Just use exception to exit loop. Do nothing
		return 0;
	}
	return 0;
}
*/




void CSvtApplication::GetDevice(ISmiDevice * & dev)
{
	JCASSERT(NULL == dev);
	dev = m_dev;
	if (dev) dev->AddRef();
}

//#define DUMMY_DEVICE

bool CSvtApplication::DummyDevice(void)
{
	LOG_STACK_TRACE();
// TODO :
	//if (m_dev)
	//{
	//	m_device_name.clear();
	//	m_dev->Release();
	//	m_dev = NULL;
	//}

	//IStorageDevice * storage = NULL;
	//CTestStorageDevice::OpenDevice(_T(""), storage);
	//m_dev = CTestSmiDevice::CreateDevice(storage);
	//storage->Release();
	//m_device_name = _T("DummyDevice");
	//_tprintf(_T("Using dummy device\r\n"));
	return true;
}


const jcparam::CParameterDefinition CSvtApplication::m_cmd_line_parser( jcparam::CParameterDefinition::RULE()
	(_T("list"), _T('l'), jcparam::VT_STRING, _T("Give a scan list, like C~Z0~9. Pass this argument to scandev.") )
	(_T("driver"), _T('d'), jcparam::VT_STRING, _T("Force using specified driver. Pass this argument to scandev.") )
	(_T("controller"), _T('c'), jcparam::VT_STRING, _T("Force using specified controller. Pass this argument to scandev.") )
	(_T("invoke"), _T('i'), jcparam::VT_STRING, _T("Invoke following command after start up.") )
	(_T("script"), _T('s'), jcparam::VT_STRING, _T("Run commands in the specifide script file.") )
	(_T("help"), _T('h'), jcparam::VT_BOOL, _T("Show this help message.") )
	//(_T("#testa"), _T('a'), jcparam::VT_BOOL, _T("Test for hided option name") )
	//(_T("testb"), 0, jcparam::VT_BOOL, _T("Test for hided abbrev") )
	);


bool CSvtApplication::ProcessCommandLine(LPCTSTR cmd)
{
	jcparam::CCmdLineParser::ParseCommandLine(m_cmd_line_parser, cmd, m_arg_set);
	//m_cmd_line_parser.ParseCommandLine(cmd, m_arg_set);
	return true;
}

void CSvtApplication::ShowHelpMessage() const
{
	_tprintf(_T("SMI Vender Test\r\n"));
	m_cmd_line_parser.OutputHelpString(stdout);
}
	
void CSvtApplication::ReportProgress(LPCTSTR msg, int percent)
{
	static int old_percent = 0;
	if ( !msg || percent < 0)
	{
		// reset
		old_percent = 0;
		return;
	}
	
	if (percent > old_percent)
	{
		_tprintf(_T("\r%s%d%% ... "), msg, percent);
		old_percent = percent;
	}
}

bool CSvtApplication::GetDefaultVariable(const CJCStringT & name, jcparam::IValue * & var)
{
	JCASSERT(NULL == var);
	bool match = false;

	if ( name == _T("MAX_BLOCKS") )
	{
		CCardInfo info;
		m_dev->GetCardInfo(info);
		var = jcparam::CTypedValue<UINT>::Create(info.m_f_block_num);
	}
	else if ( name == _T("MAX_LBAS") )
	{
		FILESIZE max_lba = 0;
		stdext::auto_cif<IStorageDevice>	storage;
		m_dev->QueryInterface(IF_NAME_STORAGE_DEVICE, storage);
		if ( storage.valid() )
		{
			max_lba = storage->GetCapacity();
		}
		var = jcparam::CTypedValue<FILESIZE>::Create(max_lba);
	}
	else return false;

	return true;

}

void CSvtApplication::OnError(JCSIZE line, JCSIZE column, LPCTSTR msg)
{
	stdext::jc_printf(_T("syntax error! line:%d, col:%d, %s\n"), line, column, msg);
}

///////////////////////////////////////////////////////////////////////////////
enum CHAR_TYPE
{
	CT_UNKNOW = 0,
	CT_WORD =		0x00000006,
	CT_NUMBER =		0x00000004,
	CT_ALPHABET =	0x00000002,
	CT_HYPHEN =		0x00000010,
};

static CHAR_TYPE CharType(TCHAR &ch)
{
	static const TCHAR LOWERCASE = _T('a') - _T('A');
	CHAR_TYPE type = CT_UNKNOW;
	if ( _T('a') <= ch &&  ch <= _T('z') )		ch -= LOWERCASE, type = CT_ALPHABET;
	else if ( _T('A') <= ch && ch <= _T('Z') )	type = CT_ALPHABET;
	else if ( _T('0') <= ch && ch <= _T('9') )	type = CT_NUMBER;
	else if ( _T('-') == ch || _T('~') == ch)	type = CT_HYPHEN;
	else THROW_ERROR(ERR_PARAMETER, _T("Unknow character %c"), ch);
	return type;
}

static void AddDeviceName(std::vector<CJCStringT> & list, TCHAR start, TCHAR end, CHAR_TYPE type)
{
	TCHAR device_name[32];
	JCASSERT(type & CT_WORD);
	JCASSERT(start);
	//if ( end < start) THROW_ERROR(ERR_PARAMETER, _T("Uncorrect letter range from %c to %c"), start, end);
	if (type & CT_ALPHABET)
	{
		if (0 == end ) end = _T('Z');
		JCASSERT( end - start < 26)
		if ( end < start) THROW_ERROR(ERR_PARAMETER, _T("Uncorrect letter range from %c to %c"), start, end);
		for (TCHAR cc = start; cc <= end; ++cc)
		{
			_stprintf_s(device_name, _T("\\\\.\\%c:"), cc);
			list.push_back(CJCStringT(device_name));
		}
	}
	else
	{
		if (0 == end ) end = _T('9');
		JCASSERT(type & CT_NUMBER);
		JCASSERT( end - start < 10);
		if ( end < start) THROW_ERROR(ERR_PARAMETER, _T("Uncorrect letter range from %c to %c"), start, end);
		for (TCHAR cc = start; cc<= end ; ++cc)
		{
			_stprintf_s(device_name, _T("\\\\.\\PhysicalDrive%c"), cc);
			list.push_back(CJCStringT(device_name));
		}	
	}
}

bool CSvtApplication::ScanDevice(jcparam::CArguSet & argu, jcparam::IValue *, jcparam::IValue * &)
{
	LOG_STACK_TRACE();

	std::vector<CJCStringT>	try_list;
	bool found = false;
	
	//ClearDevice();

	CJCStringT	str_list(_T("0~9")), force_storage, force_device;
	argu.GetValT(_T("list"), str_list);
	argu.GetValT(_T("driver"), force_storage);
	argu.GetValT(_T("controller"), force_device);

	if ( force_storage == _T("DUMMY") ) return true;

	enum STATUS
	{
		ST_START, ST_STARTED, ST_WAITING_END,
	};
	// Create a scan list
	LPCTSTR list = str_list.c_str();
	TCHAR start = 0, end = 0;
	CHAR_TYPE start_type = CT_UNKNOW;
	STATUS status = ST_START;
	while ( *list )
	{
		TCHAR ch = *list;
		CHAR_TYPE type = CharType(ch);
		switch (status)
		{
		case ST_START:
			if ( type & CT_WORD ) 
				status = ST_STARTED, end = ch, start = ch, start_type = type;
			else THROW_ERROR(ERR_PARAMETER, _T("Missing start character"));
			break;

		case ST_STARTED:
			if ( type & CT_WORD)
			{
				// process current letter
				AddDeviceName(try_list, start, start, start_type);
				end = ch;
				start = ch;
				start_type = type;
			}
			else if ( type & CT_HYPHEN)		status = ST_WAITING_END, end = 0;
			break;
		case ST_WAITING_END:
			if ( type & CT_WORD)
			{
				JCASSERT(start);
				JCASSERT(start_type);
				if ( type != start_type)
				{
					AddDeviceName(try_list, start, 0, start_type);
					start = ch;
					start_type = type;
					status = ST_STARTED;
				}
				else
				{
					AddDeviceName(try_list, start, ch, start_type);
					start = 0;
					status = ST_START;
				}
			}
			else	THROW_ERROR(ERR_PARAMETER, _T("Unexpected hyphen"));
		}
		list ++;
	}
	if (ST_START != status) 		AddDeviceName(try_list, start, end, start_type);


	std::vector<CJCStringT>::iterator it, endit=try_list.end();

	for (it = try_list.begin(); it != endit; ++it)
	{
		LOG_DEBUG(_T("Trying device %s"), it->c_str() )
		ISmiDevice * smi_dev = NULL;
		bool br = CSmiRecognizer::RecognizeDevice(it->c_str(), smi_dev, force_storage, force_device);
		if (br && smi_dev)
		{
			const CJCStringT & name (*it);
			SetDevice(name, smi_dev);
			smi_dev->Release();
			_tprintf(_T("Found device %s.\r\n"), name.c_str() );
			found = true;

			LOG_DEBUG(_T("Found device %s"), name.c_str() );
			break;
		}
	}

	if ( ! found) _tprintf(_T("Cannot find SMI device!\r\n"));

	return true;
}