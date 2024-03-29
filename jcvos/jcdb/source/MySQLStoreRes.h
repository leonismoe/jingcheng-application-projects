#pragma once
#include "../include/dbinterface.h"
#include <mysql.h>
#include "../include/field_info.h"
//#include <atlcoll.h>


namespace jcdb
{

	class CMySQLClient;

	class CMySQLStoreRes 
		: virtual public IResultSet
		, public CJCInterfaceBase
	{
	protected:
		CMySQLStoreRes(void);
		CMySQLStoreRes(MYSQL_RES *res);
		virtual ~CMySQLStoreRes(void);

	friend class CMySQLClient;

	protected:
		MYSQL_RES * m_result;
		LONGLONG	m_iRows;
		//int			m_iFields;
		int			m_i_cur_row;		// 当前纪录的索引

		//CDBFieldInfo	* m_field_info;
		CDBFieldMap m_field_map;


	public:
		// 关于浏览结果集的方法
		virtual bool GetNextRow(IRecordRow * &);

		// 关于字段的方法

		// 返回字段的个数
		virtual int	GetFieldSize() const {	return m_field_map.GetFieldSize(); }

		virtual const CDBFieldInfo * GetField(int field)/* const*/;		// 根据编号取得字段
	
		virtual CDBFieldInfo * GetField(LPCTSTR strName);		// 根据名称取得字段

		virtual int GetRowCount(void) const;

	//protected:
	//	CDBFieldMap		m_field_map;
	};
};
