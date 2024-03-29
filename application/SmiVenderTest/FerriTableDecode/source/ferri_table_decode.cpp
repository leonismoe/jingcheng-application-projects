// ferri_table_decode.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include <BlockInfo.h>
#include <ata_trace.h>

#include "ferri_table_decode.h"

LOCAL_LOGGER_ENABLE(_T("table_decode"), LOGGER_LEVEL_DEBUGINFO);

const TCHAR CTableDecodeApp::LOG_CONFIG_FN[] = _T("cache_info.cfg");

typedef jcapp::CJCApp<CTableDecodeApp>	CApplication;
CApplication _app;

#define _class_name_	CApplication
BEGIN_ARGU_DEF_TABLE()
	//ARGU_DEF_ITEM(_T("driver"),		_T('p'), CJCStringT,	m_driver,		_T("load user mode driver.") )
	ARGU_DEF_ITEM(_T("cache_index"),_T('c'), CJCStringT,	m_cache_index_fn,	_T("cache index table.") )
	ARGU_DEF_ITEM(_T("config"),		_T('g'), CJCStringT,	m_config,		_T("configuration file name for driver.") )
	ARGU_DEF_ITEM(_T("dps"),		_T('d'), CJCStringT,	m_dps_fn,		_T("file name of dps") )
	ARGU_DEF_ITEM(_T("binary"),		_T('b'), CJCStringT,	m_data_fn,		_T("file name of binary data") )
	
END_ARGU_DEF_TABLE()

///////////////////////////////////////////////////////////////////////////////
//--
CHblockLink::CHblockLink(WORD page_per_block)
: m_link(NULL), m_hblock(0xFFFF), m_page_per_block(page_per_block), m_mother(0xFFFF)
{
	m_link = new FLASH_ADDRESS[m_page_per_block];
	memset(m_link, 0xFF, sizeof(FLASH_ADDRESS) * m_page_per_block);
}

CHblockLink::~CHblockLink(void)
{
	if (m_link) delete [] m_link;
}

void CHblockLink::Reset(void)
{
	m_hblock = 0xFFFF;
	m_mother = 0xFFFF;
	memset(m_link, 0xFF, sizeof(FLASH_ADDRESS) * m_page_per_block);
}

void CHblockLink::SetLinkTable(BYTE * tab, JCSIZE offset, WORD *cache_index_tab, JCSIZE index_size)
{
	JCASSERT(m_hblock != 0xFFFF);
	JCASSERT( (offset * m_page_per_block) % 4 == 0);
	JCASSERT(cache_index_tab);
	BYTE * cache_index_array = tab + offset * m_page_per_block;
	BYTE * cache_page_array1 = tab + 0x400 + offset * m_page_per_block;
	BYTE * cache_page_array2 = tab + 0x800 + (offset * m_page_per_block /4);

	WORD fpage_h;
	for (WORD pp = 0; pp < m_page_per_block; ++pp)
	{
		if ( (pp & 0x3) == 0)	fpage_h = cache_page_array2[pp >> 2];
		BYTE cache_index = cache_index_array[pp];
		JCASSERT(cache_index < index_size);

		if (cache_index == 0xFF)
		{	// data in mother
			m_link[pp].fblock = m_mother;
			m_link[pp].fpage = 0xFFFF;
		}
		else
		{	// data in cache
			BYTE fpage_l = cache_page_array1[pp];
#if 1
			// 如果fpage高位从右到左
			WORD fpage = (fpage_h & 0x3) << 8;
			fpage_h >>= 2;
#else
			// 如果fpage高位从左到右
			WORD fpage = (fpage_h & 0xC0) << 2;
			fpage_h <<= 2;
#endif
			//fpage |= fpage_l;
			fpage = fpage_l;
			m_link[pp].fblock = cache_index_tab[cache_index];
			m_link[pp].fpage = fpage;
		}
	}
}




///////////////////////////////////////////////////////////////////////////////
//--

#define PAGE_PER_BLOCK  256
#define GLOBAL_HPAGE(hb, hp)	(hb * PAGE_PER_BLOCK + hp)

CSubLinkTable::CSubLinkTable(void)
{
	//memset(m_mother_list, 0xFF, sizeof(WORD) * 4);
	memset(m_sub_table, 0xFF, sizeof(FLASH_ADDRESS) * HPAGE_PER_TABLE);
}

void CSubLinkTable::SetTable(BYTE * buf, JCSIZE buf_len)
{
	JCASSERT(buf);
	JCASSERT(buf_len >= TABLE_SIZE);

	BYTE * hblock_info = buf + 0x900;
// debug
	LOG_DEBUG(_T("hblock: %04X, %04X, %04X, %04X"), 
		MAKEWORD(hblock_info[1], hblock_info[0]),
		MAKEWORD(hblock_info[3], hblock_info[2]),
		MAKEWORD(hblock_info[5], hblock_info[4]),
		MAKEWORD(hblock_info[7], hblock_info[6])
	);
	DWORD start_hblock = MAKEWORD(hblock_info[1], hblock_info[0]);
	return;

	m_start_hpage = GLOBAL_HPAGE(start_hblock, 0);

	WORD mother_list[4];
	for (int ii = 0; ii < 4; ++ii)
	{
		mother_list[ii] = MAKEWORD(hblock_info[16 + 2*ii + 1], hblock_info[16 + 2*ii]);
	}

	BYTE * cache_index_array = buf;
	BYTE * cache_page_array1 = buf + 0x400;
	BYTE * cache_page_array2 = buf + 800;

	// fpage bit9:8的缓存
	WORD fpage_h;

	int index = 0;
	int hblock_per_table = HPAGE_PER_TABLE / PAGE_PER_BLOCK;
	for (int bb = 0; bb < hblock_per_table; ++bb)
	{
		for (int pp = 0; pp < PAGE_PER_BLOCK; ++pp)
		{
			if ( (index & 0x3) == 0)	fpage_h = cache_page_array2[index >> 2];
			BYTE cache_index = cache_index_array[index];

			if (cache_index == 0xFF)
			{	// data in mother
				m_sub_table[index].fblock = mother_list[bb];
				m_sub_table[index].fpage = 0xFFFF;
			}
			else
			{	// data in cache
				BYTE fpage_l = cache_page_array1[index];
#if 1
				// 如果fpage高位从右到左
				WORD fpage = (fpage_h & 0x3) << 8;
				fpage_h >>= 2;
#else
				// 如果fpage高位从左到右
				WORD fpage = (fpage_h & 0xC0) << 2;
				fpage_h <<= 2;
#endif
				fpage |= fpage_l;
				m_sub_table[index].fblock = cache_index;
				m_sub_table[index].fpage = fpage;
			}
			index ++;
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
//--

CL2PhTable::CL2PhTable(void)
: m_table(NULL), m_total_pages(0), m_hblock_num(0)
{
}

CL2PhTable::~CL2PhTable(void)
{
	if (m_table) delete [] m_table;
	//{
	//	for (JCSIZE ii = 0; ii < m_total_pages; ++ii)
	//	{
	//		if (m_table[ii]) delete m_table[ii];
	//	}
	//}
	//delete [] m_table;
}

void CL2PhTable::Initialize(WORD hblock_num, WORD page_per_block)
{
	m_hblock_num = hblock_num;
	m_total_pages = m_hblock_num * page_per_block;

	//m_sub_tab_num = hblock_num / hblock_per_table;
	//m_table = new P_SEGMENT[m_sub_tab_num];
	//memset(m_table, 0, sizeof(P_SEGMENT) * m_sub_tab_num);
	m_table = new FLASH_ADDRESS[m_total_pages];
	memset(m_table, 0xFF, sizeof(FLASH_ADDRESS) * m_total_pages);
}

void CL2PhTable::Merge(CHblockLink * hlink, FILE * outfile)
{
	JCASSERT(outfile);
	JCASSERT(hlink);
	WORD page_per_block = hlink->m_page_per_block;
	
	JCSIZE start_page = hlink->m_hblock * page_per_block;
	for (WORD pp = 0; pp < page_per_block; ++pp, ++start_page)
	{
		if ( !(m_table[start_page] == hlink->m_link[pp]) )
		{
			if (hlink->m_link[pp].fpage != 0xFFFF)
			{	// not mother
				LOG_DEBUG(_T("H: (%04X, %03X) => F: (%04X, %03X)"), 
					hlink->m_hblock, pp, hlink->m_link[pp].fblock, hlink->m_link[pp].fpage);
				fprintf_s(outfile, ("(%04X:%03X), (%04X:%03X)\n"), 
					hlink->m_hblock, pp, hlink->m_link[pp].fblock, hlink->m_link[pp].fpage);
			}
			m_table[start_page] = hlink->m_link[pp];
		}
	}
}


//void CL2PhTable::Difference(CSubLinkTable * sub_tab)
//{
//	JCASSERT(sub_tab);
//	JCSIZE index = sub_tab->m_start_hpage / HPAGE_PER_TABLE;
//
//	CSubLinkTable * ref_tab = m_table[index];
//	if (!ref_tab) return;
//	for (int pp = 0; pp < HPAGE_PER_TABLE; ++pp)
//	{
//		if ( !(sub_tab->m_sub_table[pp] == ref_tab->m_sub_table[pp]) )
//		{
//			stdext::jc_printf(_T("ghpage: %06X -> fblock: %04X, fpage: %03X"), pp + sub_tab->m_start_hpage, 
//				sub_tab->m_sub_table[pp].fblock, sub_tab->m_sub_table[pp].fpage);
//		}
//	}
//
//}
//
//void CL2PhTable::SetSubTable(CSubLinkTable * sub_tab)
//{
//	JCASSERT(sub_tab);
//	JCSIZE index = sub_tab->m_start_hpage / HPAGE_PER_TABLE;
//	if (m_table[index]) delete m_table[index];
//	m_table[index] = sub_tab;
//}




///////////////////////////////////////////////////////////////////////////////
//--

#define MAX_LINE_BUF	1024

void CTableDecodeApp::AnalyseCacheInfo(FILE * src_file)
{
	JCSIZE line_no = 0;
	stdext::auto_array<char> _line_buf(MAX_LINE_BUF);
	char * line_buf = _line_buf;
 
	// skip line
	fgets(line_buf, MAX_LINE_BUF, src_file);	// Skip header

	// loop 
	CHblockLink hlink(PAGE_PER_BLOCK);
	while (1)
	{
		// read one page 
		if ( !fgets(line_buf, MAX_LINE_BUF, src_file) ) break;
		JCSIZE offset = 0, secs = 0;
		sscanf_s(line_buf + 114, "%X;%X", &offset, &secs);
		int fblock, fpage;
		sscanf_s(line_buf + 1, ("%X"), &fblock);
		sscanf_s(line_buf + 8,("%X"), &fpage);

		m_table_per_page = secs / (TABLE_SIZE / SECTOR_SIZE);
		int hblock_per_table = HPAGE_PER_TABLE / PAGE_PER_BLOCK;

		stdext::auto_interface<jcparam::IBinaryBuffer> buf;
		CreateFileMappingBuf(m_file_mapping, offset, secs, buf);
		BYTE * _data = buf->Lock();
		BYTE * data = _data;

		LOG_DEBUG(_T("=== %04X:%03X,<%08X>"), fblock, fpage, offset);
		// output cache-info f-block and f-page
		fprintf_s(m_dst_file, ("===, %04X:%03X,<%08X>,"), fblock, fpage, offset);
		// output valid h-block
		JCSIZE tt = 0;
		char str_hblock[128];
		memset(str_hblock, '-', 128);
		str_hblock[m_table_per_page * hblock_per_table] = 0;
		WORD hblock = 0xFFFF, hblock_no = 0;
		for (tt = 0, data=_data; tt < m_table_per_page; ++tt, data+= TABLE_SIZE)
		{
			BYTE * hblock_info = data + 0x900;
			for (JCSIZE hh = 0; hh < hblock_per_table; ++hh, hblock_no ++)
			{
				WORD bb = MAKEWORD(hblock_info[hh*2+1], hblock_info[hh*2]);
				if (bb != 0xFFFF)
				{
					hblock = bb - hblock_no;
					str_hblock[hblock_no] = '*';
				}
			}
		}

		fprintf_s(m_dst_file, ("%04X,%s\n"), hblock, str_hblock);

		// create L2PH table from data

		for (tt = 0, data=_data; tt < m_table_per_page; ++tt, data+= TABLE_SIZE)
		{
			BYTE * hblock_info = data + 0x900;
			//LOG_DEBUG(_T("hblock: %04X, %04X, %04X, %04X"), 
			//	MAKEWORD(hblock_info[1], hblock_info[0]),
			//	MAKEWORD(hblock_info[3], hblock_info[2]),
			//	MAKEWORD(hblock_info[5], hblock_info[4]),
			//	MAKEWORD(hblock_info[7], hblock_info[6])
			//);
			//continue;

			for (int hh = 0; hh < hblock_per_table; ++hh)
			{
				WORD hblock = MAKEWORD(hblock_info[hh * 2 + 1], hblock_info[hh * 2]);
				if (hblock == 0xFFFF) continue;
				
				WORD mother =  MAKEWORD(hblock_info[16 + 2*hh + 1], hblock_info[16 + 2*hh]);
				hlink.Reset();
				hlink.SetHblockInfo(hblock, mother);
				hlink.SetLinkTable(data, hh, m_cache_index, MAX_CACHE_NUM);

				m_table.Merge(&hlink, m_dst_file);
			}
		}
		buf->Unlock(_data);
		
		// output update
	// end loop
	}
}

void CTableDecodeApp::ReadCacheIndex(FILE * src_file, WORD * index_tab, JCSIZE max_index)
{
	JCSIZE line_no = 0;
	stdext::auto_array<char> _line_buf(MAX_LINE_BUF);
	char * line_buf = _line_buf;

	// skip lines
	fgets(line_buf, MAX_LINE_BUF, src_file);	// Skip header
	fgets(line_buf, MAX_LINE_BUF, src_file);	// Skip header
	line_no +=2;

	while (1)
	{
		line_no ++;
		if ( !fgets(line_buf, MAX_LINE_BUF, src_file) ) break;
		int fblock, index;
		sscanf_s(line_buf + 1, ("%X"), &fblock);
		sscanf_s(line_buf + 33,("%X"), &index);
		if (index >= MAX_CACHE_NUM)		
		{
			LOG_ERROR(_T("error: illeagle index = %X, (line=%d)"), index, line_no);
			JCASSERT(0);
			continue;
		}

		if (index_tab[index] != 0xFFFF)
		{
			LOG_WARNING(_T("warning: index %02X registed by f-block %04X, (line=%d)"), index, index_tab[index], line_no);
			JCASSERT(0);
		}
		index_tab[index] = (WORD)(fblock);
	}
}


///////////////////////////////////////////////////////////////////////////////
//--

CTableDecodeApp::CTableDecodeApp(void)
: m_file_mapping(NULL)
{
}


int CTableDecodeApp::Initialize(void)
{
	memset(m_cache_index, 0xFF, sizeof(WORD) * MAX_CACHE_NUM);
	return 1;
}

void CTableDecodeApp::CleanUp(void)
{
	if (m_file_mapping)	m_file_mapping->Release();
}

int CTableDecodeApp::Run(void)
{
	// open output file
	OpenOutputFile();
	JCASSERT(m_dst_file);

	// Load cache index
	FILE * file_cache_index = NULL;
	_tfopen_s(&file_cache_index, m_cache_index_fn.c_str(), _T("r"));
	if (!file_cache_index) THROW_ERROR(ERR_USER, _T("failure on openning file %s"), m_cache_index_fn.c_str() );
	ReadCacheIndex(file_cache_index, m_cache_index, MAX_CACHE_NUM);
	fclose(file_cache_index);

	// Load data file
	CreateFileMappingObject(m_data_fn, m_file_mapping);
	JCASSERT(m_file_mapping);

	// open dps file
	m_table.Initialize(4096, PAGE_PER_BLOCK);

	FILE * src_file = NULL;
	_tfopen_s(&src_file, m_dps_fn.c_str(), _T("r"));
	if (!src_file) THROW_ERROR(ERR_USER, _T("failure on openning file %s"), m_dps_fn.c_str() );
	// load cache info table from binary file
	// parse cache info table
	AnalyseCacheInfo(src_file);
	fclose(src_file);

	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	return jcapp::local_main(argc, argv);
}

