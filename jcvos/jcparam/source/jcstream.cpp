#include "stdafx.h"
#include "../include/jcstream.h"


#define		BUF_SIZE		4096
#define		BUF_EXT			1024

LOCAL_LOGGER_ENABLE(_T("jcparam.stream"), LOGGER_LEVEL_ERROR);

///////////////////////////////////////////////////////////////////////////////
// -- string stream
void CreateStreamString(CJCStringT * str, jcparam::IJCStream * & stream)
{
	JCASSERT(NULL == stream);
	stream = static_cast<jcparam::IJCStream*>(new CStreamString(str));
}

CStreamString::CStreamString(CJCStringT * str)
: m_auto_del(false), m_str(str)
{
	JCASSERT(str);
}

CStreamString::~CStreamString(void)
{
	if (m_auto_del) delete m_str;
}

void CStreamString::Put(wchar_t ch)
{
	m_str->push_back(ch);
}

void CStreamString::Put(const wchar_t * str, JCSIZE len)
{
	m_str->append(str, len);
}

void CStreamString::Format(LPCTSTR fmt, ...)
{
	TCHAR buf[256];
	va_list argptr;
	va_start(argptr, fmt);

	stdext::jc_vsprintf(buf, 256, fmt, argptr); 
}

///////////////////////////////////////////////////////////////////////////////
// -- file stream
LOG_CLASS_SIZE(CStreamFile)

const wchar_t * CStreamFile::STREAM_EOF = (wchar_t*) -1;

void CreateStreamFile(const CJCStringT & file_name, jcparam::READ_WRITE rd, jcparam::IJCStream * & stream)
{
	JCASSERT(NULL == stream);
	FILE * file = NULL;
	if (rd == jcparam::READ)	stdext::jc_fopen(&file, file_name.c_str(), _T("r"));
	else						stdext::jc_fopen(&file, file_name.c_str(), _T("w+"));

	if (NULL == file) THROW_ERROR(ERR_PARAMETER, _T("failure on openning file %s"), file_name.c_str() );
	stream = static_cast<jcparam::IJCStream*>(new CStreamFile(rd, file, file_name));
}

CStreamFile::CStreamFile(jcparam::READ_WRITE rd, FILE * file, const CJCStringT & file_name)
	: m_file(file), m_buf(NULL), m_first(NULL), m_last(NULL)
	, m_rd(rd), m_f_buf(NULL), m_file_name(file_name)
{
	JCASSERT(m_file);
	if (rd == jcparam::WRITE)	m_f_buf = new char[BUF_SIZE], m_f_last = m_f_buf;
	else						m_buf = new wchar_t[BUF_SIZE + BUF_EXT], m_first = m_buf, m_last = m_buf;
}

CStreamFile::CStreamFile(const CJCStringT & file_name)
	: m_file(NULL), m_buf(NULL), m_first(NULL), m_last(NULL)
	, m_f_buf(NULL), m_file_name(file_name)
{
	stdext::jc_fopen(&m_file, file_name.c_str(), _T("r"));
	if (NULL == m_file)	{}	// ERROR handling

	m_buf = new wchar_t[BUF_SIZE + BUF_EXT];
	m_first = m_buf;
	m_last = m_buf;
}

CStreamFile::~CStreamFile(void)
{
	if (m_file)		fclose(m_file);
	delete [] m_buf;
	delete [] m_f_buf;
}

void CStreamFile::Put(wchar_t ch) 
{
	char uch = (char)(ch);
	fwrite(&uch, 1, sizeof(char), m_file);
}

void CStreamFile::Put(const wchar_t * str, JCSIZE len) 
{
	JCSIZE u_len = stdext::UnicodeToUtf8(m_f_buf, (JCSIZE)(BUF_SIZE - (m_f_last - m_f_buf)), str, len);
	fwrite(m_f_buf, 1, sizeof(char) * u_len, m_file);
	fflush(m_file);
}

wchar_t CStreamFile::Get(void)
{
	if (m_first == m_last)	ReadFromFile();
	if (m_first == STREAM_EOF)
	{
		CEofException eof;
		throw eof;
	}
	wchar_t ch = *m_first;
	m_first ++;
	return ch;
}

JCSIZE CStreamFile::Get(wchar_t * str, JCSIZE len)
{
	JCSIZE read_len = 0;
	
	while (len > 0)
	{
		if (m_first >= m_last)	ReadFromFile();
		if (m_first == STREAM_EOF)		return read_len;
		JCSIZE ll = min(len, (JCSIZE)(m_last-m_first) );
		wmemcpy_s(str, len, m_first, ll);
		m_first += ll;
		read_len += ll;
		str += ll;
		len -= ll;
	}
	return read_len;
}

void CStreamFile::Format(LPCTSTR fmt, ...) 
{
	va_list argptr;
	va_start(argptr, fmt);
	
	_vftprintf_s(m_file, fmt, argptr);
}


bool CStreamFile::ReadFromFile(void)
{
	if (feof(m_file) )
	{
		m_last = const_cast<wchar_t*>(STREAM_EOF);
		m_first = const_cast<wchar_t*>(STREAM_EOF);
		return false;
	}
	stdext::auto_array<char>	file_buf(BUF_SIZE);
	JCSIZE read_size = (JCSIZE)fread(file_buf, 1, BUF_SIZE, m_file);
	JCSIZE conv_size = stdext::Utf8ToUnicode(m_first, BUF_SIZE, file_buf, read_size);
	m_last = m_first + conv_size;
	return true;
}


///////////////////////////////////////////////////////////////////////////////
// -- stream stdout
void CreateStreamStdout(jcparam::IJCStream * & stream)
{
	JCASSERT(stream == NULL);
	stream = new CStreamStdOut;
}

///////////////////////////////////////////////////////////////////////////////
// -- binary file stream
LOG_CLASS_SIZE(CStreamBinaryFile)
void CreateStreamBinaryFile(const CJCStringT & file_name, jcparam::READ_WRITE rd, jcparam::IJCStream * & stream)
{
	JCASSERT(NULL == stream);
	FILE * file = NULL;
	if (rd == jcparam::READ)	stdext::jc_fopen(&file, file_name.c_str(), _T("rb"));
	else						stdext::jc_fopen(&file, file_name.c_str(), _T("w+b"));

	if (NULL == file) THROW_ERROR(ERR_PARAMETER, _T("failure on openning file %s"), file_name.c_str() );
	stream = static_cast<jcparam::IJCStream*>(new CStreamBinaryFile(rd, file, file_name));
}

void CreateStreamBinaryFile(FILE * file, jcparam::READ_WRITE rd, jcparam::IJCStream * & stream)
{
	JCASSERT(NULL == stream);
	stream = static_cast<jcparam::IJCStream*>(new CStreamBinaryFile(rd, file, _T("")));
}


CStreamBinaryFile::CStreamBinaryFile(jcparam::READ_WRITE rd, FILE * file, const CJCStringT & file_name)
	: m_file(file), m_rd(rd), m_file_name(file_name)
{
	JCASSERT(m_file);
}

CStreamBinaryFile::~CStreamBinaryFile(void)
{
	if (m_file)		fclose(m_file);
}

void CStreamBinaryFile::Put(wchar_t ch) 
{
	fwrite(&ch, 1, sizeof(wchar_t), m_file);
}

void CStreamBinaryFile::Put(const wchar_t * str, JCSIZE len) 
{
	fwrite(str, 1, sizeof(wchar_t) * len, m_file);
}

wchar_t CStreamBinaryFile::Get(void)
{
	wchar_t ch;
	fread(&ch, 1, sizeof(wchar_t), m_file);
	return ch;
}

JCSIZE CStreamBinaryFile::Get(wchar_t * str, JCSIZE len)
{
	fread(str, 1, sizeof(wchar_t), m_file);
	return len;
}

///////////////////////////////////////////////////////////////////////////////
// -- CIteratorFile

CReadIterator::CReadIterator(jcparam::IJCStream* stream)
	: m_stream(stream)
{
	JCASSERT(m_stream);
	m_cur = m_stream->Get();
}

CReadIterator::~CReadIterator(void)
{
}


CReadIterator & CReadIterator::operator ++ (void)
{
	if (m_stream->IsEof())
		throw CEofException();
	m_cur = m_stream->Get();
	return (*this);
}

CReadIterator CReadIterator::operator ++ (int)
{ 
	CReadIterator tmp(*this); 	
	m_cur = m_stream->Get();
	return tmp;	
}

wchar_t & CReadIterator::operator * (void)
{
	return (m_cur);
}

bool CReadIterator::operator == (const CReadIterator *it)
{
	if (m_stream->IsEof() )	return true;
	else return false;
}

