// syntax_test.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <vld.h>

#include <script_engine.h>

#include "./syntax_test_plugin.h"

LOCAL_LOGGER_ENABLE(_T("syntax_test"), LOGGER_LEVEL_DEBUGINFO);

static CContainerSyntest g_plugin_container;
bool ParseCommand(LPCTSTR first, LPCTSTR last, IPluginContainer * container);

const jcparam::CArguDefList line_parser( jcparam::CArguDefList::RULE()
	(_T("input"), _T('i'), jcparam::VT_STRING, _T("Input file for test.") )
  );

class CSyntestErr : virtual public jcscript::LSyntaxErrorHandler
{
public:
	CSyntestErr(FILE * outfile)
		: m_out(outfile)
	{
	}

	virtual void OnError(JCSIZE line, JCSIZE column, LPCTSTR msg)
	{
		stdext::jc_fprintf(m_out, _T("syntax error! line:%d, col:%d, %s\n"), line, column, msg);
	}

protected:
	FILE * m_out;

};

FILE *outfile = NULL;

LOGGER_TO_FILE(O, _T("syntax_test.log"), 
	//CJCLogger::COL_COMPNENT_NAME | 
	CJCLogger::COL_FUNCTION_NAME | 
	CJCLogger::COL_TIME_STAMP, 0);

int _tmain(int argc, _TCHAR* argv[])
{
	LOG_STACK_TRACE();

	FILE * config_file = NULL;
	_tfopen_s(&config_file, _T("jclog.cfg"), _T("r"));
	if (config_file)
	{
		CJCLogger::Instance()->Configurate(config_file);
		fclose(config_file);
	}


//	FILE * outfile = stdout;
	jcparam::CArguSet		arg_set;		// argument for process command line
	jcparam::CCmdLineParser::ParseCommandLine(
		line_parser, GetCommandLine(), arg_set);
	//line_parser.ParseCommandLine(GetCommandLine(), arg_set);

	FILE * infile = stdin;
	outfile = stdout;
	CJCStringT str_input;

	arg_set.GetValT(_T("input"), str_input);
	if ( !str_input.empty() )
	{
		_tfopen_s(&infile, str_input.c_str(), _T("r") );
		if (NULL == infile) THROW_ERROR(ERR_APP, _T("Open file %s failed"), str_input.c_str());

		CJCStringT str_output = str_input + _T(".out");
		_tfopen_s(&outfile, str_output.c_str(), _T("w+"));
		if (NULL == outfile) THROW_ERROR(ERR_APP, _T("Open file %s failed"), str_output.c_str());
	}

	CSyntestErr err_handler(outfile);

	const TCHAR INDENTATION[16] = _T("\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t");

#if 0	
	static const JCSIZE MAX_STR_BUF=1024;
	TCHAR line_buf[MAX_STR_BUF];
	while (1)
	{
		if ( _fgetts(line_buf, MAX_STR_BUF, infile) == NULL) break;
		LPCTSTR first = line_buf, last = line_buf + _tcslen(line_buf);
		_ftprintf(outfile, line_buf);
		_ftprintf(outfile, _T("begin parsing...\n"));

		CSyntaxParser	syntax_parser(static_cast<IPluginContainer*>(&g_plugin_container) , &err_handler );
		syntax_parser.Parse(first, last);
		stdext::auto_interface<IAtomOperate>	op;
		syntax_parser.MatchScript(op);
		JCASSERT(op);
		
		op->DebugOutput(INDENTATION + 15, outfile);

		_ftprintf(outfile, _T("\n\n"));
		_ftprintf(outfile, _T("finished\n\n"));
		fflush(outfile);
		op->Invoke();
	}
	fclose(outfile);

#else
	CSyntaxParser	syntax_parser(
		static_cast<IPluginContainer*>(&g_plugin_container), &err_handler);
	syntax_parser.Source(infile);
	stdext::auto_interface<IAtomOperate>	op;
	syntax_parser.MatchScript(op);
	op->DebugOutput(INDENTATION + 15, outfile);
	fclose(outfile);

	//op->Invoke();
#endif
	g_plugin_container.Delete();
	return 0;
}


