﻿#pragma once

#include <jcparam.h>
#include "SM2242.h"

class CCmdBaseLT2244: public CSmiCommand
{
public:
	CCmdBaseLT2244(JCSIZE count) : CSmiCommand(count) {};
	inline void block(WORD b) 
	{
		m_cmd.b[2] = HIBYTE(b);
		m_cmd.b[3] = LOBYTE(b);
	}
	inline WORD block() { return MAKEWORD(m_cmd.b[3], m_cmd.b[2]); }
	inline void page(WORD p)
	{
		m_cmd.b[4] = HIBYTE(p);
		m_cmd.b[5] = LOBYTE(p);
	}
	inline WORD page() { return MAKEWORD(m_cmd.b[5], m_cmd.b[4]); }

	inline BYTE & chunk() { return m_cmd.b[6];}
	inline BYTE & mode()		{ return m_cmd.b[8]; }
};

//extern const CSmartAttrDefTab	m_smart_neci;

class CLT2244 : public CSM2242
{
public:
	static bool CreateDevice(IStorageDevice * storage, ISmiDevice *& smi_device);
	static bool Recognize(IStorageDevice * storage, BYTE * inquery);
	static bool LocalRecognize(BYTE * data)
	{
		char * ic_ver = (char*)(data + 0x2E);	
		return (NULL != strstr(ic_ver, "SM2244LT") );
	}

public:
	virtual void ReadSmartFromWpro(BYTE * data);
	virtual JCSIZE GetSystemBlockId(JCSIZE id);

protected:
	CLT2244(IStorageDevice * dev);
	virtual ~CLT2244(void) {}

	virtual bool CheckVenderCommand(BYTE * data)	{return CLT2244::LocalRecognize(data); }
	virtual void FlashAddToPhysicalAdd(const CFlashAddress & add, CSmiCommand & cmd, UINT option);
	virtual const CSmartAttrDefTab * GetSmartAttrDefTab(LPCTSTR rev) const 
	{
		if ( rev && _tcscmp(rev, _T("NECI"))==0 )	return &m_smart_neci;
		else return &m_smart_def_2244lt;
	}
	virtual void GetSpare(CSpareData & spare, BYTE* spare_buf);

	virtual bool GetProperty(LPCTSTR prop_name, UINT & val);

	// Read SRAM in 512 bytes;
	virtual void ReadSRAM(WORD ram_add, WORD bank, BYTE * buf);

// Implement CSmiDeviceComm Interface
protected:
	virtual LPCTSTR Name(void) const {return _T("LT2244");}
	virtual bool Initialize(void);

protected:
	static const CSmartAttrDefTab	m_smart_def_2244lt;
	static const CSmartAttrDefTab	m_smart_neci;
	BYTE m_info_page, m_bitmap_page, m_orphan_page, m_blockindex_page;
	WORD m_org_bad_info;
	WORD m_h_block_num;
};	

