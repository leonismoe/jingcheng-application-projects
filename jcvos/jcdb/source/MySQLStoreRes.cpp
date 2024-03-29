//#include "StdAfx.h"
#include "MySQLStoreRes.h"
#include "dbexcep.h"
//#include "RecordRowImpl.h"
#include "../include/field_info.h"
#include "MySQLRow.h"

//CLASS_SIZE(CMySQLStoreRes);
LOCAL_LOGGER_ENABLE(_T("JcdbMySQL"), LOGGER_LEVEL_DEBUGINFO);

using namespace jcdb;

CMySQLStoreRes::CMySQLStoreRes(void)
	: /*CResultSetImpl()
	, */m_result(NULL)
	, m_i_cur_row(0)
{
}

CMySQLStoreRes::CMySQLStoreRes(MYSQL_RES *res)
	: /*CResultSetImpl()
	, */m_result(res)
	, m_i_cur_row(0)
{
	JCASSERT(m_result);
	m_iRows = (LONGLONG)mysql_num_rows(m_result);
	unsigned int field_size = mysql_num_fields(m_result);

	// Get field info
	m_field_map.SetFieldSize(field_size);
	MYSQL_FIELD *fields = mysql_fetch_fields(m_result);

	// check first field should be "id"
	if (strcmp(fields[0].name, "id") != 0) THROW_DB_ERR("Fisrt field is not 'id'");
	for (unsigned int ii = 0; ii < field_size; ii++)
	{
		m_field_map.PushField(ii, fields[ii].name, fields[ii].length);
	}
}

CMySQLStoreRes::~CMySQLStoreRes(void)
{
	if (m_result) mysql_free_result(m_result);
}

	//virtual IRecordRow * GetCurrentRow() = 0;
bool CMySQLStoreRes::GetNextRow(IRecordRow * & prow)
{
	if (!m_result) THROW_DB_ERR("Invalid result");
	if (m_i_cur_row < 0) return false;

	MYSQL_ROW row = mysql_fetch_row(m_result);
	if (!row)
	{
		// 达到结果集的末尾
		m_i_cur_row = -1;
		return false;
	}

	DWORD * lengths = mysql_fetch_lengths(m_result); 
	CMySQLRow * rec = new CMySQLRow(&m_field_map, row, lengths/*, m_iFields*/);
	m_i_cur_row ++;

	prow = static_cast<IRecordRow*>(rec);
	return true;
}

	//virtual IRecordRow * GetPrevRow() = 0;
	//virtual void GotoRow(int i) = 0;

const CDBFieldInfo * CMySQLStoreRes::GetField(int field_num)/* const*/
{
	//const CDBFieldInfo * field_info = NULL;
	//if ( (field_num >= m_field_map.GetFieldSize() ) || 
	//	(field_info = m_field_map.GetValueAt(field_num)) == NULL )
	//{
	//	// 从mysql的result取得字段信息
	//	JCASSERT(m_result);		JCASSERT(field_num < m_iFields);
	//	MYSQL_FIELD * field = mysql_fetch_field_direct(m_result, field_num);
	//	if (field)
	//	{
	//		field_info = new CDBFieldInfo(CA2T(field->name), field->max_length);
	//		//if (!field) return NULL;
	//		m_field_map.InsertField(field_num, field_info);
	//	}
	//}
	//return field_info;
	return m_field_map.GetFieldInfo(field_num);
}

CDBFieldInfo * CMySQLStoreRes::GetField(LPCTSTR strName)		// 根据名称取得字段
{
	return NULL;
}

int CMySQLStoreRes::GetRowCount(void) const
{
	JCASSERT(m_result);
	return (int)(mysql_num_rows(m_result));
}
